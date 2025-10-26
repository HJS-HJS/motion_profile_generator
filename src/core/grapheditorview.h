#pragma once

#include <QGraphicsView>
#include <QMap>
#include <QList>
#include <qmath.h> // qFloor, qBound, qMax, qAbs, qSqrt, fmod
#include <QGraphicsItem> // Base class for items

// Forward declarations
class MotionDocument;
class MotorProfile;
class QUndoStack;
class QKeyEvent;
class QPainter;
class QWheelEvent;
class QMouseEvent;
class QContextMenuEvent; // <-- Correct type for QWidget event
class GraphNodeItem; // Added forward declaration

// Constants for visual scaling and default behavior
const qreal VISUAL_Y_TARGET = 300.0; // The scene Y-coordinate corresponding to the reference Y value
const qreal DEFAULT_REFERENCE_Y = 100.0; // Default reference Y value

// Main view class
class GraphEditorView : public QGraphicsView {
    Q_OBJECT

public:
    explicit GraphEditorView(QWidget* parent = nullptr);

    // Setters
    void setDocument(MotionDocument* doc);
    void setUndoStack(QUndoStack* stack) { m_undoStack = stack; }

    // Getters
    bool isSnapEnabled() const { return m_snapToGrid; }
    double gridSizeX() const { return m_gridSizeX; }
    double gridSizeY() const { return m_gridSizeY; }
    int getNumYDivisions() const { return m_numYDivisions; }
    double getReferenceYValue() const { return m_referenceYValue; }
    double getMajorGridSizeX() const { return m_gridLargeSizeX; }

public slots:
    // View control
    void fitToView();
    void toggleSnapToGrid(bool checked) { m_snapToGrid = checked; update(); }
    void fitToActiveMotor(MotorProfile* profile);

    // Slots for external control
    void setNumYDivisions(int divisions);
    // void setXRange(double minX, double maxX); // Removed
    void setReferenceYValue(double value);
    void setGridSizeX(double size);
    void setGridLargeSizeX(double size);


signals:
    void nodeSelectionChanged(QGraphicsItem* selectedNode);

protected:
    // Event overrides
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override; // <-- Correct type
    void keyPressEvent(QKeyEvent* event) override;
    void drawBackground(QPainter* painter, const QRectF& rect) override;

private slots:
    void onDocumentCleared();
    void onMotorAdded(MotorProfile* profile);
    void onActiveMotorChanged(MotorProfile* active, MotorProfile* previous);
    void onProfileDataChanged();
    void onProfileConstraintsChanged();
    void onSceneSelectionChanged();

private:
    // Helper function
    void applyFitting(double xMin, double xMax);

    // Internal functions
    void rebuildProfileItems(MotorProfile* profile);
    void rebuildAllItems();
    void updateProfileVisibility(MotorProfile* profile, bool isActive);
    void clearAllProfileItems();

    // Pointers
    QUndoStack* m_undoStack = nullptr;
    MotionDocument* m_document = nullptr;
    QGraphicsScene* m_scene;

    // View state
    bool m_snapToGrid = false;
    double m_gridSizeX = 50.0;
    double m_gridSizeY = VISUAL_Y_TARGET / 10.0;
    int m_numYDivisions = 10;
    double m_referenceYValue = DEFAULT_REFERENCE_Y;
    double m_gridLargeSizeX = 1000.0;

    // Item map
    QMap<MotorProfile*, QList<QGraphicsItem*>> m_profileItems;

    // Panning state
    bool m_isPanning = false;
    QPoint m_panStartPos;
    
    // Multi-move state REMOVED
    // QMap<GraphNodeItem*, QPointF> m_dragStartPositions;
};

