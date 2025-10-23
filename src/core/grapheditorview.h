#pragma once

#include <QGraphicsView>
#include <QMap>
#include <QList>
#include <qmath.h> // qFloor, qBound
#include <QGraphicsItem> // QGraphicsItem

class MotionDocument;
class MotorProfile;
class QUndoStack; 
class QKeyEvent; 

class GraphEditorView : public QGraphicsView {
    Q_OBJECT

public:
    explicit GraphEditorView(QWidget* parent = nullptr);
    void setDocument(MotionDocument* doc);
    
    void setUndoStack(QUndoStack* stack) { m_undoStack = stack; }
    
    bool isSnapEnabled() const { return m_snapToGrid; }
    double gridSize() const { return m_gridSizeX; } // X축 그리드 크기(ms) 반환

public slots:
    void fitToView(); 
    void toggleSnapToGrid(bool checked) { m_snapToGrid = checked; }
    void fitToActiveMotor(MotorProfile* profile);

signals:
    void nodeSelectionChanged(QGraphicsItem* selectedNode);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override; 
    void drawBackground(QPainter* painter, const QRectF& rect) override;

private slots:
    void onDocumentCleared();
    void onMotorAdded(MotorProfile* profile);
    void onActiveMotorChanged(MotorProfile* active, MotorProfile* previous);
    void onProfileDataChanged();
    void onProfileConstraintsChanged();
    void updateConstraintItems(MotorProfile* profile);
    void onSceneSelectionChanged();

private:
    void rebuildProfileItems(MotorProfile* profile);
    void updateProfileVisibility(MotorProfile* profile, bool isActive);
    void clearAllProfileItems();

    QMap<MotorProfile*, QList<QGraphicsItem*>> m_constraintItems; 
    void setConstraintItemsVisible(MotorProfile* profile, bool visible);

    QUndoStack* m_undoStack = nullptr; 
    
    bool m_snapToGrid = false;
    // 3번: X축 50ms, Y축 20칸 (씬 좌표 기준)
    double m_gridSizeX = 50.0; // 50ms
    double m_gridSizeY = (NORMALIZED_Y_MAX - NORMALIZED_Y_MIN) / 20.0; // 5.0
    
    QGraphicsScene* m_scene;
    MotionDocument* m_document = nullptr;
    QMap<MotorProfile*, QList<QGraphicsItem*>> m_profileItems;
    bool m_isPanning = false;
    QPoint m_panStartPos;
};