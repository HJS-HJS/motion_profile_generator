#include "commands.h"
#include <QDebug>

// --- AddNodeCommand ---
AddNodeCommand::AddNodeCommand(MotorProfile* profile, const MotionNode& node, QUndoCommand* parent)
    : QUndoCommand(parent), m_profile(profile), m_node(node), m_nodeIndex(-1) {
    setText("노드 추가");
}

void AddNodeCommand::redo() {
    m_nodeIndex = m_profile->internalAddNode(m_node);
    m_profile->emitDataChanged();
}

void AddNodeCommand::undo() {
    if (m_nodeIndex != -1) {
        m_profile->internalRemoveNode(m_nodeIndex);
        m_profile->emitDataChanged();
    }
}

// --- DeleteNodeCommand ---
DeleteNodeCommand::DeleteNodeCommand(MotorProfile* profile, int index, QUndoCommand* parent)
    : QUndoCommand(parent), m_profile(profile), m_nodeIndex(index) {
    m_node = m_profile->nodeAt(index);
    setText("노드 삭제");
}

void DeleteNodeCommand::redo() {
    m_profile->internalRemoveNode(m_nodeIndex);
    m_profile->emitDataChanged();
}

void DeleteNodeCommand::undo() {
    m_profile->internalAddNode(m_node);
    m_profile->emitDataChanged();
}


// --- MoveNodeCommand ---
MoveNodeCommand::MoveNodeCommand(MotorProfile* profile, int index, const QPointF& oldPos, const QPointF& newPos, QUndoCommand* parent)
    : QUndoCommand(parent), m_profile(profile), m_nodeIndex(index), m_oldPos(oldPos), m_newPos(newPos) {
    setText("노드 이동");
}

void MoveNodeCommand::redo() {
    m_profile->internalMoveNode(m_nodeIndex, m_newPos);
    m_profile->sortNodes();
    m_profile->emitDataChanged();
}

void MoveNodeCommand::undo() {
    m_profile->internalMoveNode(m_nodeIndex, m_oldPos);
    m_profile->sortNodes();
    m_profile->emitDataChanged();
}

bool MoveNodeCommand::mergeWith(const QUndoCommand* command) {
    const MoveNodeCommand* moveCommand = static_cast<const MoveNodeCommand*>(command);
    if (moveCommand->id() != id() || moveCommand->m_nodeIndex != m_nodeIndex) {
        return false;
    }
    m_newPos = moveCommand->m_newPos;
    return true;
}