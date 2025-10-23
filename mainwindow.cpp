#include "mainwindow.h"
#include "motionmodels.h"
#include "grapheditorview.h"

#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QDockWidget>
#include <QTreeWidget>      // QTreeWidget 헤더
#include <QTreeWidgetItem>  // QTreeWidgetItem 헤더
#include <QHeaderView>      // QTreeWidget 헤더 숨기기용
#include <QVBoxLayout>
#include <QFormLayout>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QStatusBar>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    // 1. 모델(데이터) 생성
    m_document = new MotionDocument(this);

    // 1. 뷰(그래프) 생성 및 설정
    m_view = new GraphEditorView(this);
    m_view->setDocument(m_document);
    setCentralWidget(m_view); 

    createActions();
    createMenus();
    createDocks(); // 7, 8. 독 위젯 생성

    // 8. QTreeWidget의 선택 변경 시그널 연결
    connect(m_motorTreeWidget, &QTreeWidget::currentItemChanged, 
            this, QOverload<QTreeWidgetItem*>::of(&MainWindow::onMotorSelectionChanged));
    
    // 문서 신호 연결 (버그 수정: onActiveMotorSwitched가 인자 2개 받도록)
    connect(m_document, &MotionDocument::modelChanged, 
            this, &MainWindow::onDocumentModelChanged);
    connect(m_document, &MotionDocument::activeMotorChanged, 
            this, &MainWindow::onActiveMotorSwitched);
            
    // 초기 모터 추가 (테스트용)
    MotorProfile* m1 = m_document->addMotor("Motor 1 (Step)", Qt::red);
    m1->addNode(QPointF(0, 0));
    m1->addNode(QPointF(200, 50));
    MotorProfile* m2 = m_document->addMotor("Motor 4 (Step)", Qt::blue);
    m2->addNode(QPointF(50, -30));
    m2->addNode(QPointF(150, 80));

    // 12. 모델이 변경되었으니 UI(트리) 갱신
    onDocumentModelChanged();
    
    // 8. 첫 번째 모터를 활성 모터로 선택
    if (m_motorsRootItem->childCount() > 0) {
        m_motorsRootItem->child(0)->setSelected(true);
        // onMotorSelectionChanged가 호출되며 m_document->activeMotor가 설정됨
    }

    setMinimumSize(800, 600);
    setWindowTitle("Profile Orchestrator (Qt 5)"); // 타이틀 설정
}

MainWindow::~MainWindow() {
    // m_document는 this의 자식이므로 자동 삭제됨
}

void MainWindow::createActions() {
    m_saveAction = new QAction("저장 (&S)", this);
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::onSaveDocument);

    m_loadAction = new QAction("불러오기 (&O)", this);
    m_loadAction->setShortcut(QKeySequence::Open);
    connect(m_loadAction, &QAction::triggered, this, &MainWindow::onLoadDocument);
}

void MainWindow::createMenus() {
    QMenu* fileMenu = menuBar()->addMenu("파일 (&F)");
    fileMenu->addAction(m_loadAction);
    fileMenu->addAction(m_saveAction);
}

// 7, 8. 좌/우측 독 위젯 생성
void MainWindow::createDocks() {
    // 8. 좌측 모터 선택창 (QTreeWidget으로 변경, 이미지 반영)
    QDockWidget* leftDock = new QDockWidget("Object", this); // 이미지처럼 "Object"
    m_motorTreeWidget = new QTreeWidget;
    m_motorTreeWidget->setHeaderHidden(true); // 이미지처럼 헤더 숨김
    
    // "Motors" 루트 아이템 추가
    m_motorsRootItem = new QTreeWidgetItem(m_motorTreeWidget, {"Motors"});
    m_motorsRootItem->setFlags(m_motorsRootItem->flags() & ~Qt::ItemIsSelectable); // 루트는 선택 안 되게
    m_motorsRootItem->setExpanded(true); // 항상 펼쳐진 상태
    
    leftDock->setWidget(m_motorTreeWidget);
    addDockWidget(Qt::LeftDockWidgetArea, leftDock);

    // 7. 우측 제약 조건 메뉴
    QDockWidget* rightDock = new QDockWidget("제약 조건 (활성 모터)", this);
    QWidget* constraintsWidget = new QWidget;
    QFormLayout* layout = new QFormLayout(constraintsWidget);

    m_yMinSpin = new QDoubleSpinBox;
    m_yMinSpin->setRange(-10000, 10000);
    layout->addRow("Y 최소값:", m_yMinSpin);

    m_yMaxSpin = new QDoubleSpinBox;
    m_yMaxSpin->setRange(-10000, 10000);
    layout->addRow("Y 최대값:", m_yMaxSpin);
    
    m_slopeSpin = new QDoubleSpinBox;
    m_slopeSpin->setRange(0, 100000);
    m_slopeSpin->setValue(1000.0); 
    layout->addRow("최대 기울기:", m_slopeSpin);
    
    rightDock->setWidget(constraintsWidget);
    addDockWidget(Qt::RightDockWidgetArea, rightDock);
}

// 8. 모터 트리에서 선택이 변경될 때
void MainWindow::onMotorSelectionChanged(QTreeWidgetItem* current) {
    if (!current || current == m_motorsRootItem) {
        m_document->setActiveMotor(nullptr); // 아무것도 선택 안 함
        return;
    }
    
    // 아이템의 UserRole에 저장된 MotorProfile 포인터 가져오기
    MotorProfile* profile = current->data(0, Qt::UserRole).value<MotorProfile*>();
    m_document->setActiveMotor(profile);
}

// 11. 저장
void MainWindow::onSaveDocument() {
    QString fileName = QFileDialog::getSaveFileName(this, "프로파일 저장", "", "모션 JSON 파일 (*.json)");
    if (fileName.isEmpty()) return;
    if (!m_document->saveToFile(fileName)) {
        QMessageBox::warning(this, "저장 실패", "파일을 저장하는 데 실패했습니다.");
    }
}

// 12. 불러오기
void MainWindow::onLoadDocument() {
    QString fileName = QFileDialog::getOpenFileName(this, "프로파일 불러오기", "", "모션 JSON 파일 (*.json)");
    if (fileName.isEmpty()) return;
    if (!m_document->loadFromFile(fileName)) {
        QMessageBox::warning(this, "불러오기 실패", "파일을 불러오는 데 실패했습니다.");
    }
    // loadFromFile이 modelChanged를 호출하므로 onDocumentModelChanged가 실행됨
}

// 12. 불러오기 등으로 모델이 완전히 변경되었을 때 (모터 트리 갱신)
void MainWindow::onDocumentModelChanged() {
    m_motorTreeWidget->blockSignals(true); // 신호 중복 방지
    m_motorsRootItem->takeChildren(); // 루트 하위의 기존 아이템 모두 삭제
    
    QTreeWidgetItem* activeItem = nullptr;
    MotorProfile* activeProfile = m_document->activeProfile();

    // 8. QTreeWidget에 모터 아이템 다시 채우기
    for (MotorProfile* profile : m_document->motorProfiles()) {
        QTreeWidgetItem* item = new QTreeWidgetItem(m_motorsRootItem);
        item->setText(0, profile->name());
        item->setForeground(0, profile->color());
        // 8. 아이템에 MotorProfile 포인터 저장
        item->setData(0, Qt::UserRole, QVariant::fromValue(profile));

        if (profile == activeProfile) {
            activeItem = item;
        }
    }
    
    // 8. 활성 모터 다시 선택
    if (activeItem) {
        m_motorTreeWidget->setCurrentItem(activeItem);
    } else if (m_motorsRootItem->childCount() > 0) {
        // 활성 모터가 없으면 첫 번째 모터를 선택
        m_motorTreeWidget->setCurrentItem(m_motorsRootItem->child(0));
    }
    
    m_motorTreeWidget->blockSignals(false);
    
    // 8. 현재 선택된 아이템으로 강제 스위칭 (스핀박스 연결 등)
    onActiveMotorSwitched(m_document->activeProfile(), nullptr);
}

// 8. 활성 모터가 변경될 때 (제약조건 스핀박스 연결)
// (Qt 6 코드 버그 수정: 인자 2개 받도록)
void MainWindow::onActiveMotorSwitched(MotorProfile* active, MotorProfile* previous) {
    // 7. 이전 모터가 있다면 스핀박스와의 연결 해제
    if (previous) {
        disconnectProfileFromSpinBoxes(previous);
    }

    // 7. 새 활성 모터가 있다면 스핀박스와 연결
    if (active) {
        connectProfileToSpinBoxes(active);

        // 8. (동기화) 모델 변경으로 활성 모터가 바뀌었을 때, 트리 위젯의 선택도 변경
        m_motorTreeWidget->blockSignals(true);
        for(int i=0; i < m_motorsRootItem->childCount(); ++i) {
            QTreeWidgetItem* item = m_motorsRootItem->child(i);
            MotorProfile* profile = item->data(0, Qt::UserRole).value<MotorProfile*>();
            if(profile == active) {
                m_motorTreeWidget->setCurrentItem(item);
                break;
            }
        }
        m_motorTreeWidget->blockSignals(false);
    }
}

// 7. 스핀박스 <-> 모델 연결 (Qt 5 qOverload 사용)
void MainWindow::connectProfileToSpinBoxes(MotorProfile* profile) {
    if (!profile) return;
    
    m_yMinSpin->setValue(profile->yMin());
    m_yMaxSpin->setValue(profile->yMax());
    m_slopeSpin->setValue(profile->maxSlope());

    // Qt 5: valueChanged(double)과 valueChanged(QString)가 있으므로 모호함 해결
    connect(m_yMinSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setYMin);
    connect(m_yMaxSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setYMax);
    connect(m_slopeSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setMaxSlope);
}

// 7. 스핀박스 <-> 모델 연결 해제 (Qt 5 qOverload 사용)
void MainWindow::disconnectProfileFromSpinBoxes(MotorProfile* profile) {
     if (!profile) return;
     disconnect(m_yMinSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setYMin);
     disconnect(m_yMaxSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setYMax);
     disconnect(m_slopeSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setMaxSlope);
}

