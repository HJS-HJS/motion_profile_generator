#include "graphnodeitem.h"
#include "motionmodels.h"
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include <QInputDialog>
#include <QPen>
#include <QBrush>
#include <QDebug>

GraphNodeItem::GraphNodeItem(MotorProfile* profile, int index, QGraphicsItem* parent)
    : QObject(nullptr), QGraphicsEllipseItem(parent), m_profile(profile), m_nodeIndex(index)
{
    // 노드의 그래픽 속성 설정
    setRect(-5, -5, 10, 10); // 10x10 크기
    setPen(QPen(Qt::black, 1));
    setBrush(QBrush(profile->color()));
    
    // 5. 노드가 움직일 수 있도록 설정
    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges); // itemChange() 활성화
    
    // 5. 노드 위치 설정 (모델 기준)
    setPos(m_profile->nodeAt(m_nodeIndex));
}

// 5. 좌클릭 시 수정/삭제 메뉴
void GraphNodeItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragStartPosition = pos(); // 드래그 시작 위치 저장
        
        QMenu menu;
        QAction* editAction = menu.addAction("좌표 수정");
        QAction* deleteAction = menu.addAction("노드 삭제");

        connect(editAction, &QAction::triggered, this, &GraphNodeItem::onEditTriggered);
        connect(deleteAction, &QAction::triggered, this, &GraphNodeItem::onDeleteTriggered);
        
        menu.exec(event->screenPos());
        event->accept(); // 이벤트 소비
    } else {
        QGraphicsEllipseItem::mousePressEvent(event); // 드래그 등 기본 동작
    }
}

// 5, 7. 마우스를 놓았을 때 (드래그 완료) 모델 업데이트
void GraphNodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    QGraphicsEllipseItem::mouseReleaseEvent(event);
    
    if (pos() != m_dragStartPosition) {
        // 위치가 변경되었으면 모델 업데이트 시도
        if (!m_profile->updateNode(m_nodeIndex, pos())) {
            // 제약조건 등으로 실패하면 원래 위치로 복귀
            setPos(m_dragStartPosition);
            qWarning() << "노드 이동 실패: 제약조건 위반";
        }
    }
}


// 7. 노드 이동 시 제약조건 실시간 적용 (Y축)
QVariant GraphNodeItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionChange && scene()) {
        // ItemPositionChange는 새 위치(value)를 제안합니다.
        QPointF newPos = value.toPointF();

        // 7. Y축 제한
        double clampedY = qBound(m_profile->yMin(), newPos.y(), m_profile->yMax());
        newPos.setY(clampedY);
        
        // 7. (고급) 실시간 기울기 제한: 생략
        
        return newPos; // 제한이 적용된 새 위치 반환
    }
    return QGraphicsItem::itemChange(change, value); // 기본 동작
}


void GraphNodeItem::onEditTriggered() {
    MotionNode currentNode = m_profile->nodeAt(m_nodeIndex);
    
    bool ok_time, ok_value;
    double newTime = QInputDialog::getDouble(nullptr, "좌표 수정", "시간 (X):", currentNode.x(), -10000, 10000, 3, &ok_time);
    double newValue = QInputDialog::getDouble(nullptr, "좌표 수정", "값 (Y):", currentNode.y(), m_profile->yMin(), m_profile->yMax(), 3, &ok_value);

    if (ok_time && ok_value) {
        if (!m_profile->updateNode(m_nodeIndex, MotionNode(newTime, newValue))) {
            qWarning() << "노드 수정 실패: 제약조건 위반";
        }
    }
}

void GraphNodeItem::onDeleteTriggered() {
    m_profile->deleteNode(m_nodeIndex);
}

