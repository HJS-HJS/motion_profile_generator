#pragma once

#include <QUndoCommand>
#include <QPointF>
#include <QList> // For MoveNodesCommand
#include <QSet>    // <<< QSet 헤더 추가 >>>
#include "motionmodels.h" // For MotionNode, MotorProfile

// --- AddNodeCommand (변경 없음) ---
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

// --- DeleteNodeCommand (변경 없음) ---
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

// --- MoveNodeCommand (단일 노드 이동용) (변경 없음) ---
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


// --- MoveNodesCommand (다중 노드 이동용) ---
class MoveNodesCommand : public QUndoCommand {
public:
    // Structure to hold move data for a single node
    struct MoveData {
        MotorProfile* profile;
        int nodeIndex;
        QPointF oldRealPos;
        QPointF newRealPos;
    };

    explicit MoveNodesCommand(const QList<MoveData>& moves, QUndoCommand* parent = nullptr);

    void undo() override;
    void redo() override;

    // bool mergeWith(const QUndoCommand* command) override; // Merging multi-move not implemented
    // int id() const override { return 5678; }

private:
    QList<MoveData> m_moves;
    // Store unique profiles affected by this multi-move
    QSet<MotorProfile*> m_affectedProfiles; // <<< 타입 명시 (QSet 헤더 필요)
};
