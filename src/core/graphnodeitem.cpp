#include "graphnodeitem.h"
#include "grapheditorview.h" // Needed for view properties and constants
#include "motionmodels.h"     // Needed for MotorProfile definition
#include "commands.h"         // For undo commands
#include <QUndoStack>
#include <QGraphicsSceneMouseEvent>
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QPen>
#include <QBrush>
#include <QDebug>
#include <qmath.h> // qRound, qMax, qBound

/**
 * @brief Helper function to calculate the motor-specific visual Y scale factor.
 */
inline qreal getMotorVisualScale(MotorProfile* profile, double referenceYValue) {
    qreal effDefYScale = (referenceYValue > 1e-9) ? VISUAL_Y_TARGET / referenceYValue : 1.0;
    if (!profile) return effDefYScale / 100.0; // Fallback scale
    double max_abs_real = qMax(qAbs(profile->yMax()), qAbs(profile->yMin()));
    if (max_abs_real < 1e-9) return effDefYScale / 100.0; // Fallback scale
    return referenceYValue / max_abs_real; // motorScale = target_Y_scene / max_Y_real
}


// Constructor implementation
GraphNodeItem::GraphNodeItem(MotorProfile* profile, int index,
                             GraphEditorView* view, QUndoStack* stack,
                             QGraphicsItem* parent)
    : QObject(nullptr),
      QGraphicsEllipseItem(parent),
      m_profile(profile), m_nodeIndex(index),
      m_view(view), m_undoStack(stack)
{
    setRect(-10, -10, 20, 20); // Node diameter 20px
    QPen borderPen(Qt::black, 1);
    borderPen.setCosmetic(true);
    setPen(borderPen);
    setBrush(QBrush(profile ? profile->color() : Qt::gray));

    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges);
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemIgnoresTransformations); // Keep visual size constant

    // Set initial position
    if (m_profile && m_view && m_nodeIndex >= 0 && m_nodeIndex < m_profile->nodeCount()) {
        MotionNode realNode = m_profile->nodeAt(m_nodeIndex);
        qreal motorScale = getMotorVisualScale(m_profile, m_view->getReferenceYValue());
        setPos(realNode.x(), realNode.y() * motorScale);
    } else {
        qWarning() << "GraphNodeItem created with invalid profile, view, or index.";
        setPos(0,0);
    }
}

// <<< mousePressEvent 구현 복원 >>>
void GraphNodeItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragStartPosition = pos(); // Record current scene position
    }
    QGraphicsEllipseItem::mousePressEvent(event); // Call base class
}

// <<< mouseReleaseEvent 구현 복원 (단일 MoveNodeCommand 사용) >>>
void GraphNodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    QGraphicsEllipseItem::mouseReleaseEvent(event);

    if (event->button() == Qt::LeftButton && pos() != m_dragStartPosition) {
        if (!m_profile || !m_undoStack || !m_view) return;

        if (m_nodeIndex < 0 || m_nodeIndex >= m_profile->nodeCount()) {
             qWarning() << "Node index" << m_nodeIndex << "is invalid on mouse release.";
             setPos(m_dragStartPosition);
             return;
        }

        QPointF oldRealPos = m_profile->nodeAt(m_nodeIndex);
        QPointF newScenePos = pos();
        qreal motorScale = getMotorVisualScale(m_profile, m_view->getReferenceYValue());
        if (qAbs(motorScale) < 1e-9) motorScale = 1.0;
        QPointF newRealPos(newScenePos.x(), newScenePos.y() / motorScale);
        newRealPos.setY(qBound(m_profile->yMin(), newRealPos.y(), m_profile->yMax()));

        // Push the *single* MoveNodeCommand
        m_undoStack->push(new MoveNodeCommand(m_profile, m_nodeIndex, oldRealPos, newRealPos));
    }
}


// Show context menu (Delete Node) on right-click
void GraphNodeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
    if (!isSelected() || !m_profile || !m_undoStack) {
        event->ignore();
        return;
    }
    if (m_nodeIndex < 0 || m_nodeIndex >= m_profile->nodeCount()) {
         qWarning() << "Node index" << m_nodeIndex << "is invalid for context menu.";
         event->ignore();
         return;
     }

    QMenu menu;
    QAction* deleteAction = menu.addAction("Delete Node");
    connect(deleteAction, &QAction::triggered, this, &GraphNodeItem::onDeleteTriggered);

    menu.exec(event->screenPos());
    event->accept();
}

// Handles position changes during dragging (snapping, constraints)
QVariant GraphNodeItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    if (change == ItemPositionChange && scene() && m_profile && m_view) {
        QPointF newScenePos = value.toPointF();
        qreal motorScale = getMotorVisualScale(m_profile, m_view->getReferenceYValue());
        if (qAbs(motorScale) < 1e-9) motorScale = 1.0;

        if (m_view->isSnapEnabled()) {
            double gridX = m_view->gridSizeX();
            double gridY_scene = m_view->gridSizeY();
            if (gridX > 0) newScenePos.setX(qRound(newScenePos.x() / gridX) * gridX);
            if (gridY_scene > 0) newScenePos.setY(qRound(newScenePos.y() / gridY_scene) * gridY_scene);
        }

        newScenePos.setX(qMax(0.0, newScenePos.x()));

        double sceneYMin = m_profile->yMin() * motorScale;
        double sceneYMax = m_profile->yMax() * motorScale;
        double clampedSceneY = qBound(sceneYMin, newScenePos.y(), sceneYMax);
        newScenePos.setY(clampedSceneY);

        return newScenePos;
    }
    return QGraphicsItem::itemChange(change, value);
}

// Slot called when "Delete Node" action is triggered
void GraphNodeItem::onDeleteTriggered() {
    if (m_profile && m_undoStack && m_nodeIndex >= 0 && m_nodeIndex < m_profile->nodeCount()) {
        m_undoStack->push(new DeleteNodeCommand(m_profile, m_nodeIndex));
    } else {
        qWarning() << "Cannot delete node, profile, undo stack, or index" << m_nodeIndex << "is invalid.";
    }
}

