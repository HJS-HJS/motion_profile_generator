#include "graphnodeitem.h"
#include "grapheditorview.h" // 스냅 기능
#include "commands.h" // 커맨드
#include <QUndoStack> // Undo 스택
#include <QGraphicsSceneMouseEvent>
#include <QMenu>
#include <QPen>
#include <QBrush>
#include <QDebug>
#include <qmath.h> // qRound, qMax, qBound

GraphNodeItem::GraphNodeItem(MotorProfile* profile, int index, 
                             GraphEditorView* view, QUndoStack* stack, 
                             QGraphicsItem* parent)
    : QObject(nullptr), QGraphicsEllipseItem(parent), 
      m_profile(profile), m_nodeIndex(index), 
      m_view(view), m_undoStack(stack)
{
    setRect(-5, -5, 10, 10); 
    setPen(QPen(Qt::black, 1));
    setBrush(QBrush(profile->color()));
    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges); 
    setFlag(QGraphicsItem::ItemIsSelectable); 
    
    // "실제" 노드 좌표(x:ms)를 가져와 "정규화된 씬" 좌표로 변환하여 설정
    MotionNode realNode = m_profile->nodeAt(m_nodeIndex);
    double sceneY = m_profile->getNormalizedY(realNode.y());
    setPos(realNode.x(), sceneY); // X는 ms 단위 그대로 사용
}

// 씬 좌표 (정규화됨)
void GraphNodeItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragStartPosition = pos(); // 씬 좌표
    }
    QGraphicsEllipseItem::mousePressEvent(event);
}

// MoveNodeCommand에 "실제" 좌표(x:ms) 전달
void GraphNodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    QGraphicsEllipseItem::mouseReleaseEvent(event);
    
    if (event->button() == Qt::LeftButton && pos() != m_dragStartPosition) {
        // "실제" 이전 좌표 (x:ms)
        QPointF oldRealPos = m_profile->nodeAt(m_nodeIndex);
        
        // "실제" 새 좌표 (x:ms)
        QPointF newScenePos = pos();
        double newRealY = m_profile->getRealY(newScenePos.y());
        QPointF newRealPos(newScenePos.x(), newRealY); // X는 ms 단위 그대로 사용

        m_undoStack->push(new MoveNodeCommand(m_profile, m_nodeIndex, oldRealPos, newRealPos));
    }
}

// 우클릭 메뉴
void GraphNodeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
    if (!isSelected()) {
        event->ignore();
        return;
    }
    QMenu menu;
    QAction* deleteAction = menu.addAction("노드 삭제");
    connect(deleteAction, &QAction::triggered, this, &GraphNodeItem::onDeleteTriggered);
    menu.exec(event->screenPos());
    event->accept(); 
}

// 씬 좌표 (정규화됨, x:ms)
QVariant GraphNodeItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionChange && scene()) {
        QPointF newScenePos = value.toPointF();

        if (m_view && m_view->isSnapEnabled()) {
            double gridX = m_view->gridSize(); // gridSize()는 이제 X축 그리드 (ms)
            double gridY_scene = (NORMALIZED_Y_MAX - NORMALIZED_Y_MIN) / 20.0; // Y축 20칸 스냅
            if (gridX > 0) {
                newScenePos.setX(qRound(newScenePos.x() / gridX) * gridX);
            }
             if (gridY_scene > 0) {
                newScenePos.setY(qRound(newScenePos.y() / gridY_scene) * gridY_scene);
            }
        }
        
        newScenePos.setX(qMax(0.0, newScenePos.x())); // X >= 0 ms

        double clampedSceneY = qBound(NORMALIZED_Y_MIN, newScenePos.y(), NORMALIZED_Y_MAX);
        newScenePos.setY(clampedSceneY);
        
        return newScenePos;
    }
    return QGraphicsItem::itemChange(change, value);
}

void GraphNodeItem::onDeleteTriggered() {
    m_undoStack->push(new DeleteNodeCommand(m_profile, m_nodeIndex));
}