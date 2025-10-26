#pragma once

#include <QUndoCommand>
#include <QPointF>
#include <QList>
#include <QSet>
#include "motionmodels.h" // For MotionNode, MotorProfile

/**
 * @brief Undo/Redo command for adding a new node.
 */
class AddNodeCommand : public QUndoCommand {
public:
    AddNodeCommand(MotorProfile* profile, const MotionNode& node, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    MotorProfile* m_profile;
    MotionNode m_node;
    int m_nodeIndex = -1;
};

/**
 * @brief Undo/Redo command for deleting an existing node.
 */
class DeleteNodeCommand : public QUndoCommand {
public:
    DeleteNodeCommand(MotorProfile* profile, int index, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    MotorProfile* m_profile;
    MotionNode m_node;
    int m_nodeIndex;
};

/**
 * @brief Undo/Redo command for moving a SINGLE node (e.g., from Apply Coordinates).
 */
class MoveNodeCommand : public QUndoCommand {
public:
    MoveNodeCommand(MotorProfile* profile, int index, const QPointF& oldPos, const QPointF& newPos, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
    bool mergeWith(const QUndoCommand* command) override;
    int id() const override { return 1234; }

private:
    MotorProfile* m_profile;
    int m_nodeIndex;
    QPointF m_oldPos;
    QPointF m_newPos;
};

// <<< MoveNodesCommand 선언부 삭제 >>>

