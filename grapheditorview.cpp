#include "grapheditorview.h"
#include "motionmodels.h"
#include "graphnodeitem.h" // 커스텀 노드 아이템

#include <QWheelEvent>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QScrollBar>
#include <QGraphicsLineItem>
#include <QPen>
#include <QBrush>
#include <QDebug>

GraphEditorView::GraphEditorView(QWidget* parent)
    : QGraphicsView(parent), m_scene(new QGraphicsScene(this)), m_isPanning(false)
{
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing); // 부드럽게 그리기
    setDragMode(QGraphicsView::NoDrag); 
    setTransformationAnchor(AnchorUnderMouse);
    setAlignment(Qt::AlignCenter); 
    m_scene->setSceneRect(-10000, -10000, 20000, 20000); 
}

void GraphEditorView::setDocument(MotionDocument* doc) {
    if (m_document) {
        disconnect(m_document, nullptr, this, nullptr);
    }
    m_document = doc;
    if (!m_document) return;

    connect(m_document, &MotionDocument::documentCleared, this, &GraphEditorView::onDocumentCleared);
    connect(m_document, &MotionDocument::motorAdded, this, &GraphEditorView::onMotorAdded);
    connect(m_document, &MotionDocument::activeMotorChanged, this, &GraphEditorView::onActiveMotorChanged);
    
    for(MotorProfile* profile : m_document->motorProfiles()) {
        onMotorAdded(profile);
    }
    onActiveMotorChanged(m_document->activeProfile(), nullptr);
}

// 6. 확대/축소 (Qt 5 호환)
void GraphEditorView::wheelEvent(QWheelEvent* event) {
    double scaleFactor = (event->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
    scale(scaleFactor, scaleFactor);
}

// 6. 패닝 시작
void GraphEditorView::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        m_isPanning = true;
        m_panStartPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    QGraphicsView::mousePressEvent(event);
}

// 6. 패닝 종료
void GraphEditorView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}

// 6. 패닝 중 이동
void GraphEditorView::mouseMoveEvent(QMouseEvent* event) {
    if (m_isPanning) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - (event->pos().x() - m_panStartPos.x()));
        verticalScrollBar()->setValue(verticalScrollBar()->value() - (event->pos().y() - m_panStartPos.y()));
        m_panStartPos = event->pos(); 
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

// 2. 우클릭 (노드 추가)
void GraphEditorView::contextMenuEvent(QContextMenuEvent* event) {
    if (!m_document || !m_document->activeProfile()) return;

    QPointF scenePos = mapToScene(event->pos());
    MotorProfile* activeProfile = m_document->activeProfile();

    QMenu menu;
    QAction* addAction = menu.addAction("여기( " + 
        QString::number(scenePos.x(), 'f', 2) + ", " + 
        QString::number(scenePos.y(), 'f', 2) + 
        " )에 새 노드 추가");

    // 7. Y축 제약조건 검사
    if (scenePos.y() < activeProfile->yMin() || scenePos.y() > activeProfile->yMax()) {
        addAction->setEnabled(false);
        addAction->setText(addAction->text() + " (Y축 제한 위반)");
    }
    
    connect(addAction, &QAction::triggered, this, [=]() {
        if (!activeProfile->addNode(scenePos)) {
             qWarning() << "노드 추가 실패: 제약조건 위반";
        }
    });
    
    menu.exec(event->globalPos());
}

// 1. 배경 그리드
void GraphEditorView::drawBackground(QPainter* painter, const QRectF& rect) {
    QGraphicsView::drawBackground(painter, rect);
    QPen pen(QColor(220, 220, 220), 0); 
    pen.setCosmetic(true);
    painter->setPen(pen);
    double gridSize = 50.0;
    double left = qFloor(rect.left() / gridSize) * gridSize;
    double top = qFloor(rect.top() / gridSize) * gridSize;
    for (double x = left; x < rect.right(); x += gridSize) {
        painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    }
    for (double y = top; y < rect.bottom(); y += gridSize) {
        painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
    }
    painter->setPen(QPen(QColor(180, 180, 180), 0));
    painter->drawLine(QPointF(0, rect.top()), QPointF(0, rect.bottom())); // Y축
    painter->drawLine(QPointF(rect.left(), 0), QPointF(rect.right(), 0)); // X축
}

void GraphEditorView::onDocumentCleared() {
    clearAllProfileItems();
    m_profileItems.clear();
}

void GraphEditorView::clearAllProfileItems() {
    for(const auto& items : m_profileItems) {
        for(QGraphicsItem* item : items) {
            m_scene->removeItem(item);
            delete item;
        }
    }
}

void GraphEditorView::onMotorAdded(MotorProfile* profile) {
    if (!profile || m_profileItems.contains(profile)) return;
    
    m_profileItems.insert(profile, QList<QGraphicsItem*>());
    rebuildProfileItems(profile);
    updateProfileVisibility(profile, false); // 기본 비활성
    
    connect(profile, &MotorProfile::dataChanged, this, &GraphEditorView::onProfileDataChanged);
    connect(profile, &MotorProfile::constraintsChanged, this, &GraphEditorView::onProfileConstraintsChanged);
}

// 8, 9. 활성 모터 변경 시 (Qt 6 코드 버그 수정 - 인자 2개 받음)
void GraphEditorView::onActiveMotorChanged(MotorProfile* active, MotorProfile* previous) {
    if (previous) {
        updateProfileVisibility(previous, false); // 9. 이전 모터 희미하게
    }
    if (active) {
        updateProfileVisibility(active, true); // 8. 새 모터 선명하게
    }
}

// 8. 데이터 변경 시
void GraphEditorView::onProfileDataChanged() {
    MotorProfile* profile = qobject_cast<MotorProfile*>(sender());
    if (profile) {
        rebuildProfileItems(profile);
        updateProfileVisibility(profile, (profile == m_document->activeProfile()));
    }
}

// 7. 제약조건 변경 시
void GraphEditorView::onProfileConstraintsChanged() {
    MotorProfile* profile = qobject_cast<MotorProfile*>(sender());
     if (profile && profile == m_document->activeProfile()) {
        rebuildProfileItems(profile);
        updateProfileVisibility(profile, true);
     }
}

// 1, 3, 4. 프로파일 다시 그리기
void GraphEditorView::rebuildProfileItems(MotorProfile* profile) {
    if (!profile) return;
    if (m_profileItems.contains(profile)) {
        for (QGraphicsItem* item : m_profileItems.value(profile)) {
            m_scene->removeItem(item);
            delete item;
        }
        m_profileItems[profile].clear();
    }
    QList<QGraphicsItem*>& items = m_profileItems[profile];
    const auto& nodes = profile->nodes();
    QColor color = profile->color();
    QPen linePen(color, 2); 
    for (int i = 0; i < nodes.size() - 1; ++i) {
        QGraphicsLineItem* line = m_scene->addLine(QLineF(nodes[i], nodes[i+1]), linePen);
        items.append(line);
    }
    for (int i = 0; i < nodes.size(); ++i) {
        GraphNodeItem* nodeItem = new GraphNodeItem(profile, i);
        m_scene->addItem(nodeItem);
        items.append(nodeItem);
    }
}

// 9. 가시성 조절
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
        color.setAlpha(60); 
        opacity = 0.5;
        zValue = 0; 
    }
    for (QGraphicsItem* item : items) {
        item->setZValue(zValue);
        item->setOpacity(opacity);
        item->setEnabled(isActive); 
        if (auto line = qgraphicsitem_cast<QGraphicsLineItem*>(item)) {
            QPen pen = line->pen();
            pen.setColor(color);
            line->setPen(pen);
        } else if (auto node = qgraphicsitem_cast<GraphNodeItem*>(item)) {
            node->setBrush(QBrush(color));
            node->setPen(QPen(isActive ? Qt::black : color.darker(120), 1));
        }
    }
}

