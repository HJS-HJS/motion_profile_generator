
#pragma once

#include <QMainWindow>
#include <QOverload> // Qt 5에서 모호한 시그널을 위해 필요

// 전방 선언
class MotionDocument;
class GraphEditorView;
class MotorProfile;
class QTreeWidget;      // QListWidget 대신 QTreeWidget
class QTreeWidgetItem;  // QTreeWidget 아이템
class QDoubleSpinBox;

// 7, 8, 11, 12. 메인 윈도우
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    // 8. 모터 선택 변경 (QTreeWidget용 슬롯)
    void onMotorSelectionChanged(QTreeWidgetItem* current);
    
    // 11. 저장
    void onSaveDocument();
    // 12. 불러오기
    void onLoadDocument();
    
    // 모델 신호 슬롯
    void onDocumentModelChanged(); // 모델이 (불러오기 등으로) 통째로 바뀜
    // (Qt 6 코드 버그 수정: 인자 2개 받도록)
    void onActiveMotorSwitched(MotorProfile* active, MotorProfile* previous);

private:
    void createActions();
    void createMenus();
    void createDocks();
    
    // 7. 현재 활성 프로파일의 제약조건을 UI 스핀박스에 연결
    void connectProfileToSpinBoxes(MotorProfile* profile);
    void disconnectProfileFromSpinBoxes(MotorProfile* profile);

    MotionDocument* m_document;
    GraphEditorView* m_view;

    // 8. 좌측 모터 선택창 (이미지 반영)
    QTreeWidget* m_motorTreeWidget;
    QTreeWidgetItem* m_motorsRootItem; // "Motors" 루트 아이템

    // 7. 우측 제약조건 메뉴
    QDoubleSpinBox* m_yMinSpin;
    QDoubleSpinBox* m_yMaxSpin;
    QDoubleSpinBox* m_slopeSpin;
    
    // 11, 12. 메뉴 액션
    QAction* m_saveAction;
    QAction* m_loadAction;
};
