#pragma once

#include <QGraphicsEllipseItem>
#include <QObject>
#include <QPointF> // Include QPointF

// Forward declarations
class MotorProfile;
class GraphEditorView;
class QUndoStack;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneContextMenuEvent;

/**
 * @brief Represents a single draggable node (QGraphicsItem) in the view.
 * Handles its own visual appearance, interaction (context menu, snapping),
 * and coordinate conversion.
 */
class GraphNodeItem : public QObject, public QGraphicsEllipseItem {
    Q_OBJECT

public:
    GraphNodeItem(MotorProfile* profile, int index,
                  GraphEditorView* view, QUndoStack* stack,
                  QGraphicsItem* parent = nullptr);

    // Getters
    MotorProfile* profile() const { return m_profile; }
    int index() const { return m_nodeIndex; }
    void setNodeIndex(int index) { m_nodeIndex = index; }

protected:
    // Event handlers for interaction
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    // <<< 단일 드래그를 위해 이벤트 핸들러 복원 >>>
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private slots:
    void onDeleteTriggered();

private:
    MotorProfile* m_profile;
    int m_nodeIndex;
    // <<< 단일 드래그 시작 위치 저장을 위해 복원 >>>
    QPointF m_dragStartPosition; 
    
    GraphEditorView* m_view;
    QUndoStack* m_undoStack;
};

