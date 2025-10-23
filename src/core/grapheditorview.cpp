#include "grapheditorview.h"
#include "motionmodels.h"
#include "graphnodeitem.h"
#include "commands.h" // 커맨드
#include <QUndoStack> // Undo 스택
#include <QKeyEvent> // Delete 키
#include <QWheelEvent>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QScrollBar>
#include <QGraphicsLineItem>
#include <QGraphicsTextItem> // 축 값
#include <QPen>
#include <QBrush>
#include <QDebug>
#include <QTransform> // 1번: Y축 반전
#include <QPainter> // QPainter

GraphEditorView::GraphEditorView(QWidget* parent)
    : QGraphicsView(parent), m_scene(new QGraphicsScene(this)), m_isPanning(false)
{
    setScene(m_scene);
    setRenderHint(QPainter::Antialiasing); 
    setDragMode(QGraphicsView::NoDrag); 
    setTransformationAnchor(AnchorUnderMouse);
    // setAlignment(Qt::AlignCenter); // 2번: X=0 중앙 고정은 centerOn으로 처리
    m_scene->setSceneRect(-1000000, -100, 2000000, 200); // X축 범위 증가 (ms 단위), Y는 정규화
    setFocusPolicy(Qt::StrongFocus); // Delete 키
    
    scale(1, -1);
    
    connect(m_scene, &QGraphicsScene::selectionChanged, this, &GraphEditorView::onSceneSelectionChanged);
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
    // Y축 레이블 업데이트를 위해 update() 호출
    connect(m_document, &MotionDocument::activeMotorChanged, this, QOverload<>::of(&GraphEditorView::update)); 

    for(MotorProfile* profile : m_document->motorProfiles()) {
        onMotorAdded(profile);
    }
    onActiveMotorChanged(m_document->activeProfile(), nullptr);
}

// --- 이벤트 오버라이드 ---
// 4번: X축만 줌
void GraphEditorView::wheelEvent(QWheelEvent* event) {
    double scaleFactor = (event->angleDelta().y() > 0) ? 1.15 : 1.0 / 1.15;
    scale(scaleFactor, 1.0); // Y축 스케일은 1.0으로 고정
}
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
void GraphEditorView::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::MiddleButton) {
        m_isPanning = false;
        setCursor(Qt::ArrowCursor);
        event->accept();
        return;
    }
    QGraphicsView::mouseReleaseEvent(event);
}
// 1번, 2번: 수직 패닝만 가능, 방향 수정
void GraphEditorView::mouseMoveEvent(QMouseEvent* event) {
    if (m_isPanning) {
        // 수평 스크롤바 조작 없음 (X=0 중앙 고정)
        verticalScrollBar()->setValue(verticalScrollBar()->value() + (event->pos().y() - m_panStartPos.y()));
        m_panStartPos = event->pos(); 
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}
// AddNodeCommand에 "실제" 좌표(x:ms) 전달
void GraphEditorView::contextMenuEvent(QContextMenuEvent* event) {
    if (!m_document || !m_document->activeProfile() || !m_undoStack) return;
    QPointF scenePos = mapToScene(event->pos()); // 씬 좌표 (x:ms, y:normalized)
    MotorProfile* activeProfile = m_document->activeProfile();
    QMenu menu;
    QAction* addAction = menu.addAction("여기( " + 
        QString::number(scenePos.x(), 'f', 0) + "ms, " + // ms 단위 표시
        QString::number(activeProfile->getRealY(scenePos.y()), 'f', 2) + 
        " )에 새 노드 추가");
    if (scenePos.x() < 0) {
        addAction->setEnabled(false);
        addAction->setText(addAction->text() + " (X < 0 불가)");
    }
    else if (scenePos.y() < NORMALIZED_Y_MIN || scenePos.y() > NORMALIZED_Y_MAX) {
        addAction->setEnabled(false);
        addAction->setText(addAction->text() + " (Y축 제한 위반)");
    }
    connect(addAction, &QAction::triggered, this, [=]() {
        double realY = activeProfile->getRealY(scenePos.y());
        QPointF realPos(scenePos.x(), realY); // X는 ms 단위 그대로
        m_undoStack->push(new AddNodeCommand(activeProfile, realPos));
    });
    menu.exec(event->globalPos());
}
void GraphEditorView::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Delete) {
        if (!m_document || !m_document->activeProfile() || !m_undoStack) return;
        QList<GraphNodeItem*> itemsToDelete;
        for (QGraphicsItem* item : m_scene->selectedItems()) {
            if (auto nodeItem = qgraphicsitem_cast<GraphNodeItem*>(item)) {
                itemsToDelete.append(nodeItem);
            }
        }
        if (itemsToDelete.isEmpty()) return;
        std::sort(itemsToDelete.begin(), itemsToDelete.end(), [](auto a, auto b) {
            return a->index() > b->index();
        });
        m_undoStack->beginMacro("선택된 노드 삭제");
        for (GraphNodeItem* item : itemsToDelete) {
            m_undoStack->push(new DeleteNodeCommand(item->profile(), item->index()));
        }
        m_undoStack->endMacro();
        event->accept();
    } else {
        QGraphicsView::keyPressEvent(event);
    }
}

// 3번, 4번: X/Y축 그리드 및 레이블 조정
// 1번: 활성 모터 없을 때 Y 레이블 숨김
void GraphEditorView::drawBackground(QPainter* painter, const QRectF& rect) {
    QGraphicsView::drawBackground(painter, rect);
    QPen gridPen(QColor(220, 220, 220), 0); 
    gridPen.setCosmetic(true);
    painter->setPen(gridPen);
    
    MotorProfile* activeProfile = m_document ? m_document->activeProfile() : nullptr;

    // 3번: X축 그리드 (m_gridSizeX = 50ms 사용)
    double left_x = qFloor(rect.left() / m_gridSizeX) * m_gridSizeX;
    for (double x = left_x; x < rect.right(); x += m_gridSizeX) { 
        painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
    }
    
    // 4번: Y축 그리드 (max_abs_y / 10 간격)
    if (activeProfile) {
        double maxAbsY = activeProfile->getMaxAbsY();
        if (maxAbsY > 1e-6) {
             double yGridStepReal = maxAbsY / 10.0;
             // 양수 방향 그리드
             for(double y_real = yGridStepReal; y_real <= maxAbsY + 1e-6; y_real += yGridStepReal) {
                 double y_scene = activeProfile->getNormalizedY(y_real);
                 painter->drawLine(QPointF(rect.left(), y_scene), QPointF(rect.right(), y_scene));
             }
             // 음수 방향 그리드
             for(double y_real = -yGridStepReal; y_real >= -maxAbsY - 1e-6; y_real -= yGridStepReal) {
                 double y_scene = activeProfile->getNormalizedY(y_real);
                 painter->drawLine(QPointF(rect.left(), y_scene), QPointF(rect.right(), y_scene));
             }
        }
    } else { // 활성 모터 없을 때 기본 그리드 (씬 좌표 5 간격)
        double gridY_scene = (NORMALIZED_Y_MAX - NORMALIZED_Y_MIN) / 20.0; // 5.0
        double top_y_scene = qFloor(rect.top() / gridY_scene) * gridY_scene;
         for (double y = top_y_scene; y < rect.bottom(); y += gridY_scene) {
            painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
        }
    }
    
    // 축선
    QPen axisPen(QColor(180, 180, 180), 0);
    axisPen.setCosmetic(true);
    painter->setPen(axisPen);
    painter->drawLine(QPointF(0, rect.top()), QPointF(0, rect.bottom())); // Y축
    painter->drawLine(QPointF(rect.left(), 0), QPointF(rect.right(), 0)); // X축

    
    painter->save(); 
    painter->scale(1, -1); 

    painter->setPen(QPen(Qt::black));
    QFont font = painter->font();
    qreal currentScaleY = transform().m22(); 
    qreal scaleFactorY = qAbs(currentScaleY) > 1e-6 ? 1.0 / qAbs(currentScaleY) : 1.0;
    qreal currentScaleX = transform().m11();
    qreal scaleFactorX = qAbs(currentScaleX) > 1e-6 ? 1.0 / qAbs(currentScaleX) : 1.0;
    font.setPointSizeF(8 * scaleFactorY); // Y 스케일 기준으로 폰트 크기 고정
    painter->setFont(font);


    // 3번: X축 레이블 (50ms 간격)
    double xLabelInterval = m_gridSizeX; // 50ms
    double startXLabel = qFloor(rect.left() / xLabelInterval) * xLabelInterval;
    for (double x = startXLabel; x < rect.right(); x += xLabelInterval) {
        QString xLabel = QString::number(x, 'f', 0); // ms 단위, 정수
        // 0 레이블은 그리지 않음
        if (qAbs(x) > 1e-3) {
             QRectF textRect(x - 50 * scaleFactorX, 2 * scaleFactorY, 100 * scaleFactorX, 20 * scaleFactorY);
             painter->drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, xLabel);
        }
    }
    
    // 1번, 2번, 4번: Y축 레이블 (max_abs_y / 10 간격, 활성 모터 있을 때만)
    if (activeProfile) {
        double maxAbsY = activeProfile->getMaxAbsY();
        if (maxAbsY > 1e-6) {
             double yLabelStepReal = maxAbsY / 10.0;
             // 양수/음수 방향 레이블 (0 제외)
             for(int i = 1; i <= 10; ++i) {
                 // 양수
                 double y_real_pos = i * yLabelStepReal;
                 double y_scene_pos = activeProfile->getNormalizedY(y_real_pos);
                 QString label_pos = QString::number(y_real_pos, 'f', 1);
                 QRectF textRectPos(2 * scaleFactorX, -y_scene_pos - 10 * scaleFactorY, 50 * scaleFactorX, 20 * scaleFactorY);
                 painter->drawText(textRectPos, Qt::AlignLeft | Qt::AlignVCenter, label_pos);

                 // 음수
                 double y_real_neg = -i * yLabelStepReal;
                 double y_scene_neg = activeProfile->getNormalizedY(y_real_neg);
                 QString label_neg = QString::number(y_real_neg, 'f', 1);
                 QRectF textRectNeg(2 * scaleFactorX, -y_scene_neg - 10 * scaleFactorY, 50 * scaleFactorX, 20 * scaleFactorY);
                 painter->drawText(textRectNeg, Qt::AlignLeft | Qt::AlignVCenter, label_neg);
             }
        }
    } // 활성 모터 없으면 Y 레이블 없음 (1번)
    
    painter->restore(); 
}

// --- 슬롯 구현 ---
void GraphEditorView::onDocumentCleared() {
    clearAllProfileItems();
    for(const auto& list : m_constraintItems) {
        qDeleteAll(list);
    }
    m_constraintItems.clear();
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
    updateProfileVisibility(profile, false); 
    connect(profile, &MotorProfile::dataChanged, this, &GraphEditorView::onProfileDataChanged);
    connect(profile, &MotorProfile::constraintsChanged, this, &GraphEditorView::onProfileConstraintsChanged);
}
void GraphEditorView::onActiveMotorChanged(MotorProfile* active, MotorProfile* previous) {
    if (previous) {
        updateProfileVisibility(previous, false);
        setConstraintItemsVisible(previous, false); 
    }
    if (active) {
        updateProfileVisibility(active, true);
        updateConstraintItems(active); 
        setConstraintItemsVisible(active, true);  
    }
}
void GraphEditorView::onProfileDataChanged() {
    MotorProfile* profile = qobject_cast<MotorProfile*>(sender());
    if (profile) {
        int selectedIndex = -1;
        QPointF selectedRealPos; 
        if (m_scene->selectedItems().size() == 1) {
            if (auto node = qgraphicsitem_cast<GraphNodeItem*>(m_scene->selectedItems().first())) {
                selectedIndex = node->index();
                // Check if index is still valid before accessing node data
                if (selectedIndex >= 0 && selectedIndex < node->profile()->nodeCount()) {
                     selectedRealPos = node->profile()->nodeAt(selectedIndex);
                } else {
                    selectedIndex = -1; // Index became invalid (e.g., node deleted)
                }
            }
        }
        rebuildProfileItems(profile);
        updateProfileVisibility(profile, (profile == m_document->activeProfile()));
        if (selectedIndex != -1) {
            GraphNodeItem* reselectedNode = nullptr;
            qreal minDistSq = -1.0;
            for (auto item : m_profileItems.value(profile)) {
                if (auto node = qgraphicsitem_cast<GraphNodeItem*>(item)) {
                     // Check if index is valid before accessing
                     if(node->index() >= 0 && node->index() < node->profile()->nodeCount()){
                         QPointF currentRealPos = node->profile()->nodeAt(node->index());
                         qreal distSq = QPointF(currentRealPos - selectedRealPos).manhattanLength();
                         if (reselectedNode == nullptr || distSq < minDistSq) {
                             minDistSq = distSq;
                             reselectedNode = node;
                         }
                     }
                }
            }
            if (reselectedNode && minDistSq < 1e-3) {
                 reselectedNode->setSelected(true);
            } else {
                 reselectedNode = nullptr;
            }
            emit nodeSelectionChanged(reselectedNode);
        } else {
             emit nodeSelectionChanged(nullptr); // Ensure panel clears if selection is lost
        }
    }
}
void GraphEditorView::onProfileConstraintsChanged() {
    MotorProfile* profile = qobject_cast<MotorProfile*>(sender());
     if (profile) {
        updateConstraintItems(profile);
     }
}
// "실제" 좌표(x:ms)를 "씬" 좌표로 변환하여 그리기
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
    const auto& nodes = profile->nodes(); // "실제" 좌표 노드 (x:ms)
    QColor color = profile->color();
    QPen linePen(color, 2); 
    for (int i = 0; i < nodes.size() - 1; ++i) {
        double sceneY_i = profile->getNormalizedY(nodes[i].y());
        double sceneY_i1 = profile->getNormalizedY(nodes[i+1].y());
        // X 좌표는 ms 단위 그대로 사용
        QGraphicsLineItem* line = m_scene->addLine(QLineF(nodes[i].x(), sceneY_i, nodes[i+1].x(), sceneY_i1), linePen);
        items.append(line);
    }
    for (int i = 0; i < nodes.size(); ++i) {
        GraphNodeItem* nodeItem = new GraphNodeItem(profile, i, this, m_undoStack);
        m_scene->addItem(nodeItem);
        items.append(nodeItem);
    }
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

// 씬 좌표 (정규화됨) 기준으로 상/하한선 그리기, 색상 고정
void GraphEditorView::updateConstraintItems(MotorProfile* profile) {
    if (!profile) return;

    if (m_constraintItems.contains(profile)) {
        qDeleteAll(m_constraintItems.value(profile));
        m_constraintItems[profile].clear();
    } else {
        m_constraintItems.insert(profile, QList<QGraphicsItem*>());
    }

    QList<QGraphicsItem*>& items = m_constraintItems[profile];
    QRectF r = sceneRect(); 
    
    double sceneYMin = NORMALIZED_Y_MIN; // 씬 좌표 0
    double sceneYMax = NORMALIZED_Y_MAX; // 씬 좌표 100
    
    QColor lineColor(128, 128, 128, 150); 
    QColor textColor(80, 80, 80); 

    QPen linePen(lineColor, 3, Qt::SolidLine); 

    QGraphicsLineItem* minLine = m_scene->addLine(r.left(), sceneYMin, r.right(), sceneYMin, linePen);
    QGraphicsLineItem* maxLine = m_scene->addLine(r.left(), sceneYMax, r.right(), sceneYMax, linePen);
    minLine->setZValue(-1); 
    maxLine->setZValue(-1);
    items.append(minLine);
    items.append(maxLine);

    qreal currentScaleY = transform().m22();
    qreal scaleFactor = qAbs(currentScaleY) > 1e-6 ? 1.0 / qAbs(currentScaleY) : 1.0;
    qreal currentScaleX = transform().m11();
    qreal scaleFactorX = qAbs(currentScaleX) > 1e-6 ? 1.0 / qAbs(currentScaleX) : 1.0;

    QGraphicsTextItem* minLabel = m_scene->addText(QString::number(profile->yMin(), 'f', 1));
    minLabel->setTransform(QTransform::fromScale(scaleFactorX, -scaleFactorY)); // 텍스트 크기 고정
    minLabel->setPos(r.left() + 5 , sceneYMin + minLabel->boundingRect().height() * scaleFactor); // 위치 보정
    minLabel->setDefaultTextColor(textColor); 
    minLabel->setZValue(-1);
    items.append(minLabel);

    QGraphicsTextItem* maxLabel = m_scene->addText(QString::number(profile->yMax(), 'f', 1));
    maxLabel->setTransform(QTransform::fromScale(scaleFactorX, -scaleFactorY)); // 텍스트 크기 고정
    maxLabel->setPos(r.left() + 5 , sceneYMax); // 위치 보정
    maxLabel->setDefaultTextColor(textColor); 
    maxLabel->setZValue(-1);
    items.append(maxLabel);

    setConstraintItemsVisible(profile, (profile == m_document->activeProfile()));
}
void GraphEditorView::setConstraintItemsVisible(MotorProfile* profile, bool visible) {
    if (!m_constraintItems.contains(profile)) return;
    for (QGraphicsItem* item : m_constraintItems.value(profile)) {
        item->setVisible(visible);
    }
}

// Fit to View 슬롯
void GraphEditorView::fitToView() {
    QRectF bounds;
    double xMaxFit = 1000.0; // 기본 X 범위 (ms)
    if (m_document && m_document->activeProfile()) {
         if(!m_document->activeProfile()->nodes().isEmpty()){
            for(const auto& node : m_document->activeProfile()->nodes()){
                if(node.x() > xMaxFit) x