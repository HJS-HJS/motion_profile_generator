#pragma once

#include <QMainWindow>
#include <QGraphicsItem> // For node selection signal
#include <QShowEvent>    // For initial view setting

// Use QMetaMethod header for qOverload
#if QT_VERSION >= QT_VERSION_CHECK(5, 7, 0)
#include <QMetaMethod>
#endif

// Forward declarations
class MotionDocument;
class GraphEditorView;
class MotorProfile;
class GraphNodeItem;
class QTreeWidget;
class QTreeWidgetItem;
class QDoubleSpinBox;
class QSpinBox;
class QToolButton;
class QPushButton;
class QUndoStack;
class QAction;
class QGroupBox;
class QDockWidget;
class QTextStream;
class QSettings; // For settings

/**
 * @brief The main application window.
 * Manages docks, menus, actions, and facilitates communication
 * between the document (model) and the view.
 */
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    // Override showEvent to apply initial view settings *after* window is shown
    void showEvent(QShowEvent *event) override;

private slots:
    // Motor list actions
    void onMotorSelectionChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onAddMotor();
    void onRemoveMotor();

    // File actions
    void onSaveDocument();
    void onLoadDocument();
    void onExportDocument(); // Export Samples

    // View actions
    void onFitToView();
    void onApplyViewSettings(); // Slot for the "Apply View Settings" button

    // Properties actions
    void onApplyConstraints(); // Applies Y Min/Max from profile

    // Model update slots
    void onDocumentModelChanged(); // Rebuilds motor list
    void onActiveMotorSwitched(MotorProfile* active, MotorProfile* previous); // Connects properties

    // Node editing slots
    void onNodeSelected(QGraphicsItem* selectedNode); // Updates node coordinate spins
    void onApplyNodeCoords(); // Applies changes from node coordinate spins

    // Settings
    void loadViewSettings();
    void saveViewSettings();


private:
    void createActions();
    void createMenus();
    void createDocks(); // Creates all dock widgets

    // Helpers
    QString getSettingsFilePath() const;
    void connectProfileToSpinBoxes(MotorProfile* profile);
    void disconnectProfileFromSpinBoxes(MotorProfile* profile);

    // YAML export helpers
    void writeYAMLSamples(QTextStream& out, MotorProfile* profile, double sampleRateHz, double endTimeSec) const;

    // Core data and view
    MotionDocument* m_document;
    GraphEditorView* m_view;

    // Undo/Redo stack
    QUndoStack* m_undoStack;

    // Left Dock: Motor List
    QTreeWidget* m_motorTreeWidget;
    QToolButton* m_addMotorButton;
    QToolButton* m_removeMotorButton;

    // Right Dock 1: Properties
    QDoubleSpinBox* m_yMaxSpin; // Swapped order
    QDoubleSpinBox* m_yMinSpin;
    QDoubleSpinBox* m_slopeSpin;
    QPushButton* m_applyConstraintsButton;

    // Right Dock 1: Selected Node Editor
    QGroupBox* m_nodeEditGroup;
    QDoubleSpinBox* m_nodeXSpin;
    QDoubleSpinBox* m_nodeYSpin;
    QPushButton* m_applyNodeCoordsButton;
    GraphNodeItem* m_selectedNode = nullptr; // Track currently selected node item

    // Right Dock 2: View Options
    QSpinBox* m_yDivisionsSpin;
    QDoubleSpinBox* m_xMinSpin;
    QDoubleSpinBox* m_xMaxSpin;
    QPushButton* m_applyViewButton; // Renamed button
    QDoubleSpinBox* m_refYSpin; // Added
    // QDoubleSpinBox* m_refXSpin; // Removed

    // Actions (for menus and shortcuts)
    QAction* m_saveAction;
    QAction* m_loadAction;
    QAction* m_exportAction;
    QAction* m_undoAction;
    QAction* m_redoAction;
    QAction* m_fitToViewAction;
    QAction* m_snapGridAction;

    // Flag for initial view setup
    bool m_initialViewApplied = false;
};
