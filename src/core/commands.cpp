#include "commands.h"
#include <QDebug>
#include <QSet>

// --- AddNodeCommand Implementation ---
AddNodeCommand::AddNodeCommand(MotorProfile* profile, const MotionNode& node, QUndoCommand* parent)
    : QUndoCommand(parent), m_profile(profile), m_node(node), m_nodeIndex(-1) {
    setText("Add Node");
}
void AddNodeCommand::redo() {
    if (!m_profile) return;
    m_nodeIndex = m_profile->internalAddNode(m_node);
    // m_profile->emitDataChanged(); // Done by internalAddNode
}
void AddNodeCommand::undo() {
    if (!m_profile || m_nodeIndex == -1) return;
    m_profile->internalRemoveNode(m_nodeIndex);
    m_nodeIndex = -1;
    // m_profile->emitDataChanged(); // Done by internalRemoveNode
}

// --- DeleteNodeCommand Implementation ---
DeleteNodeCommand::DeleteNodeCommand(MotorProfile* profile, int index, QUndoCommand* parent)
    : QUndoCommand(parent), m_profile(profile), m_nodeIndex(index) {
    if (m_profile && index >= 0 && index < m_profile->nodeCount()) {
        m_node = m_profile->nodeAt(index);
    } else {
        qWarning() << "DeleteNodeCommand: Invalid index" << index;
        m_node = QPointF();
    }
    setText("Delete Node");
}
void DeleteNodeCommand::redo() {
    if (!m_profile) return;
    int indexToUse = m_nodeIndex;
    if (indexToUse < 0 || indexToUse >= m_profile->nodeCount() || m_profile->nodeAt(indexToUse) != m_node) {
         indexToUse = m_profile->nodes().indexOf(m_node);
    }

    if (indexToUse != -1) {
         m_profile->internalRemoveNode(indexToUse);
         m_nodeIndex = indexToUse;
    } else {
         qWarning() << "DeleteNodeCommand redo: Node not found or index invalid.";
    }
}
void DeleteNodeCommand::undo() {
    if (!m_profile || m_node.isNull()) return;
    m_nodeIndex = m_profile->internalAddNode(m_node);
}

// --- MoveNodeCommand Implementation ---
MoveNodeCommand::MoveNodeCommand(MotorProfile* profile, int index, const QPointF& oldPos, const QPointF& newPos, QUndoCommand* parent)
    : QUndoCommand(parent), m_profile(profile), m_nodeIndex(index), m_oldPos(oldPos), m_newPos(newPos) {
    setText("Move Node");
}
void MoveNodeCommand::redo() {
    if (!m_profile) return;
    if (m_nodeIndex >= 0 && m_nodeIndex < m_profile->nodeCount()) {
        m_profile->internalMoveNode(m_nodeIndex, m_newPos);
        m_profile->sortNodes();
        m_profile->emitDataChanged();
    } else { qWarning() << "MoveNodeCommand redo: Invalid index" << m_nodeIndex; }
}
void MoveNodeCommand::undo() {
    if (!m_profile) return;
    if (m_nodeIndex >= 0 && m_nodeIndex < m_profile->nodeCount()) {
        m_profile->internalMoveNode(m_nodeIndex, m_oldPos);
        m_profile->sortNodes();
        m_profile->emitDataChanged();
    } else { qWarning() << "MoveNodeCommand undo: Invalid index" << m_nodeIndex; }
}
bool MoveNodeCommand::mergeWith(const QUndoCommand* command) {
    const MoveNodeCommand* moveCommand = dynamic_cast<const MoveNodeCommand*>(command);
    if (!moveCommand || moveCommand->id() != id() || !moveCommand->m_profile || moveCommand->m_profile != m_profile || moveCommand->m_nodeIndex != m_nodeIndex) {
        return false;
    }
    m_newPos = moveCommand->m_newPos;
    setText("Move Node to (" + QString::number(m_newPos.x(),'f',1) + ", " + QString::number(m_newPos.y(),'f',1) + ")");
    return true;
}

// <<< MoveNodesCommand 구현부 삭제 >>>

