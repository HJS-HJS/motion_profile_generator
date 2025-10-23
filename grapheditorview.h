#pragma once

#include <QGraphicsView>
#include <QMap>
#include <QList> // QList 사용

class MotionDocument;
class MotorProfile;
class QGraphicsItem; // 전방 선언

// 1, 6, 8, 9, 10. 메인 그래프 뷰
class GraphEditorView : public QGraphicsView {
    Q_OBJECT

public:
    explicit GraphEditorView(QWidget* parent = nullptr);
    void setDocument(MotionDocument* doc);

protected:
    // 6. 휠 스크롤 (확대/축소)
    void wheelEvent(QWheelEvent* event) override;
    // 6. 휠 클릭 (패닝)
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

    // 2. 우클릭 (노드 추가)
    void contextMenuEvent(QContextMenuEvent* event) override;

    // 배경에 그리드 그리기 (선택적)
    void drawBackground(QPainter* painter, const QRectF& rect) override;

private slots:
    // 모델(Document)의 신호를 받아 뷰를 업데이트합니다.
    void onDocumentCleared();
    void onMotorAdded(MotorProfile* profile);
    void onActiveMotorChanged(MotorProfile* active, MotorProfile* previous);
    
    // 특정 프로파일의 데이터가 변경되었을 때 호출
    void onProfileDataChanged();
    // 특정 프로파일의 제약조건이 변경되었을 때 호출
    void onProfileConstraintsChanged();


private:
    void rebuildProfileItems(MotorProfile* profile);
    void updateProfileVisibility(MotorProfile* profile, bool isActive);
    void clearAllProfileItems();

    QGraphicsScene* m_scene;
    MotionDocument* m_document = nullptr;
    
    // 8, 9. 모터 프로파일과 그래픽 아이템(노드, 선)을 매핑
    QMap<MotorProfile*, QList<QGraphicsItem*>> m_profileItems;
    
    // 6. 패닝용
    bool m_isPanning = false;
    QPoint m_panStartPos;
};

