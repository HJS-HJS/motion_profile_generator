#pragma once

#include <QGraphicsEllipseItem>
#include <QObject> 
#include <QPointF> // QPointF 사용

class MotorProfile;
class GraphEditorView; // 뷰 포인터 (스냅용)
class QUndoStack; // Undo 스택 포인터

class GraphNodeItem : public QObject, public QGraphicsEllipseItem {
    Q_OBJECT

public:
    GraphNodeItem(MotorProfile* profile, int index, 
                  GraphEditorView* view, QUndoStack* stack, 
                  QGraphicsItem* parent = nullptr);
    
    MotorProfile* profile() const { return m_profile; }
    int index() const { return m_nodeIndex; }
    void setNodeIndex(int index) { m_nodeIndex = index; } 

protected:
    void contextMenuEvent(QGraphicsSceneContextMenuEvent *event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private slots:
    void onDeleteTriggered();

private:
    MotorProfile* m_profile;
    int m_nodeIndex;
    QPointF m_dragStartPosition; // 씬 좌표 (정규화됨)
    
    GraphEditorView* m_view; // 스냅 기능용
    QUndoStack* m_undoStack; // Undo/Redo용
};