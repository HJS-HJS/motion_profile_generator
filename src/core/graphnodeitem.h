#pragma once

#include <QGraphicsEllipseItem>
#include <QObject>
#include <QPointF> // Include QPointF

// Forward declarations
class MotorProfile;
class GraphEditorView;
class QUndoStack;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneContextMenuEvent; // <-- Correct type for item event

/**
 * @brief Represents a single draggable node (QGraphicsItem) in the view.
 * Handles its own visual appearance, interaction, and coordinate conversion.
 */
class GraphNodeItem : public QObject, public QGraphicsEllipseItem {
    Q_OBJECT

public:
    /**
     * @brief Constructs a new GraphNodeItem.
     * @param profile The data model (MotorProfile) this item represents.
     * @param index The index of the specific node within the profile.
     * @param view The parent GraphEditorView, used for scaling and snapping info.
     * @param stack The undo stack for pushing move/delete commands.
     * @param parent The parent QGraphicsItem (usually null).
     */
    GraphNodeItem(MotorProfile* profile, int index,
                  GraphEditorView* view, QUndoStack* stack,
                  QGraphicsItem* parent = nullptr);

    // Getters
    MotorProfile* profile() const { return m_profile; }
    int index() const { return m_nodeIndex; }
    // Setter needed because index can change when other nodes are added/removed
    void setNodeIndex(int index) { m_nodeIndex = index; }

protected:
    // Event handlers for interaction
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override; // <-- Correct type
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    // Handles snapping and position constraints during moves
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private slots:
    // Triggered by the context menu's delete action
    void onDeleteTriggered();

private:
    MotorProfile* m_profile; // Pointer to the data
    int m_nodeIndex;         // Index within the profile's node list
    QPointF m_dragStartPosition; // Position where dragging started (scene coords)

    // Pointers to other objects for interaction
    GraphEditorView* m_view; // To access view properties (e.g., scale, snap grid)
    QUndoStack* m_undoStack; // To push undo commands
};
