#pragma once

#include <QMainWindow>
#include <QGraphicsItem> // 1번: QGraphicsItem

// 전방 선언
class MotionDocument;
class GraphEditorView;
class MotorProfile;
class GraphNodeItem; // 1번: GraphNodeItem
class QTreeWidget;
class QTreeWidgetItem;
class QDoubleSpinBox;
class QToolButton; 
class QPushButton; 
class QUndoStack; 
class QAction;
class QGroupBox; // 1번: QGroupBox

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onMotorSelectionChanged(QTreeWidgetItem* current, QTreeWidgetItem* previous);
    void onAddMotor();
    void onRemoveMotor();
    void onSaveDocument();
    void onLoadDocument();
    void onExportDocument(); 
    void onFitToView();      
    void onApplyConstraints();
    void onDocumentModelChanged();
    void onActiveMotorSwitched(MotorProfile* active, MotorProfile* previous);

    // 1번: 노드 좌표 편집 슬롯
    void onNodeSelected(QGraphicsItem* selectedNode);
    void onApplyNodeCoords();

private:
    void createActions();
    void createMenus();
    void createDocks();
    
    void connectProfileToSpinBoxes(MotorProfile* profile);
    void disconnectProfileFromSpinBoxes(MotorProfile* profile);

    MotionDocument* m_document;
    GraphEditorView* m_view;

    QUndoStack* m_undoStack; 
    
    QTreeWidget* m_motorTreeWidget;
    // QTreeWidgetItem* m_motorsRootItem; // 2번: 제거됨
    QToolButton* m_addMotorButton;
    QToolButton* m_removeMotorButton;

    QDoubleSpinBox* m_yMinSpin;
    QDoubleSpinBox* m_yMaxSpin;
    QDoubleSpinBox* m_slopeSpin;
    QPushButton* m_applyConstraintsButton;
    
    // 1번: 노드 편집 위젯
    QGroupBox* m_nodeEditGroup;
    QDoubleSpinBox* m_nodeXSpin; // X는 ms 단위
    QDoubleSpinBox* m_nodeYSpin; // Y는 실제 단위
    QPushButton* m_applyNodeCoordsButton;
    GraphNodeItem* m_selectedNode = nullptr; // 현재 선택된 노드 저장
    
    QAction* m_saveAction;
    QAction* m_loadAction;
    QAction* m_exportAction; 
    QAction* m_undoAction;   
    QAction* m_redoAction;   
    QAction* m_fitToViewAction; 
    QAction* m_snapGridAction;  
};