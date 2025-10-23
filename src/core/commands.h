#pragma once

#include <QUndoCommand>
#include <QPointF>
#include "motionmodels.h" // MotionNode, MotorProfile 사용

// 1. 노드 추가 커맨드
class AddNodeCommand : public QUndoCommand {
public:
    AddNodeCommand(MotorProfile* profile, const MotionNode& node, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    MotorProfile* m_profile;
    MotionNode m_node; // 실제 좌표 (x: ms)
    int m_nodeIndex;
};

// 2. 노드 삭제 커맨드
class DeleteNodeCommand : public QUndoCommand {
public:
    DeleteNodeCommand(MotorProfile* profile, int index, QUndoCommand* parent = nullptr);
    void undo() override;
    void redo() override;
private:
    MotorProfile* m_profile;
    MotionNode m_node; // 실제 좌표 (x: ms)
    int m_nodeIndex;
};

// 3. 노드 이동 커맨드
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
    QPointF m_oldPos; // 실제 좌표 (x: ms)
    QPointF m_newPos; // 실제 좌표 (x: ms)
};