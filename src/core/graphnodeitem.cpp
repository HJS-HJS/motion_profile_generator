#include "graphnodeitem.h"
#include "grapheditorview.h" // Needed for view properties and constants
#include "motionmodels.h"     // Needed for MotorProfile definition
#include "commands.h"         // For undo commands
#include <QUndoStack>
#include <QGraphicsSceneMouseEvent> // Use scene mouse event
#include <QGraphicsSceneContextMenuEvent> // <-- Correct event type
#include <QMenu>
#include <QPen>
#include <QBrush>
#include <QDebug>
#include <qmath.h> // qRound, qMax, qBound

/**
 * @brief Helper function to calculate the motor-specific visual Y scale factor.
 * @param profile The motor profile to calculate the scale for.
 * @param visualYTarget The scene Y coordinate that the max real value should map to.
 * @return The calculated scale factor (scene_Y / real_Y).
 */
inline qreal getMotorVisualScale(MotorProfile* profile, double visualYTarget) {
    if (!profile) {
        // Fallback: Assume 100 is the default real range
        return (visualYTarget > 1e-9) ? visualYTarget / 100.0 : 1.0;
    }
    double max_abs_real = qMax(qAbs(profile->yMax()), qAbs(profile->yMin()));
    // Prevent division by zero if limits are both zero
    if (max_abs_real < 1e-9) {
         // Fallback to default 100:1 scale if range is 0
         return (visualYTarget > 1e-9) ? visualYTarget / 100.0 : 1.0;
    }
    // Calculate scale: Target visual height (visualYTarget) / Actual max absolute value
    return visualYTarget / max_abs_real;
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
    // Visual setup: 20x20 pixel circle
    setRect(-10, -10, 20, 20); // Node diameter 20px
    QPen borderPen(Qt::black, 1);
    borderPen.setCosmetic(true); // Keep border 1 pixel wide
    setPen(borderPen);
    setBrush(QBrush(profile ? profile->color() : Qt::gray)); // Use profile color

    // Interaction flags
    setFlag(QGraphicsItem::ItemIsMovable);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges); // Needed for itemChange()
    setFlag(QGraphicsItem::ItemIsSelectable);
    setFlag(QGraphicsItem::ItemIgnoresTransformations); // Keep visual size constant

    // Set initial position based on model data and motor-specific scale
    if (m_profile && m_view && m_nodeIndex >= 0 && m_nodeIndex < m_profile->nodeCount()) {
        MotionNode realNode = m_profile->nodeAt(m_nodeIndex);
        qreal motorScale = getMotorVisualScale(m_profile, m_view->getReferenceYValue()); // Pass reference Y
        setPos(realNode.x(), realNode.y() * motorScale); // Apply scale to Y coordinate
    } else {
        qWarning() << "GraphNodeItem created with invalid profile, view, or index.";
        setPos(0,0); // Default position
    }
}

// Record starting position on left mouse press
void GraphNodeItem::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        m_dragStartPosition = pos(); // Record current scene position
    }
    QGraphicsEllipseItem::mousePressEvent(event); // Call base class
}

// Create and push MoveNodeCommand on mouse release after dragging
void GraphNodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    QGraphicsEllipseItem::mouseReleaseEvent(event); // Call base class with correct type

    // Only process if left button was released and position changed
    if (event->button() == Qt::LeftButton && pos() != m_dragStartPosition) {
        // Ensure necessary pointers are valid
        if (!m_profile || !m_undoStack || !m_view) return;

        // Verify the node index is still valid within the profile
        if (m_nodeIndex < 0 || m_nodeIndex >= m_profile->nodeCount()) {
             qWarning() << "Node index" << m_nodeIndex << "is invalid on mouse release.";
             setPos(m_dragStartPosition); // Optionally snap back
             return; // Don't push command if index is bad
        }

        // Get the definitive 'old' position from the model (real coords)
        QPointF oldRealPos = m_profile->nodeAt(m_nodeIndex);

        // Get the current scene position (which is scaled)
        QPointF newScenePos = pos();

        // Convert the new scene Y position back to the real Y value using motor-specific scale
        qreal motorScale = getMotorVisualScale(m_profile, m_view->getReferenceYValue()); // Pass reference Y
        if (qAbs(motorScale) < 1e-9) motorScale = 1.0; // Avoid division by zero
        QPointF newRealPos(newScenePos.x(), newScenePos.y() / motorScale);

        // Clamp newRealPos Y value based on profile limits
        newRealPos.setY(qBound(m_profile->yMin(), newRealPos.y(), m_profile->yMax()));


        // Push the move command with REAL coordinates
        m_undoStack->push(new MoveNodeCommand(m_profile, m_nodeIndex, oldRealPos, newRealPos));
    }
}

// Show context menu (Delete Node) on right-click
void GraphNodeItem::contextMenuEvent(QGraphicsSceneContextMenuEvent *event) {
    // Check if item is selected and pointers are valid
    if (!isSelected() || !m_profile || !m_undoStack) {
        event->ignore();
        return;
    }
    // Check if index is valid
    if (m_nodeIndex < 0 || m_nodeIndex >= m_profile->nodeCount()) {
         qWarning() << "Node index" << m_nodeIndex << "is invalid for context menu.";
         event->ignore();
         return;
     }

    QMenu menu;
    QAction* deleteAction = menu.addAction("Delete Node"); // Action text
    connect(deleteAction, &QAction::triggered, this, &GraphNodeItem::onDeleteTriggered); // Connect action to slot

    menu.exec(event->screenPos()); // Show menu at cursor position
    event->accept(); // Event handled
}

// Handles position changes during dragging (snapping, constraints)
QVariant GraphNodeItem::itemChange(GraphicsItemChange change, const QVariant& value) {
    // Only process position changes when the item is in a scene and pointers are valid
    if (change == ItemPositionChange && scene() && m_profile && m_view) {
        QPointF newScenePos = value.toPointF(); // Proposed new position in scene coordinates

        // Apply snapping if enabled
        if (m_view->isSnapEnabled()) {
            double gridX = m_view->gridSizeX(); // Logical X grid size

            // Calculate Y grid spacing in scene coordinates
            double gridY_scene = m_view->gridSizeY(); // Use the view's calculated scene grid size

            if (gridX > 0) {
                newScenePos.setX(qRound(newScenePos.x() / gridX) * gridX); // Snap X
            }
            if (gridY_scene > 0) {
                 newScenePos.setY(qRound(newScenePos.y() / gridY_scene) * gridY_scene); // Snap Y using scene grid step
            }
        }

        // Apply basic constraint: X cannot be negative
        newScenePos.setX(qMax(0.0, newScenePos.x()));

        // Apply Y constraints based on profile limits, converted to scene coordinates using motor scale
        qreal motorScale = getMotorVisualScale(m_profile, m_view->getReferenceYValue()); // Pass reference Y
        if (qAbs(motorScale) < 1e-9) motorScale = 1.0; // Avoid division by zero
        double sceneYMin = m_profile->yMin() * motorScale; // Calculate min Y in scene coords
        double sceneYMax = m_profile->yMax() * motorScale; // Calculate max Y in scene coords
        // Clamp the proposed scene Y position within these limits
        double clampedSceneY = qBound(sceneYMin, newScenePos.y(), sceneYMax);
        newScenePos.setY(clampedSceneY);

        return newScenePos; // Return the adjusted scene position
    }
    // For other changes, call the base class implementation
    return QGraphicsItem::itemChange(change, value);
}

// Slot called when "Delete Node" action is triggered
void GraphNodeItem::onDeleteTriggered() {
    // Ensure pointers are valid and index is valid before pushing command
    if (m_profile && m_undoStack && m_nodeIndex >= 0 && m_nodeIndex < m_profile->nodeCount()) {
        m_undoStack->push(new DeleteNodeCommand(m_profile, m_nodeIndex));
    } else {
        qWarning() << "Cannot delete node, profile, undo stack, or index" << m_nodeIndex << "is invalid.";
    }
}
