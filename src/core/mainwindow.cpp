#include "mainwindow.h"
#include "motionmodels.h"
#include "grapheditorview.h"
#include "graphnodeitem.h" // 1번: GraphNodeItem
#include "commands.h" // 1번: MoveNodeCommand

#include <QMenu>
#include <QMenuBar>
#include <QDockWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QPushButton>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QSpinBox> // Export
#include <QRadioButton> // Export
#include <QDialogButtonBox> // Export
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QUndoStack> // Undo 스택
#include <QDebug> // qrand
#include <QTime> // qsrand
#include <QJsonArray> // 4번
#include <QJsonObject> // 4번
#include <QJsonDocument> // 4번
#include <QGroupBox> // 1번
// 3번: YAML 헤더
#include <yaml-cpp/yaml.h>
#include <fstream> // 파일 쓰기

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    m_undoStack = new QUndoStack(this);
    m_document = new MotionDocument(this);
    m_view = new GraphEditorView(this);
    m_view->setDocument(m_document);
    m_view->setUndoStack(m_undoStack); 
    setCentralWidget(m_view); 

    createActions();
    createMenus();
    createDocks(); 
    
    connect(m_motorTreeWidget, &QTreeWidget::currentItemChanged, 
            this, &MainWindow::onMotorSelectionChanged);
    connect(m_document, &MotionDocument::modelChanged, 
            this, &MainWindow::onDocumentModelChanged);
    connect(m_document, &MotionDocument::activeMotorChanged, 
            this, &MainWindow::onActiveMotorSwitched);
    connect(m_fitToViewAction, &QAction::triggered, m_view, &GraphEditorView::fitToView);
    connect(m_snapGridAction, &QAction::toggled, m_view, &GraphEditorView::toggleSnapToGrid);
    connect(m_view, &GraphEditorView::nodeSelectionChanged, this, &MainWindow::onNodeSelected);
            
    qsrand(QTime::currentTime().msec()); 
    MotorProfile* m1 = m_document->addMotor("Motor 1 (Red)", Qt::red);
    m1->internalAddNode(QPointF(0, 0));       // X=0ms
    m1->internalAddNode(QPointF(2000, 50));   // X=2000ms
    MotorProfile* m2 = m_document->addMotor("Motor 4 (Blue)", Qt::blue);
    m2->internalAddNode(QPointF(500, -30));   // X=500ms
    m2->internalAddNode(QPointF(1500, 80));  // X=1500ms

    onDocumentModelChanged(); 
    
    if (m_motorTreeWidget->topLevelItemCount() > 0) {
        m_motorTreeWidget->setCurrentItem(m_motorTreeWidget->topLevelItem(0));
    }

    m_undoStack->clear(); 
    setMinimumSize(800, 600);
    setWindowTitle("Profile Orchestrator (Qt 5)"); 
}

MainWindow::~MainWindow() {}

void MainWindow::createActions() {
    m_saveAction = new QAction("저장 (&S)", this);
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::onSaveDocument);

    m_loadAction = new QAction("불러오기 (&O)", this);
    m_loadAction->setShortcut(QKeySequence::Open);
    connect(m_loadAction, &QAction::triggered, this, &MainWindow::onLoadDocument);

    m_exportAction = new QAction("내보내기 (&E)...", this);
    connect(m_exportAction, &QAction::triggered, this, &MainWindow::onExportDocument);

    m_undoAction = m_undoStack->createUndoAction(this, "실행 취소 (&U)");
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_redoAction = m_undoStack->createRedoAction(this, "다시 실행 (&R)");
    m_redoAction->setShortcut(QKeySequence::Redo);

    m_fitToViewAction = new QAction("전체 보기 (&F)", this);
    m_fitToViewAction->setShortcut(Qt::Key_F);
    
    m_snapGridAction = new QAction("그리드에 맞추기 (&G)", this);
    m_snapGridAction->setCheckable(true);
    m_snapGridAction->setShortcut(Qt::Key_G);
}

void MainWindow::createMenus() {
    QMenu* fileMenu = menuBar()->addMenu("파일 (&F)");
    fileMenu->addAction(m_loadAction);
    fileMenu->addAction(m_saveAction);
    fileMenu->addAction(m_exportAction);
    
    QMenu* editMenu = menuBar()->addMenu("편집 (&E)");
    editMenu->addAction(m_undoAction);
    editMenu->addAction(m_redoAction);
    editMenu->addSeparator();
    editMenu->addAction(m_snapGridAction);
    
    QMenu* viewMenu = menuBar()->addMenu("보기 (&V)");
    viewMenu->addAction(m_fitToViewAction);
}

void MainWindow::createDocks() {
    // 좌측 독
    QDockWidget* leftDock = new QDockWidget("Object", this);
    QWidget* motorListWidget = new QWidget; 
    QVBoxLayout* motorLayout = new QVBoxLayout(motorListWidget);
    motorLayout->setContentsMargins(0,0,0,0);
    motorLayout->setSpacing(0);
    m_motorTreeWidget = new QTreeWidget;
    m_motorTreeWidget->setHeaderHidden(true); 
    // m_motorsRootItem 제거
    motorLayout->addWidget(m_motorTreeWidget); 
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    m_addMotorButton = new QToolButton();
    m_addMotorButton->setText("+");
    m_removeMotorButton = new QToolButton();
    m_removeMotorButton->setText("-");
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_addMotorButton);
    buttonLayout->addWidget(m_removeMotorButton);
    motorLayout->addLayout(buttonLayout); 
    leftDock->setWidget(motorListWidget); 
    addDockWidget(Qt::LeftDockWidgetArea, leftDock);
    connect(m_addMotorButton, &QToolButton::clicked, this, &MainWindow::onAddMotor);
    connect(m_removeMotorButton, &QToolButton::clicked, this, &MainWindow::onRemoveMotor);

    // 우측 독
    QDockWidget* rightDock = new QDockWidget("속성", this);
    QWidget* constraintsWidget = new QWidget;
    QVBoxLayout* rightLayout = new QVBoxLayout(constraintsWidget);
    
    // 제약 조건 그룹
    QGroupBox* constraintsGroup = new QGroupBox("모터 제약 조건");
    QFormLayout* formLayout = new QFormLayout; 
    m_yMinSpin = new QDoubleSpinBox;
    m_yMinSpin->setRange(-100000, 100000); 
    formLayout->addRow("Y 최소값:", m_yMinSpin);
    m_yMaxSpin = new QDoubleSpinBox;
    m_yMaxSpin->setRange(-100000, 100000); 
    formLayout->addRow("Y 최대값:", m_yMaxSpin);
    m_slopeSpin = new QDoubleSpinBox;
    m_slopeSpin->setRange(0, 100000);
    m_slopeSpin->setDecimals(3); 
    m_slopeSpin->setValue(1.0); // 기본값 (1 unit/ms)
    formLayout->addRow("최대 기울기(unit/ms):", m_slopeSpin); // 단위 표시
    m_applyConstraintsButton = new QPushButton("제약조건 적용");
    formLayout->addWidget(m_applyConstraintsButton);
    constraintsGroup->setLayout(formLayout);
    rightLayout->addWidget(constraintsGroup); 

    // 노드 좌표 편집 그룹
    m_nodeEditGroup = new QGroupBox("선택된 노드 좌표");
    QFormLayout* nodeEditLayout = new QFormLayout;
    m_nodeXSpin = new QDoubleSpinBox;
    m_nodeXSpin->setRange(0.0, 1000000.0); // X(시간, ms) 범위 증가
    m_nodeXSpin->setDecimals(0); // ms는 정수
    m_nodeXSpin->setSuffix(" ms"); // 단위 표시
    m_nodeYSpin = new QDoubleSpinBox; 
    m_nodeYSpin->setRange(-100000.0, 100000.0); 
    m_nodeYSpin->setDecimals(3);
    m_applyNodeCoordsButton = new QPushButton("좌표 적용");
    nodeEditLayout->addRow("Time:", m_nodeXSpin);
    nodeEditLayout->addRow("Value:", m_nodeYSpin);
    nodeEditLayout->addWidget(m_applyNodeCoordsButton);
    m_nodeEditGroup->setLayout(nodeEditLayout);
    rightLayout->addWidget(m_nodeEditGroup);
    m_nodeEditGroup->setEnabled(false); 
    
    rightLayout->addStretch(); 
    
    rightDock->setWidget(constraintsWidget);
    addDockWidget(Qt::RightDockWidgetArea, rightDock);

    connect(m_applyConstraintsButton, &QPushButton::clicked, this, &MainWindow::onApplyConstraints);
    connect(m_applyNodeCoordsButton, &QPushButton::clicked, this, &MainWindow::onApplyNodeCoords); 
}

// 2번: 트리 구조 변경 반영
void MainWindow::onMotorSelectionChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous) {
    Q_UNUSED(previous); 
    // 루트 아이템 체크 제거
    if (!current) { 
        m_document->setActiveMotor(nullptr);
        return;
    }
    MotorProfile* profile = current->data(0, Qt::UserRole).value<MotorProfile*>();
    m_document->setActiveMotor(profile);
}

void MainWindow::onAddMotor() {
    bool ok;
    QString name = QInputDialog::getText(this, "새 모터", "모터 이름:", QLineEdit::Normal, "New Motor", &ok);
    if (ok && !name.isEmpty()) {
        QColor color = QColor::fromHsv(qrand() % 360, 200, 200);
        m_document->addMotor(name, color);
    }
}

// 2번: 트리 구조 변경 반영
void MainWindow::onRemoveMotor() {
    QTreeWidgetItem* currentItem = m_motorTreeWidget->currentItem();
    // 루트 아이템 체크 제거
    if (!currentItem) return; 
    
    MotorProfile* profile = currentItem->data(0, Qt::UserRole).value<MotorProfile*>();
    if (!profile) return;
    auto reply = QMessageBox::question(this, "모터 삭제",
        QString("'%1' 모터를 정말 삭제하시겠습니까?").arg(profile->name()),
        QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        m_document->removeMotor(profile);
    }
}

void MainWindow::onSaveDocument() {
    QString fileName = QFileDialog::getSaveFileName(this, "프로파일 저장", "", "모션 JSON 파일 (*.json)");
    if (fileName.isEmpty()) return;
    if (!m_document->saveToFile(fileName)) {
        QMessageBox::warning(this, "저장 실패", "파일을 저장하는 데 실패했습니다.");
    }
}

void MainWindow::onLoadDocument() {
    QString fileName = QFileDialog::getOpenFileName(this, "프로파일 불러오기", "", "모션 JSON 파일 (*.json)");
    if (fileName.isEmpty()) return;
    if (!m_document->loadFromFile(fileName)) {
        QMessageBox::warning(this, "불러오기 실패", "파일을 불러오는 데 실패했습니다.");
    }
}

void MainWindow::onFitToView() {
    m_view->fitToView();
}

void MainWindow::onApplyConstraints() {
    MotorProfile* profile = m_document->activeProfile();
    if (profile) {
        profile->checkAllNodes();
    }
}

// 2번: 트리 구조 변경 반영
void MainWindow::onDocumentModelChanged() {
    m_motorTreeWidget->blockSignals(true); 
    m_motorTreeWidget->clear(); // 트리 전체 클리어

    QTreeWidgetItem* activeItem = nullptr;
    MotorProfile* activeProfile = m_document->activeProfile();

    for (MotorProfile* profile : m_document->motorProfiles()) {
        // 루트 아이템 대신 트리 위젯에 바로 추가
        QTreeWidgetItem* item = new QTreeWidgetItem(m_motorTreeWidget); 
        item->setText(0, profile->name());
        item->setForeground(0, profile->color());
        item->setData(0, Qt::UserRole, QVariant::fromValue(profile));
        if (profile == activeProfile) {
            activeItem = item;
        }
    }

    if (activeItem) {
        m_motorTreeWidget->setCurrentItem(activeItem);
    } else if (m_motorTreeWidget->topLevelItemCount() > 0) { 
        m_motorTreeWidget->setCurrentItem(m_motorTreeWidget->topLevelItem(0));
    }

    m_motorTreeWidget->blockSignals(false);
    
    onActiveMotorSwitched(m_document->activeProfile(), nullptr); 
    onNodeSelected(nullptr); 
    m_undoStack->clear(); 
}

// 1번: 뷰 변경(fit) 코드 주석 처리 유지
void MainWindow::onActiveMotorSwitched(MotorProfile* active, MotorProfile* previous) {
    if (previous) {
        disconnectProfileFromSpinBoxes(previous);
    }
    if (active) {
        connectProfileToSpinBoxes(active);

        m_motorTreeWidget->blockSignals(true);
        // 루트 대신 topLevelItem 사용
        for(int i=0; i < m_motorTreeWidget->topLevelItemCount(); ++i) { 
            QTreeWidgetItem* item = m_motorTreeWidget->topLevelItem(i);
            MotorProfile* profile = item->data(0, Qt::UserRole).value<MotorProfile*>();
            if(profile == active) {
                m_motorTreeWidget->setCurrentItem(item);
                break;
            }
        }
        m_motorTreeWidget->blockSignals(false);
        
        // 1번: 시점 변경 코드 주석 유지
        // m_view->fitToActiveMotor(active);
        
    } else {
        // 1번: 시점 변경 코드 주석 유지
        // m_view->fitToActiveMotor(nullptr);
    }
    
    onNodeSelected(nullptr); 
    m_undoStack->clear(); 
}

void MainWindow::connectProfileToSpinBoxes(MotorProfile* profile) {
    if (!profile) return;
    
    m_yMinSpin->setValue(profile->yMin());
    m_yMaxSpin->setValue(profile->yMax());
    m_slopeSpin->setValue(profile->maxSlope());

#if QT_VERSION < QT_VERSION_CHECK(5, 7, 0)
    connect(m_yMinSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setYMin);
    connect(m_yMaxSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setYMax);
    connect(m_slopeSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setMaxSlope);
#else
    connect(m_yMinSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setYMin);
    connect(m_yMaxSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setYMax);
    connect(m_slopeSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setMaxSlope);
#endif
}

void MainWindow::disconnectProfileFromSpinBoxes(MotorProfile* profile) {
     if (!profile) return;
#if QT_VERSION < QT_VERSION_CHECK(5, 7, 0)
     disconnect(m_yMinSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setYMin);
     disconnect(m_yMaxSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setYMax);
     disconnect(m_slopeSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setMaxSlope);
#else
     disconnect(m_yMinSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setYMin);
     disconnect(m_yMaxSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setYMax);
     disconnect(m_slopeSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setMaxSlope);
#endif
}


// 5번: Export 로직 (YAML 추가, 시간 단위 ms)
void MainWindow::onExportDocument() {
    if (!m_document) return; 

    // 1. 다이얼로그 생성
    QDialog dialog(this);
    dialog.setWindowTitle("프로파일 내보내기");
    QVBoxLayout layout(&dialog);
    QGroupBox* formatGroup = new QGroupBox("포맷 선택");
    QVBoxLayout* formatLayout = new QVBoxLayout;
    QRadioButton* jsonRadio = new QRadioButton("JSON");
    QRadioButton* yamlRadio = new QRadioButton("YAML");
    jsonRadio->setChecked(true);
    formatLayout->addWidget(jsonRadio);
    formatLayout->addWidget(yamlRadio);
    formatGroup->setLayout(formatLayout);
    layout.addWidget(formatGroup);
    QGroupBox* contentGroup = new QGroupBox("내용 선택");
    QVBoxLayout* contentLayout = new QVBoxLayout;
    QRadioButton* nodesOnlyRadio = new QRadioButton("활성 모터 노드만");
    QRadioButton* sampledRadio = new QRadioButton("모든 모터 샘플링"); // 5번
    QRadioButton* allNodesRadio = new QRadioButton("모든 모터 노드 (Save와 동일)");
    nodesOnlyRadio->setChecked(true);
    contentLayout->addWidget(nodesOnlyRadio);
    contentLayout->addWidget(sampledRadio);
    contentLayout->addWidget(allNodesRadio);
    contentGroup->setLayout(contentLayout);
    layout.addWidget(contentGroup);
    QWidget* optionsWidget = new QWidget;
    QFormLayout* optionsLayout = new QFormLayout(optionsWidget);
    QDoubleSpinBox* endTimeSpin = new QDoubleSpinBox;
    endTimeSpin->setRange(0.0, 1000000.0); // ms 단위
    endTimeSpin->setDecimals(0);
    endTimeSpin->setValue(2000.0); // 기본 2000ms
    double maxTime = 0.0;
    for (MotorProfile* p : m_document->motorProfiles()) {
        if (!p->nodes().isEmpty() && p->nodes().last().x() > maxTime) {
            maxTime = p->nodes().last().x();
        }
    }
    if (maxTime < 2000.0) maxTime = 2000.0;
    endTimeSpin->setValue(maxTime);
    endTimeSpin->setSuffix(" ms"); // 단위 변경
    QSpinBox* hzSpin = new QSpinBox;
    hzSpin->setRange(1, 10000);
    hzSpin->setValue(100);
    hzSpin->setSuffix(" Hz");
    optionsLayout->addRow("종료 시간:", endTimeSpin);
    optionsLayout->addRow("샘플링 속도:", hzSpin);
    optionsWidget->setVisible(false);
    layout.addWidget(optionsWidget);
    connect(sampledRadio, &QRadioButton::toggled, optionsWidget, &QWidget::setVisible);
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout.addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    // 2. 다이얼로그 실행
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    // 3. 파일 경로
    bool isYaml = yamlRadio->isChecked();
    QString filter = isYaml ? "YAML 파일 (*.yaml *.yml)" : "JSON 파일 (*.json)";
    QString fileName = QFileDialog::getSaveFileName(this, "내보내기", "", filter);
    if (fileName.isEmpty()) return;

    // 4. 내보내기 로직
    bool exportOk = false;
    if (isYaml) {
        // YAML 내보내기
        YAML::Emitter emitter;
        bool sample = sampledRadio->isChecked();
        bool exportResult = m_document->exportDocumentToYAML(emitter, sample, hzSpin->value(), endTimeSpin->value());

        if (exportResult && emitter.good()) {
            std::ofstream fout(fileName.toStdString());
            if (fout.is_open()) {
                fout << emitter.c_str();
                exportOk = true;
            } else {
                 QMessageBox::warning(this, "파일 오류", "파일을 열 수 없습니다.");
            }
        } else {
             QMessageBox::warning(this, "YAML 오류", "YAML 데이터를 생성하는 데 실패했습니다.");
        }
    } else {
        // JSON 내보내기
        QFile file(fileName);
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox::warning(this, "파일 오류", "파일을 열 수 없습니다.");
        } else {
            QJsonObject rootObj;
            QJsonArray motorsArray;
            MotorProfile* activeProfile = m_document->activeProfile();

            if (allNodesRadio->isChecked()) {
                for (MotorProfile* profile : m_document->motorProfiles()) {
                    QJsonObject motorObj;
                    profile->write(motorObj);
                    motorsArray.append(motorObj);
                }
            } else if (nodesOnlyRadio->isChecked()) {
                if (!activeProfile) {
                     QMessageBox::warning(this, "내보내기 오류", "활성 모터가 없습니다.");
                     file.close(); 
                     return;
                }
                QJsonObject motorObj;
                activeProfile->write(motorObj);
                motorsArray.append(motorObj);
            } else { // 5번: 샘플링 시 모든 모터
                for (MotorProfile* profile : m_document->motorProfiles()) {
                     QJsonObject motorObj;
                     profile->exportSamplesToJSON(motorObj, hzSpin->value(), endTimeSpin->value());
                     motorsArray.append(motorObj);
                }
            }
            rootObj["motors"] = motorsArray;
            file.write(QJsonDocument(rootObj).toJson());
            file.close();
            exportOk = true;
        }
    }

    if (exportOk) {
        statusBar()->showMessage("내보내기 완료.", 2000);
    }
}


// 노드 선택 시 호출되는 슬롯
void MainWindow::onNodeSelected(QGraphicsItem* selectedNode)
{
    m_selectedNode = qgraphicsitem_cast<GraphNodeItem*>(selectedNode);
    if (m_selectedNode) {
        m_nodeEditGroup->setEnabled(true);
        m_nodeXSpin->blockSignals(true);
        m_nodeYSpin->blockSignals(true);
        MotorProfile* profile = m_selectedNode->profile();
        MotionNode realNode = profile->nodeAt(m_selectedNode->index()); 
        m_nodeXSpin->setValue(realNode.x()); // ms 단위
        m_nodeYSpin->setRange(profile->yMin(), profile->yMax());
        m_nodeYSpin->setValue(realNode.y()); 
        m_nodeXSpin->blockSignals(false);
        m_nodeYSpin->blockSignals(false);
    } else {
        m_nodeEditGroup->setEnabled(false);
        m_nodeXSpin->setValue(0);
        m_nodeYSpin->setValue(0);
        m_selectedNode = nullptr; 
    }
}

// "좌표 적용" 버튼 클릭 시
void MainWindow::onApplyNodeCoords()
{
    if (!m_selectedNode || !m_undoStack) return;
    QPointF oldRealPos = m_selectedNode->profile()->nodeAt(m_selectedNode->index());
    QPointF newRealPos(m_nodeXSpin->value(), m_nodeYSpin->value()); // ms 단위
    if (oldRealPos == newRealPos) return; 
    m_undoStack->push(new MoveNodeCommand(m_selectedNode->profile(), 
                                         m_selectedNode->index(), 
                                         oldRealPos, 
                                         newRealPos));
}