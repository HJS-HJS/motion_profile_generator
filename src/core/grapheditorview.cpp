#include "grapheditorview.h"
#include "motionmodels.h"
#include "graphnodeitem.h"
#include "commands.h"
#include <QUndoStack>
#include <QKeyEvent>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QScrollBar>
#include <QGraphicsLineItem>
#include <QPen>
#include <QBrush>
#include <QDebug>
#include <QTransform>
#include <QPainter>
#include <QtMath>
#include <QApplication> // For RubberBandDrag cursor

/**
 * @brief Helper function to calculate the motor-specific visual Y scale factor.
 */
inline qreal getMotorVisualScale(MotorProfile* profile, double referenceYValue) {
    qreal effDefYScale = (referenceYValue > 1e-9) ? VISUAL_Y_TARGET / referenceYValue : 1.0;
    if (!profile) return effDefYScale / 100.0;
    double max_abs_real = qMax(qAbs(profile->yMax()), qAbs(profile->yMin()));
    if (max_abs_real < 1e-9) return effDefYScale / 100.0;
    return referenceYValue / max_abs_real;
}


GraphEditorView::GraphEditorView(QWidget* parent)
    : QGraphicsView(parent), m_scene(new QGraphicsScene(this)),
      m_isPanning(false), m_numYDivisions(10),
      m_referenceYValue(DEFAULT_REFERENCE_Y),
      m_gridLargeSizeX(1000.0),
      m_gridSizeY(m_referenceYValue / 10.0)
{
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing);
    setDragMode(QGraphicsView::NoDrag); // <<< RubberBandDrag -> NoDrag
    setTransformationAnchor(AnchorUnderMouse);
    setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_scene->setSceneRect(-1000000, -1000000, 2000000, 2000000);
    setFocusPolicy(Qt::StrongFocus);
    scale(1, -1);
    connect(m_scene, &QGraphicsScene::selectionChanged, this, &GraphEditorView::onSceneSelectionChanged);
}

void GraphEditorView::setDocument(MotionDocument* doc) {
    if (m_document) {
        disconnect(m_document, nullptr, this, nullptr);
        for (MotorProfile* profile : m_document->motorProfiles()) {
             if (profile) disconnect(profile, nullptr, this, nullptr);
        }
    }
    m_document = doc;
    clearAllProfileItems();
    if (!m_document) {
        update();
        return;
    }
    connect(m_document, &MotionDocument::documentCleared, this, &GraphEditorView::onDocumentCleared, Qt::UniqueConnection);
    connect(m_document, &MotionDocument::motorAdded, this, &GraphEditorView::onMotorAdded, Qt::UniqueConnection);
    connect(m_document, &MotionDocument::activeMotorChanged, this, &GraphEditorView::onActiveMotorChanged, Qt::UniqueConnection);
    connect(m_document, &MotionDocument::activeMotorChanged, this, QOverload<>::of(&GraphEditorView::update), Qt::UniqueConnection);
    for(MotorProfile* profile : m_document->motorProfiles()) {
        if(profile) onMotorAdded(profile);
    }
    onActiveMotorChanged(m_document->activeProfile(), nullptr);
}

// --- Event Overrides ---
void GraphEditorView::wheelEvent(QWheelEvent* event) {
    double scaleFactor = (event->angleDelta().y() > 0) ? 1.15 : (1.0 / 1.15);
    scale(scaleFactor, 1.0);
    update();
}

void GraphEditorView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        m_isPanning = true;
        m_panStartPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    
    // <<< 다중 이동 로직 삭제, 기본 동작(아이템 선택)으로 되돌림 >>>
    QGraphicsView::mousePressEvent(event);
}

void GraphEditorView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }

    // <<< 다중 이동 Undo 로직 삭제 >>>
    
    QGraphicsView::mouseReleaseEvent(event);
}

void GraphEditorView::mouseMoveEvent(QMouseEvent* event) {
    if (m_isPanning) {
        QPoint delta = event->pos() - m_panStartPos;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        m_panStartPos = event->pos();
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void GraphEditorView::contextMenuEvent(QContextMenuEvent* event) {
    if (!m_document || !m_document->activeProfile() || !m_undoStack) return;
    QPointF scenePos = mapToScene(event->pos());
    MotorProfile* activeProfile = m_document->activeProfile();
    QMenu menu;
    qreal motorScale = getMotorVisualScale(activeProfile, m_referenceYValue);
    if (qAbs(motorScale) < 1e-9) motorScale = 1.0;
    double realY = scenePos.y() / motorScale;
    QAction* addAction = menu.addAction("Add New Node at ( " +
        QString::number(scenePos.x(), 'f', 1) + ", " +
        QString::number(realY, 'f', 3) + " )");
    if (scenePos.x() < 0) {
        addAction->setEnabled(false);
        addAction->setText(addAction->text() + " (X < 0 not allowed)");
    } else if (realY < activeProfile->yMin() || realY > activeProfile->yMax()) {
        addAction->setEnabled(false);
        addAction->setText(addAction->text() + " (Y-axis limit exceeded)");
    }
    connect(addAction, &QAction::triggered, this, [=]() {
        QPointF realPos(scenePos.x(), realY);
        m_undoStack->push(new AddNodeCommand(activeProfile, realPos));
    });
    menu.exec(event->globalPos());
}

void GraphEditorView::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete) {
        if (!m_document || !m_document->activeProfile() || !m_undoStack) return;
        
        // <<< 다중 삭제 로직 삭제 -> 단일 삭제로 변경 >>>
        auto selectedItems = m_scene->selectedItems();
        if (selectedItems.size() != 1) return; // 오직 하나만 선택됐을 때만 삭제

        if (auto nodeItem = qgraphicsitem_cast<GraphNodeItem*>(selectedItems.first())) {
            if(nodeItem->profile() == m_document->activeProfile()) {
                // 단일 삭제 Command 푸시
                m_undoStack->push(new DeleteNodeCommand(nodeItem->profile(), nodeItem->index()));
                event->accept();
            }
        }
    } else {
        QGraphicsView::keyPressEvent(event);
    }
}

// Draw grid, axes, labels, and limit lines
void GraphEditorView::drawBackground(QPainter* painter, const QRectF& rect) {
    QGraphicsView::drawBackground(painter, rect);

    QPen gridPen(QColor(220, 220, 220), 0);
    gridPen.setCosmetic(true);
    QPen axisPen(QColor(150, 150, 150), 2);
    axisPen.setCosmetic(true);
    QPen majorGridPen = axisPen;

    MotorProfile* activeProfile = m_document ? m_document->activeProfile() : nullptr;
    qreal visualYTarget = m_referenceYValue;
    qreal effDefYScale = (m_referenceYValue > 1e-9) ? visualYTarget / m_referenceYValue : 1.0;
    if (qAbs(effDefYScale) < 1e-9) effDefYScale = 1.0;

    painter->setPen(gridPen);
    if (m_gridSizeX > 1e-6) {
        double left_x = qFloor(rect.left() / m_gridSizeX) * m_gridSizeX;
        for (double x = left_x; x < rect.right(); x += m_gridSizeX) {
            bool isMajor = (m_gridLargeSizeX > 1e-6) && (qAbs(fmod(x, m_gridLargeSizeX)) < 1e-6);
            if (qAbs(x) < 1e-6) {
                 painter->setPen(gridPen);
            } else {
                 painter->setPen(isMajor ? majorGridPen : gridPen);
            }
            painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
        }
    }

    painter->setPen(gridPen);
    m_gridSizeY = (m_numYDivisions > 0) ? visualYTarget / (m_numYDivisions) : 50.0;
    if(qAbs(m_gridSizeY) < 1e-6) m_gridSizeY = 50.0;
    double top_y_scene = qFloor(rect.top() / m_gridSizeY) * m_gridSizeY;
    for (double y_scene = top_y_scene; y_scene < rect.bottom(); y_scene += m_gridSizeY) {
        if(qAbs(y_scene) > 1e-6) {
            painter->drawLine(QPointF(rect.left(), y_scene), QPointF(rect.right(), y_scene));
        }
    }

    painter->setPen(axisPen);
    painter->drawLine(QPointF(0, rect.top()), QPointF(0, rect.bottom()));
    painter->drawLine(QPointF(rect.left(), 0), QPointF(rect.right(), 0));

    painter->save();
    painter->scale(1, -1);
    painter->setPen(QPen(Qt::black));
    QFont font = painter->font();
    qreal sx = transform().m11();
    qreal sy = transform().m22();
    qreal scaleFactor = 1.0;
    if (qAbs(sx * sy) > 1e-9) {
         scaleFactor = 1.0 / qSqrt(qAbs(sx * sy));
    }
    font.setPointSizeF(8 * scaleFactor);
    painter->setFont(font);

    double xLabelInterval = 250.0;
    double startXLabel = qFloor(rect.left() / xLabelInterval) * xLabelInterval;
    for (double x = startXLabel; x < rect.right(); x += xLabelInterval) {
         if (qAbs(x) > 1e-3 || qAbs(xLabelInterval - (qAbs(rect.left()) + qAbs(rect.right()))) < 1e-3) {
             QRectF textRect(x - 50 * scaleFactor, 2 * scaleFactor, 100 * scaleFactor, 20 * scaleFactor);
             painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, QString::number(x, 'f', 0));
        }
    }

    const int numYLabels = m_numYDivisions * 2 + 1;
    double yLabelMin_real = -m_referenceYValue;
    double yLabelMax_real = +m_referenceYValue;
    if (activeProfile) {
        double currentYMaxAbs = qMax(qAbs(activeProfile->yMax()), qAbs(activeProfile->yMin()));
        if(currentYMaxAbs < 1e-6) currentYMaxAbs = m_referenceYValue;
        yLabelMin_real = -currentYMaxAbs;
        yLabelMax_real = +currentYMaxAbs;
    }
    double yRange_real = yLabelMax_real - yLabelMin_real;

    if (qAbs(yRange_real) > 1e-6 && numYLabels > 1) {
        double yLabelStep_real = yRange_real / (numYLabels - 1);
        for (int i = 0; i < numYLabels; ++i) {
            double y_real = yLabelMin_real + i * yLabelStep_real;
            double y_scene_grid = (i - m_numYDivisions) * m_gridSizeY;
            if (y_scene_grid >= rect.top() - qAbs(m_gridSizeY) && y_scene_grid <= rect.bottom() + qAbs(m_gridSizeY)) {
                if (qAbs(y_scene_grid) < 1e-3 && i != m_numYDivisions) {
                   if (qAbs(m_gridSizeY) < 20 * scaleFactor) continue;
                }
                QString label = QString::number(y_real, 'f', 1);
                QRectF textRect(-52 * scaleFactor, -y_scene_grid - 10 * scaleFactor, 50 * scaleFactor, 20 * scaleFactor);
                painter->drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, label);
            }
        }
    }

    if (activeProfile) {
        double realYMin = activeProfile->yMin();
        double realYMax = activeProfile->yMax();
        qreal motorScale = getMotorVisualScale(activeProfile, m_referenceYValue);
        double sceneYMin = realYMin * motorScale;
        double sceneYMax = realYMax * motorScale;
        QColor motorColor = activeProfile->color();
        QColor lineColor = motorColor.lighter(130);
        lineColor.setAlpha(180);
        QColor textColor = motorColor.darker(130);
        QPen limitPen(lineColor, 2);
        limitPen.setCosmetic(true);
        painter->setPen(limitPen);
        painter->drawLine(QPointF(rect.left(), -sceneYMax), QPointF(rect.right(), -sceneYMax));
        painter->setPen(textColor);
        QString maxLabel = QString::number(realYMax, 'f', 1);
        QRectF maxLabelRect(-52 * scaleFactor, -sceneYMax - 10 * scaleFactor, 50 * scaleFactor, 20 * scaleFactor);
        painter->drawText(maxLabelRect, Qt::AlignRight | Qt::AlignVCenter, maxLabel);
        painter->setPen(limitPen);
        painter->drawLine(QPointF(rect.left(), -sceneYMin), QPointF(rect.right(), -sceneYMin));
        painter->setPen(textColor);
        QString minLabel = QString::number(realYMin, 'f', 1);
        QRectF minLabelRect(-52 * scaleFactor, -sceneYMin - 10 * scaleFactor, 50 * scaleFactor, 20 * scaleFactor);
        painter->drawText(minLabelRect, Qt::AlignRight | Qt::AlignVCenter, minLabel);
    }
    painter->restore();
}

// --- Slot Implementations ---
void GraphEditorView::onDocumentCleared() {
    clearAllProfileItems();
    update();
}

void GraphEditorView::clearAllProfileItems() {
    for(const auto& items : m_profileItems) {
        qDeleteAll(items);
    }
    m_profileItems.clear();
}

void GraphEditorView::onMotorAdded(MotorProfile* profile) {
    if (!profile || m_profileItems.contains(profile)) return;
    m_profileItems.insert(profile, QList<QGraphicsItem*>());
    rebuildProfileItems(profile);
    updateProfileVisibility(profile, (profile == (m_document ? m_document->activeProfile() : nullptr) ));
    connect(profile, &MotorProfile::dataChanged, this, &GraphEditorView::onProfileDataChanged, Qt::UniqueConnection);
    connect(profile, &MotorProfile::constraintsChanged, this, &GraphEditorView::onProfileConstraintsChanged, Qt::UniqueConnection);
}

void GraphEditorView::onActiveMotorChanged(MotorProfile* active, MotorProfile* previous) {
    Q_UNUSED(previous);
    if(m_document) {
        for(MotorProfile* profile : m_document->motorProfiles()){
            if(profile) {
                 updateProfileVisibility(profile, profile == active);
            }
        }
    }
    update();
}

void GraphEditorView::onProfileDataChanged() {
    MotorProfile* profile = qobject_cast<MotorProfile*>(sender());
    if (!profile) return;
    
    GraphNodeItem* nodeToReselect = nullptr;
    QMap<GraphNodeItem*, QPointF> oldPositions;
    if(m_scene->selectedItems().count() == 1) {
        if (auto node = qgraphicsitem_cast<GraphNodeItem*>(m_scene->selectedItems().first())) {
            if (node->profile() == profile) {
                 oldPositions[node] = node->pos();
            }
        }
    }

    rebuildProfileItems(profile);
    updateProfileVisibility(profile, (profile == (m_document ? m_document->activeProfile() : nullptr) ));

    if(oldPositions.count() == 1) {
        GraphNodeItem* oldNode = oldPositions.firstKey();
        QPointF oldPos = oldPositions.first();
        qreal minDist = 1e-2;
        if (m_profileItems.contains(profile)){
            for (auto item : m_profileItems.value(profile)) {
                if (auto node = qgraphicsitem_cast<GraphNodeItem*>(item)) {
                     if(node->index() == oldNode->index()) {
                         nodeToReselect = node;
                         break;
                     }
                     qreal dist = (node->pos() - oldPos).manhattanLength();
                     if(dist < minDist) {
                         minDist = dist;
                         nodeToReselect = node;
                     }
                }
            }
        }
    }
    
    m_scene->clearSelection();
    if (nodeToReselect) {
         nodeToReselect->setSelected(true);
    }
    emit nodeSelectionChanged(nodeToReselect);
    update();
}

void GraphEditorView::onProfileConstraintsChanged() {
    MotorProfile* profile = qobject_cast<MotorProfile*>(sender());
    if (profile) {
         rebuildProfileItems(profile);
    }
    if (profile && profile == (m_document ? m_document->activeProfile() : nullptr) ) {
        update();
     }
}


void GraphEditorView::rebuildProfileItems(MotorProfile* profile) {
    if (!profile) return;
    if (m_profileItems.contains(profile)) {
        qDeleteAll(m_profileItems.value(profile));
        m_profileItems[profile].clear();
    } else {
        m_profileItems.insert(profile, QList<QGraphicsItem*>());
    }

    QList<QGraphicsItem*>& items = m_profileItems[profile];
    const auto& nodes = profile->nodes();
    QColor color = profile->color();
    qreal motorScale = getMotorVisualScale(profile, m_referenceYValue);
    if (qAbs(motorScale) < 1e-9) motorScale = 1.0;

    for (int i = 0; i < nodes.size() - 1; ++i) {
        QPen linePen(color, 2);
        linePen.setCosmetic(true);
        const MotionNode& prevNode = nodes[i];
        const MotionNode& currNode = nodes[i+1];
        double deltaX = currNode.x() - prevNode.x();
        if (qAbs(deltaX) > 1e-6) {
            double deltaY = currNode.y() - prevNode.y();
            double realSlope = deltaY / deltaX;
            double maxSlopeLimit = profile->maxSlope();
            if (maxSlopeLimit > 0 && qAbs(realSlope) > maxSlopeLimit) {
                linePen.setStyle(Qt::DashLine);
            }
        }
        QGraphicsLineItem* line = m_scene->addLine(
            QLineF(prevNode.x(), prevNode.y() * motorScale,
                   currNode.x(), currNode.y() * motorScale),
            linePen);
        items.append(line);
    }
    for (int i = 0; i < nodes.size(); ++i) {
        GraphNodeItem* nodeItem = new GraphNodeItem(profile, i, this, m_undoStack);
        m_scene->addItem(nodeItem);
        items.append(nodeItem);
    }
}

void GraphEditorView::rebuildAllItems() {
    if (!m_document) return;
    clearAllProfileItems();
    MotorProfile* active = m_document ? m_document->activeProfile() : nullptr;
    for(MotorProfile* p : m_document->motorProfiles()){
        if(p) {
            m_profileItems.insert(p, QList<QGraphicsItem*>());
            rebuildProfileItems(p);
            updateProfileVisibility(p, p == active);
        }
    }
    m_scene->clearSelection();
    emit nodeSelectionChanged(nullptr);
}


void GraphEditorView::updateProfileVisibility(MotorProfile* profile, bool isActive) {
    if (!profile || !m_profileItems.contains(profile)) return;
    QList<QGraphicsItem*>& items = m_profileItems[profile];
    QColor color = profile->color();
    qreal opacity;
    int zValue;
    if (isActive) {
        opacity = 1.0;
        zValue = 1;
    } else {
        color.setAlpha(80);
        opacity = 0.6;
        zValue = 0;
    }
    for (QGraphicsItem* item : items) {
        if (!item) continue;
        item->setZValue(zValue);
        item->setOpacity(opacity);
        item->setEnabled(isActive);
        if (auto line = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
            QPen currentPen = line->pen();
            currentPen.setColor(color);
            currentPen.setCosmetic(true);
            line->setPen(currentPen);
        } else if (auto node = qgraphicsitem_cast<GraphNodeItem*>(item)) {
            node->setBrush(QBrush(color));
            QPen nodePen(isActive ? Qt::black : color.darker(120), 1);
            nodePen.setCosmetic(true);
            node->setPen(nodePen);
        }
    }
}


// --- Fitting Logic Helper ---
void GraphEditorView::applyFitting(double xMin, double xMax) {
    const double minWidth = 2000.0;
    if (xMax <= xMin) xMax = xMin + minWidth;
    if ((xMax - xMin) < minWidth) {
        double midX = (xMin + xMax) / 2.0;
        xMin = midX - minWidth / 2.0;
        xMax = midX + minWidth / 2.0;
    }

    double sceneYMax = m_referenceYValue;
    double sceneYMin = -m_referenceYValue;
    if (qAbs(sceneYMax - sceneYMin) < 1e-6) {
        sceneYMax = 100.0; // Fallback
        sceneYMin = -100.0;
    }

    QRectF targetSceneRect(xMin, sceneYMax, xMax - xMin, sceneYMin - sceneYMax );
    resetTransform();
    scale(1, -1);
    double yMarginFactor = 0.02;
    double xMarginFactor = 0.10;
    double yMargin = qAbs(targetSceneRect.height()) * yMarginFactor;
    double xMargin = qAbs(targetSceneRect.width()) * xMarginFactor;
    fitInView(targetSceneRect.marginsAdded(QMarginsF(xMargin, yMargin, xMargin, yMargin)), Qt::KeepAspectRatio);
    centerOn(targetSceneRect.left() + targetSceneRect.width() / 2.0, 0);
    update();
}

// --- Public Fitting Slots ---
void GraphEditorView::fitToView() {
    double xMin = -100.0;
    double xMax = 2000.0;
    const double minWidth = 2000.0;

    if (m_document && m_document->activeProfile()) {
         MotorProfile* profile = m_document->activeProfile();
         if(!profile->nodes().isEmpty()){
            xMin = profile->nodes().first().x();
            xMax = profile->nodes().last().x();
            for(const auto& node : profile->nodes()){
                if(node.x() < xMin) xMin = node.x();
                if(node.x() > xMax) xMax = node.x();
            }
             if (xMax < xMin + minWidth) xMax = xMin + minWidth;
             xMin = qMin(xMin, -100.0);
         }
    } else if (!m_scene->items().isEmpty()) {
        QRectF itemsRect = m_scene->itemsBoundingRect();
        xMin = itemsRect.left();
        xMax = itemsRect.right();
        if (xMax < xMin + minWidth) xMax = xMin + minWidth;
        xMin = qMin(xMin, -100.0);
    }
    applyFitting(xMin, xMax);
}

void GraphEditorView::fitToActiveMotor(MotorProfile* profile)
{
    if (!profile) {
        fitToView();
        return;
    }
    double xMin = -100.0;
    double xMax = 0;
    const double minWidth = 2000.0;
    bool nodes_exist = !profile->nodes().isEmpty();
    if (nodes_exist) {
        xMin = profile->nodes().first().x();
        xMax = profile->nodes().last().x();
        for (const auto& node : profile->nodes()) {
            if (node.x() < xMin) xMin = node.x();
            if (node.x() > xMax) xMax = node.x();
        }
         xMin = qMin(xMin, -100.0);
    } else {
         xMin = -100.0;
         xMax = 2000.0;
    }
     if (xMax < xMin + minWidth) xMax = xMin + minWidth;

    applyFitting(xMin, xMax);
}

// Slot called by View Options dock spinbox (Y Divisions)
void GraphEditorView::setNumYDivisions(int divisions) {
    if (divisions < 1) divisions = 1;
    if (divisions != m_numYDivisions) {
        m_numYDivisions = divisions;
        update();
    }
}

// Slot called by View Options dock spinbox (Reference Y)
void GraphEditorView::setReferenceYValue(double value) {
    if (value > 0 && qAbs(m_referenceYValue - value) > 1e-6) {
        m_referenceYValue = value;
        rebuildAllItems();
        update();
        // Re-apply current X range fit
        QRectF currentSceneRect = mapToScene(viewport()->rect()).boundingRect();
        applyFitting(currentSceneRect.left(), currentSceneRect.right());
    }
}

// Slot called by View Options dock spinbox (Grid Size X)
void GraphEditorView::setGridSizeX(double size) {
     if (size > 0 && qAbs(m_gridSizeX - size) > 1e-6) {
         m_gridSizeX = size;
         update();
     }
}

// Slot called by View Options dock spinbox (Major Grid X)
void GraphEditorView::setGridLargeSizeX(double size) {
     if (size > 0 && qAbs(m_gridLargeSizeX - size) > 1e-6) {
         m_gridLargeSizeX = size;
         update();
     }
}

// <<< setXRange slot (no longer connected) - REMOVED >>>
// void GraphEditorView::setXRange(double minX, double maxX) { ... }


// Slot connected to scene selection changes
void GraphEditorView::onSceneSelectionChanged()
{
    auto selected = m_scene->selectedItems();
    QGraphicsItem* selectedNodeItem = nullptr;

    if (selected.size() == 1) { // Only update node panel if *one* item is selected
        selectedNodeItem = qgraphicsitem_cast<GraphNodeItem*>(selected.first());
    }
    
    emit nodeSelectionChanged(selectedNodeItem);
}

