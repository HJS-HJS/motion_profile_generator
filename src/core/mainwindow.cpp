#include "mainwindow.h"
#include "motionmodels.h"
#include "grapheditorview.h"
#include "graphnodeitem.h"
#include "commands.h"

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
#include <QSpinBox>
#include <QRadioButton>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QStatusBar>
#include <QUndoStack>
#include <QDebug>
#include <QTime>
#include <QGroupBox>
#include <QLabel>
#include <QTextStream>
#include <QTimer>
#include <QSettings>   // For saving/loading view options
#include <QFileInfo>   // For getting settings file path
#include <QDir>        // For getting executable path
#include <QApplication> // For applicationDirPath()

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), m_selectedNode(nullptr), m_initialViewApplied(false) // Initialize flag
{
    m_undoStack = new QUndoStack(this);
    m_document = new MotionDocument(this);
    m_view = new GraphEditorView(this);
    m_view->setDocument(m_document);
    m_view->setUndoStack(m_undoStack);
    setCentralWidget(m_view);

    createActions();
    createMenus();
    createDocks(); // Creates all three docks

    loadViewSettings(); // Load saved settings (applies to dock widgets)

    // Connect signals for interaction
    connect(m_motorTreeWidget, &QTreeWidget::currentItemChanged,
            this, &MainWindow::onMotorSelectionChanged);
    connect(m_document, &MotionDocument::modelChanged,
            this, &MainWindow::onDocumentModelChanged);
    connect(m_document, &MotionDocument::activeMotorChanged,
            this, &MainWindow::onActiveMotorSwitched);
    connect(m_fitToViewAction, &QAction::triggered, m_view, &GraphEditorView::fitToView);
    connect(m_snapGridAction, &QAction::toggled, m_view, &GraphEditorView::toggleSnapToGrid);
    connect(m_view, &GraphEditorView::nodeSelectionChanged, this, &MainWindow::onNodeSelected);

    // Initialize random seed for colors
    qsrand(QTime::currentTime().msec());

    // Add initial example motor if document is empty
    if (m_document->motorProfiles().isEmpty()) {
        // Add a default motor without dialog
        MotorProfile* m1 = m_document->addMotor("Motor 1", Qt::red);
        if (m1) {
            m1->internalAddNode(QPointF(0, 0)); // Add default node
        }
    }

    // Refresh UI based on initial document
    onDocumentModelChanged(); // This populates the tree

    // Select the first motor if available (triggers onActiveMotorSwitched)
    if (m_motorTreeWidget->topLevelItemCount() > 0 && !m_motorTreeWidget->currentItem()) {
        m_motorTreeWidget->setCurrentItem(m_motorTreeWidget->topLevelItem(0));
    }
    // If still no motor (e.g. load failed, addMotor failed?), add one
    if (m_motorTreeWidget->topLevelItemCount() == 0) {
        // Add default motor without dialog
        MotorProfile* m1 = m_document->addMotor("Motor 1", Qt::red);
        if (m1) m1->internalAddNode(QPointF(0, 0));
        onDocumentModelChanged(); // Re-populate tree
        if (m_motorTreeWidget->topLevelItemCount() > 0) {
             m_motorTreeWidget->setCurrentItem(m_motorTreeWidget->topLevelItem(0));
        }
    }
    // NOTE: Initial view fitting is now handled in showEvent

    m_undoStack->clear(); // Start with a clean undo stack
    setMinimumSize(800, 600);
    setWindowTitle("Profile Orchestrator (Qt 5) - YAML");
}

MainWindow::~MainWindow() {
    saveViewSettings(); // Save settings on exit
}

// Override showEvent to apply initial view settings *after* window is shown
void MainWindow::showEvent(QShowEvent *event) {
    QMainWindow::showEvent(event); // Call base implementation
    // Apply initial view settings only once, after event loop is ready
    if (!m_initialViewApplied && m_view) {
        m_initialViewApplied = true;
        // Use QTimer::singleShot to ensure it runs after the event loop starts
        // and after the initial motor selection signal has been processed
        QTimer::singleShot(0, this, &MainWindow::onApplyViewSettings); // Call the slot
    }
}


void MainWindow::createActions() {
    m_saveAction = new QAction("Save (&S)", this);
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::onSaveDocument);

    m_loadAction = new QAction("Open (&O)", this);
    m_loadAction->setShortcut(QKeySequence::Open);
    connect(m_loadAction, &QAction::triggered, this, &MainWindow::onLoadDocument);

    m_exportAction = new QAction("Export Samples (&E)...", this);
    connect(m_exportAction, &QAction::triggered, this, &MainWindow::onExportDocument);

    m_undoAction = m_undoStack->createUndoAction(this, "Undo (&U)");
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_redoAction = m_undoStack->createRedoAction(this, "Redo (&R)");
    m_redoAction->setShortcut(QKeySequence::Redo);

    m_fitToViewAction = new QAction("Fit to View (&F)", this);
    m_fitToViewAction->setShortcut(Qt::Key_F);

    m_snapGridAction = new QAction("Snap to Grid (&G)", this);
    m_snapGridAction->setCheckable(true);
    m_snapGridAction->setShortcut(Qt::Key_G);
}

void MainWindow::createMenus() {
    QMenu* fileMenu = menuBar()->addMenu("File (&F)");
    fileMenu->addAction(m_loadAction);
    fileMenu->addAction(m_saveAction);
    fileMenu->addAction(m_exportAction);

    QMenu* editMenu = menuBar()->addMenu("Edit (&E)");
    editMenu->addAction(m_undoAction);
    editMenu->addAction(m_redoAction);
    editMenu->addSeparator();
    editMenu->addAction(m_snapGridAction);

    QMenu* viewMenu = menuBar()->addMenu("View (&V)");
    viewMenu->addAction(m_fitToViewAction);
}

void MainWindow::createDocks() {
    // --- Left Dock: Motor List ---
    QDockWidget* leftDock = new QDockWidget("Object", this);
    QWidget* motorListWidget = new QWidget;
    QVBoxLayout* motorLayout = new QVBoxLayout(motorListWidget);
    motorLayout->setContentsMargins(0,0,0,0);
    motorLayout->setSpacing(0);
    m_motorTreeWidget = new QTreeWidget;
    m_motorTreeWidget->setHeaderHidden(true);
    m_motorTreeWidget->setRootIsDecorated(false);
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

    // --- Right Dock 1: Properties ---
    QDockWidget* rightDock = new QDockWidget("Properties", this);
    QWidget* propertiesWidget = new QWidget;
    QVBoxLayout* rightLayout = new QVBoxLayout(propertiesWidget);

    QGroupBox* constraintsGroup = new QGroupBox("Motor Constraints");
    QFormLayout* formLayout = new QFormLayout;
    m_yMaxSpin = new QDoubleSpinBox;
    m_yMaxSpin->setRange(-100000, 100000);
    formLayout->addRow("Y Max:", m_yMaxSpin);
    m_yMinSpin = new QDoubleSpinBox;
    m_yMinSpin->setRange(-100000, 100000);
    formLayout->addRow("Y Min:", m_yMinSpin);
    m_slopeSpin = new QDoubleSpinBox;
    m_slopeSpin->setRange(0, 100000);
    m_slopeSpin->setValue(1000.0);
    formLayout->addRow("Max Slope:", m_slopeSpin);
    m_applyConstraintsButton = new QPushButton("Apply Constraints");
    formLayout->addWidget(m_applyConstraintsButton);
    constraintsGroup->setLayout(formLayout);
    rightLayout->addWidget(constraintsGroup);

    m_nodeEditGroup = new QGroupBox("Selected Node");
    QFormLayout* nodeEditLayout = new QFormLayout;
    m_nodeXSpin = new QDoubleSpinBox;
    m_nodeXSpin->setRange(0.0, 1000000.0);
    m_nodeXSpin->setDecimals(1);
    m_nodeYSpin = new QDoubleSpinBox;
    m_nodeYSpin->setRange(-100000.0, 100000.0);
    m_nodeYSpin->setDecimals(3);
    m_applyNodeCoordsButton = new QPushButton("Apply Coordinates");
    nodeEditLayout->addRow("Time (X):", m_nodeXSpin);
    nodeEditLayout->addRow("Value (Y):", m_nodeYSpin);
    nodeEditLayout->addWidget(m_applyNodeCoordsButton);
    m_nodeEditGroup->setLayout(nodeEditLayout);
    rightLayout->addWidget(m_nodeEditGroup);
    m_nodeEditGroup->setEnabled(false);

    rightLayout->addStretch();
    rightDock->setWidget(propertiesWidget);
    addDockWidget(Qt::RightDockWidgetArea, rightDock);

    connect(m_applyConstraintsButton, &QPushButton::clicked, this, &MainWindow::onApplyConstraints);
    connect(m_applyNodeCoordsButton, &QPushButton::clicked, this, &MainWindow::onApplyNodeCoords);

    // --- Right Dock 2: View Options ---
    QDockWidget* viewDock = new QDockWidget("View Options", this);
    QWidget* viewOptionsWidget = new QWidget;
    QVBoxLayout* viewLayout = new QVBoxLayout(viewOptionsWidget);

    QGroupBox* gridGroup = new QGroupBox("Grid & Scale");
    QFormLayout* gridLayout = new QFormLayout;

    m_yDivisionsSpin = new QSpinBox;
    m_yDivisionsSpin->setRange(1, 10);
    m_yDivisionsSpin->setValue(10); // Default +/- 10
    m_yDivisionsSpin->setToolTip("Number of divisions above (and below) the X-axis");
    gridLayout->addRow("Y Divisions (+/-):", m_yDivisionsSpin);

    m_refYSpin = new QDoubleSpinBox;
    m_refYSpin->setRange(1.0, 100000.0);
    m_refYSpin->setDecimals(1);
    m_refYSpin->setValue(DEFAULT_REFERENCE_Y); // Default 100
    m_refYSpin->setToolTip("The real Y value that maps to the top/bottom of the view");
    gridLayout->addRow("Reference Y Value:", m_refYSpin);

    m_gridXSpin = new QDoubleSpinBox; // Added Grid Size X
    m_gridXSpin->setRange(1.0, 1000.0);
    m_gridXSpin->setDecimals(0);
    m_gridXSpin->setValue(50.0); // Default 50ms
    m_gridXSpin->setSuffix(" ms");
    gridLayout->addRow("Grid Size X (ms):", m_gridXSpin);

    m_gridLargeXSpin = new QDoubleSpinBox; // Added Major Grid X
    m_gridLargeXSpin->setRange(100.0, 10000.0);
    m_gridLargeXSpin->setDecimals(0);
    m_gridLargeXSpin->setValue(1000.0); // Default 1000ms
    m_gridLargeXSpin->setSuffix(" ms");
    gridLayout->addRow("Major Grid X (ms):", m_gridLargeXSpin);

    gridGroup->setLayout(gridLayout);
    viewLayout->addWidget(gridGroup);

    // X Range Group Removed
    
    m_applyViewButton = new QPushButton("Apply View Settings"); // Renamed button
    viewLayout->addWidget(m_applyViewButton);
    viewLayout->addStretch();
    viewDock->setWidget(viewOptionsWidget);
    addDockWidget(Qt::RightDockWidgetArea, viewDock);

    // Connect *button* to the new slot
    connect(m_applyViewButton, &QPushButton::clicked, this, &MainWindow::onApplyViewSettings);
    // Remove direct connections from spin boxes
} // End of createDocks

// --- Slot Implementations ---

// Slot for the "Apply View Settings" button
void MainWindow::onApplyViewSettings() {
    if (!m_view) return;
    // Apply all settings from the dock to the view
    m_view->setNumYDivisions(m_yDivisionsSpin->value());
    m_view->setGridSizeX(m_gridXSpin->value());
    m_view->setGridLargeSizeX(m_gridLargeXSpin->value());
    // Apply Reference Y last as it triggers rebuilds and refitting
    m_view->setReferenceYValue(m_refYSpin->value());
    
    // X Range is no longer set here
}


void MainWindow::onMotorSelectionChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous) {
    Q_UNUSED(previous);
    MotorProfile* profile = nullptr;
    if (current) {
        profile = current->data(0, Qt::UserRole).value<MotorProfile*>();
    }
    m_document->setActiveMotor(profile);
}

void MainWindow::onAddMotor() {
    bool ok;
    QString name = QInputDialog::getText(this, "New Motor", "Motor Name:", QLineEdit::Normal, "New Motor", &ok);
    if (ok && !name.isEmpty()) {
        QColor color = QColor::fromHsv(qrand() % 360, 200, 200);
        MotorProfile* newMotor = m_document->addMotor(name, color);

        if (newMotor) {
            // Add a default (0,0) node
            newMotor->internalAddNode(QPointF(0.0, 0.0));
        }

        // Auto-select the new motor in the tree
        if (newMotor) {
             for(int i = 0; i < m_motorTreeWidget->topLevelItemCount(); ++i) {
                 QTreeWidgetItem* item = m_motorTreeWidget->topLevelItem(i);
                 if(item->data(0, Qt::UserRole).value<MotorProfile*>() == newMotor){
                     m_motorTreeWidget->setCurrentItem(item);
                     break;
                 }
             }
        }
    }
}

void MainWindow::onRemoveMotor() {
    QTreeWidgetItem* currentItem = m_motorTreeWidget->currentItem();
    if (!currentItem) return;
    MotorProfile* profile = currentItem->data(0, Qt::UserRole).value<MotorProfile*>();
    if (!profile) return;
    auto reply = QMessageBox::question(this, "Delete Motor",
        QString("Are you sure you want to delete '%1'?").arg(profile->name()),
        QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        m_document->removeMotor(profile);
    }
}

void MainWindow::onSaveDocument() {
    QString fileName = QFileDialog::getSaveFileName(this, "Save Profile", "", "Motion YAML File (*.yaml)");
    if (fileName.isEmpty()) return;
    bool ok;
    QString id = QInputDialog::getText(this, "Enter ID", "Enter File ID:", QLineEdit::Normal, "default_id", &ok);
    if (!ok) return;
    if (id.isEmpty()) id = "default_id";
    if (!m_document->saveToYAML(fileName, id)) {
        QMessageBox::warning(this, "Save Failed", "Failed to save the file.");
    } else {
        statusBar()->showMessage("YAML file saved.", 3000);
    }
}

void MainWindow::onLoadDocument() {
    QString fileName = QFileDialog::getOpenFileName(this, "Load Profile", "", "Motion YAML File (*.yaml)");
    if (fileName.isEmpty()) return;

    m_undoStack->clear();

    if (!m_document->loadFromYAML(fileName)) {
        QMessageBox::warning(this, "Load Failed", "Failed to load or parse the file.");
    } else {
        statusBar()->showMessage("YAML file loaded.", 3000);
         QTimer::singleShot(0, this, [=]() {
             loadViewSettings(); // Load saved settings
             onApplyViewSettings(); // Apply them
         });
    }
}

void MainWindow::onFitToView() {
    if(m_view) m_view->fitToView();
}

void MainWindow::onApplyConstraints() {
    MotorProfile* profile = m_document ? m_document->activeProfile() : nullptr;
    if (profile) {
        profile->checkAllNodes();
    }
}

void MainWindow::onDocumentModelChanged() {
    m_motorTreeWidget->blockSignals(true);
    m_motorTreeWidget->clear();

    QTreeWidgetItem* itemToSelect = nullptr;
    MotorProfile* currentActive = m_document ? m_document->activeProfile() : nullptr;

    if (m_document) {
        for (MotorProfile* profile : m_document->motorProfiles()) {
            if (!profile) continue;
            QTreeWidgetItem* item = new QTreeWidgetItem(m_motorTreeWidget);
            item->setText(0, profile->name());
            item->setForeground(0, profile->color());
            item->setData(0, Qt::UserRole, QVariant::fromValue(profile));
            if (profile == currentActive) {
                itemToSelect = item;
            }
        }
    }

    if (itemToSelect) {
        m_motorTreeWidget->setCurrentItem(itemToSelect);
    } else if (m_motorTreeWidget->topLevelItemCount() > 0) {
        m_motorTreeWidget->setCurrentItem(m_motorTreeWidget->topLevelItem(0));
    }

    m_motorTreeWidget->blockSignals(false);

    // Update properties *after* selection is potentially set
    onActiveMotorSwitched(m_document ? m_document->activeProfile() : nullptr, nullptr);
    onNodeSelected(nullptr);
}

void MainWindow::onActiveMotorSwitched(MotorProfile* active, MotorProfile* previous) {
    if (previous) {
        disconnectProfileFromSpinBoxes(previous);
    }
    bool motorIsActive = (active != nullptr);
    m_yMinSpin->setEnabled(motorIsActive);
    m_yMaxSpin->setEnabled(motorIsActive);
    m_slopeSpin->setEnabled(motorIsActive);
    m_applyConstraintsButton->setEnabled(motorIsActive);

    if (active) {
        connectProfileToSpinBoxes(active);

        m_motorTreeWidget->blockSignals(true);
        for(int i=0; i < m_motorTreeWidget->topLevelItemCount(); ++i) {
            QTreeWidgetItem* item = m_motorTreeWidget->topLevelItem(i);
            if(item->data(0, Qt::UserRole).value<MotorProfile*>() == active) {
                if (m_motorTreeWidget->currentItem() != item) {
                     m_motorTreeWidget->setCurrentItem(item);
                }
                break;
            }
        }
        m_motorTreeWidget->blockSignals(false);

        // Apply View Settings (which fits the view)
        onApplyViewSettings();
        // Also fit X axis to this specific motor
        m_view->fitToActiveMotor(active);

    } else {
         m_yMinSpin->setValue(0);
         m_yMaxSpin->setValue(0);
         m_slopeSpin->setValue(0);
    }
    onNodeSelected(nullptr);
}

void MainWindow::connectProfileToSpinBoxes(MotorProfile* profile) {
    if (!profile) return;

    m_yMinSpin->blockSignals(true);
    m_yMaxSpin->blockSignals(true);
    m_slopeSpin->blockSignals(true);

    m_yMinSpin->setValue(profile->yMin());
    m_yMaxSpin->setValue(profile->yMax());
    m_slopeSpin->setValue(profile->maxSlope());

    m_yMinSpin->blockSignals(false);
    m_yMaxSpin->blockSignals(false);
    m_slopeSpin->blockSignals(false);

    #if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
        connect(m_yMinSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setYMin, Qt::UniqueConnection);
        connect(m_yMaxSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setYMax, Qt::UniqueConnection);
        connect(m_slopeSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setMaxSlope, Qt::UniqueConnection);
    #else
        connect(m_yMinSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setYMin, Qt::UniqueConnection);
        connect(m_yMaxSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setYMax, Qt::UniqueConnection);
        connect(m_slopeSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setMaxSlope, Qt::UniqueConnection);
    #endif
}

void MainWindow::disconnectProfileFromSpinBoxes(MotorProfile* profile) {
     if (!profile) return;
     #if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
         disconnect(m_yMinSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setYMin);
         disconnect(m_yMaxSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setYMax);
         disconnect(m_slopeSpin, qOverload<double>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setMaxSlope);
     #else
         disconnect(m_yMinSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setYMin);
         disconnect(m_yMaxSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setYMax);
         disconnect(m_slopeSpin, static_cast<void (QDoubleSpinBox::*)(double)>(&QDoubleSpinBox::valueChanged), profile, &MotorProfile::setMaxSlope);
     #endif
}


void MainWindow::writeYAMLSamples(QTextStream& out, MotorProfile* profile, double sampleRateHz, double endTimeSec) const {
    if (!profile) return;
    QString keyName = profile->name(); keyName.replace(':', '_').replace(' ', '_');
    out << "  " << keyName << ":\n";
    out << "    - [";
    if (sampleRateHz <= 0 || endTimeSec < 0) {
        out << "]\n";
        return;
    }
    double dt_ms = (1.0 / sampleRateHz) * 1000.0;
    if (dt_ms < 1e-3) dt_ms = 1.0;
    bool first = true;
    for (double time_ms = 0.0; time_ms <= endTimeSec + (dt_ms/2.0); time_ms += dt_ms) {
         double capped_time_ms = qMin(time_ms, endTimeSec);
        if (!first) out << ", ";
        double value = profile->sampleAt(capped_time_ms);
        out << "[" << QString::number(capped_time_ms, 'g', 10) << ", " << QString::number(value, 'g', 10) << "]";
        first = false;
         if (capped_time_ms >= endTimeSec) break;
    }
     if (endTimeSec == 0.0 && dt_ms > 0.0 && first) {
         double value = profile->sampleAt(0.0);
         out << "[" << QString::number(0.0, 'g', 10) << ", " << QString::number(value, 'g', 10) << "]";
     }
    out << "]\n";
}

void MainWindow::onExportDocument() {
    if (!m_document) return;
    QDialog dialog(this);
    dialog.setWindowTitle("Export Samples");
    QVBoxLayout layout(&dialog);
    QLabel* infoLabel = new QLabel("Export all motors as a sampled YAML file.");
    layout.addWidget(infoLabel);
    QWidget* optionsWidget = new QWidget;
    QFormLayout* optionsLayout = new QFormLayout(optionsWidget);
    QDoubleSpinBox* endTimeSpin = new QDoubleSpinBox;
    endTimeSpin->setRange(0.0, 1000000.0);
    endTimeSpin->setDecimals(0);
    endTimeSpin->setSingleStep(100);
    double maxTime = 2000.0;
    for (MotorProfile* p : m_document->motorProfiles()) {
        if (p && !p->nodes().isEmpty() && p->nodes().last().x() > maxTime) {
            maxTime = p->nodes().last().x();
        }
    }
    endTimeSpin->setValue(maxTime);
    endTimeSpin->setSuffix(" ms");
    QSpinBox* hzSpin = new QSpinBox;
    hzSpin->setRange(1, 10000);
    hzSpin->setValue(100);
    hzSpin->setSuffix(" Hz");
    optionsLayout->addRow("End Time:", endTimeSpin);
    optionsLayout->addRow("Sample Rate:", hzSpin);
    layout.addWidget(optionsWidget);
    QDialogButtonBox* buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout.addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() != QDialog::Accepted) return;
    bool okId;
    QString id = QInputDialog::getText(this, "Enter ID", "Enter File ID:", QLineEdit::Normal, "default_id", &okId);
    if (!okId) return;
    if (id.isEmpty()) id = "default_id";
    QString fileName = QFileDialog::getSaveFileName(this, "Export Samples", "", "YAML File (*.yaml)");
    if (fileName.isEmpty()) return;
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "File Error", "Could not open file for writing:\n" + file.errorString());
        return;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << "id: " << id << "\n";
    double endTimeMs = endTimeSpin->value();
    int sampleRateHz = hzSpin->value();
    for (MotorProfile* profile : m_document->motorProfiles()) {
         if (profile) {
             writeYAMLSamples(out, profile, sampleRateHz, endTimeMs);
         }
    }
    file.close();
    statusBar()->showMessage("Sample export complete.", 3000);
}

void MainWindow::onNodeSelected(QGraphicsItem* selectedNodeItem) {
    m_selectedNode = qgraphicsitem_cast<GraphNodeItem*>(selectedNodeItem);
    if (m_selectedNode && m_selectedNode->profile()) {
        m_nodeEditGroup->setEnabled(true);
        m_nodeXSpin->blockSignals(true);
        m_nodeYSpin->blockSignals(true);
        MotorProfile* profile = m_selectedNode->profile();
        int index = m_selectedNode->index();
        if (index >= 0 && index < profile->nodeCount()) {
             QPointF realPos = profile->nodeAt(index);
             m_nodeXSpin->setValue(realPos.x());
             m_nodeYSpin->setValue(realPos.y());
        } else {
             qWarning() << "onNodeSelected: Node index" << index << "invalid for profile" << profile->name();
             m_nodeXSpin->setValue(0);
             m_nodeYSpin->setValue(0);
        }
        m_nodeYSpin->setRange(profile->yMin(), profile->yMax());
        m_nodeXSpin->blockSignals(false);
        m_nodeYSpin->blockSignals(false);
    } else {
        m_nodeEditGroup->setEnabled(false);
        m_nodeXSpin->setValue(1000);
        m_nodeYSpin->setValue(50);
        m_selectedNode = nullptr;
    }
}

void MainWindow::onApplyNodeCoords() {
    if (!m_selectedNode || !m_undoStack || !m_selectedNode->profile()) return;
    MotorProfile* profile = m_selectedNode->profile();
    int index = m_selectedNode->index();
    if (index < 0 || index >= profile->nodeCount()) {
        qWarning() << "Selected node index" << index << "is invalid when applying coordinates.";
        onNodeSelected(nullptr);
        return;
    }
    QPointF oldRealPos = profile->nodeAt(index);
    QPointF newRealPos(m_nodeXSpin->value(), m_nodeYSpin->value());
    if (qAbs(oldRealPos.x() - newRealPos.x()) < 1e-6 && qAbs(oldRealPos.y() - newRealPos.y()) < 1e-6) return;
    m_undoStack->push(new MoveNodeCommand(profile, index, oldRealPos, newRealPos));
}

// --- Settings Load/Save ---
QString MainWindow::getSettingsFilePath() const {
    QString appPath = QApplication::applicationDirPath();
    return QDir(appPath).filePath(".view_settings.ini");
}

void MainWindow::saveViewSettings() {
    if (!m_yDivisionsSpin) return;
    QSettings settings(getSettingsFilePath(), QSettings::IniFormat);
    settings.beginGroup("ViewOptions");
    settings.setValue("YDivisions", m_yDivisionsSpin->value());
    settings.setValue("ReferenceY", m_refYSpin->value());
    settings.setValue("GridSizeX", m_gridXSpin->value());
    settings.setValue("MajorGridX", m_gridLargeXSpin->value());
    // XMin/XMax Removed
    settings.setValue("SnapGrid", m_snapGridAction->isChecked());
    settings.endGroup();
}

void MainWindow::loadViewSettings() {
    if (!m_yDivisionsSpin) {
       qWarning() << "loadViewSettings called before docks were created.";
       return;
    }
    QSettings settings(getSettingsFilePath(), QSettings::IniFormat);
    settings.beginGroup("ViewOptions");
    m_yDivisionsSpin->setValue(settings.value("YDivisions", 10).toInt());
    m_refYSpin->setValue(settings.value("ReferenceY", DEFAULT_REFERENCE_Y).toDouble());
    m_gridXSpin->setValue(settings.value("GridSizeX", 50.0).toDouble());
    m_gridLargeXSpin->setValue(settings.value("MajorGridX", 1000.0).toDouble());
    // XMin/XMax Removed
    m_snapGridAction->setChecked(settings.value("SnapGrid", false).toBool());
    settings.endGroup();

    // Apply loaded settings to the view's internal state
    if(m_view) {
        m_view->setNumYDivisions(m_yDivisionsSpin->value());
        m_view->setReferenceYValue(m_refYSpin->value());
        m_view->setGridSizeX(m_gridXSpin->value());
        m_view->setGridLargeSizeX(m_gridLargeXSpin->value());
        m_view->toggleSnapToGrid(m_snapGridAction->isChecked());
    }
}