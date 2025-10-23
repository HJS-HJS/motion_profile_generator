#pragma once

#include <QGraphicsEllipseItem>
#include <QObject> // Q_OBJECT 매크로를 위해

class MotorProfile;

// 5. 그래프 위의 '노드' 아이템
class GraphNodeItem : public QObject, public QGraphicsEllipseItem {
    Q_OBJECT

public:
    // 노드는 자신이 어떤 프로필, 몇 번째 인덱스인지 알아야 합니다.
    GraphNodeItem(MotorProfile* profile, int index, QGraphicsItem* parent = nullptr);
    
    MotorProfile* profile() const { return m_profile; }
    int index() const { return m_nodeIndex; }

protected:
    // 5. 좌클릭 (수정/삭제 메뉴)
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    // 5, 7. 노드 드래그 완료 시
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    // 7. 노드 이동 시 제약조건 실시간 적용 (Y축)
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private slots:
    void onEditTriggered();  // "좌표 수정" 액션
    void onDeleteTriggered(); // "노드 삭제" 액션

private:
    MotorProfile* m_profile;
    int m_nodeIndex;
    QPointF m_dragStartPosition;
};

