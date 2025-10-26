#include "motionmodels.h"
#include <QFile>
#include <QTextStream>
#include <QDebug>
#include <algorithm> // for std::sort
#include <qmath.h>   // qBound, qAbs, fmod, qFloor, qMax
#include <QStringList> // For YAML parsing

// --- MotorProfile Implementation ---
MotorProfile::MotorProfile(const QString& name, QColor color, QObject* parent)
    : QObject(parent), m_name(name), m_color(color),
      m_y_min(-100.0), m_y_max(100.0), m_max_slope(1000.0) // Initialize defaults
{
}

MotionNode MotorProfile::nodeAt(int index) const {
    if (index < 0 || index >= m_nodes.size()) {
        qWarning() << "nodeAt: Index" << index << "out of bounds (size" << m_nodes.size() << ")";
        return MotionNode();
    }
    return m_nodes[index];
}

void MotorProfile::setYMin(double val) {
    if (m_y_min != val) {
        m_y_min = val;
        emit constraintsChanged();
    }
}
void MotorProfile::setYMax(double val) {
    if (m_y_max != val) {
        m_y_max = val;
        emit constraintsChanged();
    }
}
void MotorProfile::setMaxSlope(double val) {
    val = qMax(0.0, val); // Ensure non-negative
    if (m_max_slope != val) {
        m_max_slope = val;
        emit constraintsChanged();
    }
}

void MotorProfile::checkAllNodes() {
    bool dataWasChanged = false;
    for (int i = 0; i < m_nodes.size(); ++i) {
        MotionNode& node = m_nodes[i];
        double oldY = node.y();
        double clampedY = qBound(m_y_min, oldY, m_y_max);
        if (qAbs(oldY - clampedY) > 1e-6) {
            node.setY(clampedY);
            dataWasChanged = true;
        }
    }
    if (dataWasChanged) {
        emit dataChanged();
    }
}

double MotorProfile::sampleAt(double time) const {
    if (m_nodes.isEmpty()) return 0.0;
    if (time <= m_nodes.first().x()) return m_nodes.first().y();
    if (time >= m_nodes.last().x()) return m_nodes.last().y();

    for (int i = 0; i < m_nodes.size() - 1; ++i) {
        const MotionNode& prev = m_nodes[i];
        const MotionNode& next = m_nodes[i+1];
        if (prev.x() <= time && next.x() >= time) {
            if (qAbs(next.x() - prev.x()) < 1e-6) return prev.y();
            double t = (time - prev.x()) / (next.x() - prev.x());
            return prev.y() * (1.0 - t) + next.y() * t;
        }
    }
    qWarning() << "sampleAt: Time" << time << "was within bounds but segment not found.";
    return m_nodes.last().y();
}

bool MotorProfile::isNodeValid(const MotionNode& node, int /*indexToIgnore*/) const {
    if (node.x() < 0.0) {
        qDebug() << "Node validation failed: X < 0 (" << node.x() << ")";
        return false;
    }
    return true;
}

// --- Internal functions for Undo/Redo ---
int MotorProfile::internalAddNode(const MotionNode& node) {
    m_nodes.append(node);
    sortNodes();
    emitDataChanged(); // <<< 시그널 방출 추가 (요청 1, 2, 3 해결)
    return m_nodes.indexOf(node);
}

void MotorProfile::internalRemoveNode(int index) {
    if (index >= 0 && index < m_nodes.size()) {
        m_nodes.remove(index);
        emitDataChanged(); // <<< 시그널 방출 추가 (요청 1, 2, 3 해결)
    } else {
         qWarning() << "internalRemoveNode: Invalid index" << index;
    }
}

void MotorProfile::internalMoveNode(int index, const MotionNode& pos) {
    if (index >= 0 && index < m_nodes.size()) {
        m_nodes[index] = pos;
        // 정렬 및 시그널 방출은 MoveNodeCommand에서 담당
    } else {
        qWarning() << "internalMoveNode: Invalid index" << index;
    }
}

void MotorProfile::sortNodes() {
    std::sort(m_nodes.begin(), m_nodes.end(), [](const MotionNode& a, const MotionNode& b) {
        if (qAbs(a.x() - b.x()) < 1e-9) return a.y() < b.y();
        return a.x() < b.x();
    });
}

void MotorProfile::emitDataChanged() {
    emit dataChanged();
}

// --- MotionDocument Implementation ---
MotionDocument::MotionDocument(QObject* parent) : QObject(parent), m_activeProfile(nullptr) {}

MotionDocument::~MotionDocument() {
    qDeleteAll(m_profiles);
}

int MotionDocument::activeProfileIndex() const {
    if (!m_activeProfile) return -1;
    return m_profiles.indexOf(m_activeProfile);
}

MotorProfile* MotionDocument::addMotor(const QString& name, QColor color) {
    MotorProfile* profile = new MotorProfile(name, color, this);
    m_profiles.append(profile);
    emit motorAdded(profile);
    emit modelChanged();
    return profile;
}

void MotionDocument::setActiveMotor(MotorProfile* profile) {
    if ((profile && m_profiles.contains(profile)) || !profile) {
        if (profile != m_activeProfile) {
            MotorProfile* oldActive = m_activeProfile;
            m_activeProfile = profile;
            emit activeMotorChanged(m_activeProfile, oldActive);
        }
    } else {
         qWarning() << "Attempted to set an invalid motor profile as active:" << (profile ? profile->name() : "null pointer");
    }
}

void MotionDocument::removeMotor(MotorProfile* profile) {
    if (!profile) return;
    int index = m_profiles.indexOf(profile);
    if (index == -1) return;

    bool wasActive = (m_activeProfile == profile);
    m_profiles.removeAt(index);

    if (wasActive) {
        m_activeProfile = nullptr;
        if (!m_profiles.isEmpty()) {
            int newIndex = qMax(0, index - 1);
            if (newIndex < m_profiles.size()) setActiveMotor(m_profiles.at(newIndex));
            else if (!m_profiles.isEmpty()) setActiveMotor(m_profiles.first());
        } else {
             setActiveMotor(nullptr);
        }
    }

    emit modelChanged();
    profile->deleteLater();
}

// Save all motors to YAML format
bool MotionDocument::saveToYAML(const QString& filename, const QString& id) const {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for writing:" << filename << file.errorString();
        return false;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");

    out << "id: " << id << "\n";
    for (const MotorProfile* profile : m_profiles) {
        if (!profile) continue;
        QString keyName = profile->name();
        keyName.replace(':', '_').replace(' ', '_'); // Basic sanitization
        if (keyName.isEmpty()) keyName = "unnamed_motor";

        out << keyName << ":\n";
        out << "  - ["; // Start sequence
        bool firstNode = true;
        for (const MotionNode& node : profile->nodes()) {
            if (!firstNode) out << ", ";
            out << "[" << QString::number(node.x(), 'g', 10) << ", "
                << QString::number(node.y(), 'g', 10) << "]";
            firstNode = false;
        }
        out << "]\n"; // End sequence
    }
    file.close();
    return true;
}

// Load motors from YAML format
bool MotionDocument::loadFromYAML(const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Failed to open file for reading:" << filename << file.errorString();
        return false;
    }
    QTextStream in(&file);
    in.setCodec("UTF-8");

    emit documentCleared();
    qDeleteAll(m_profiles);
    m_profiles.clear();
    m_activeProfile = nullptr;

    QString file_id;
    MotorProfile* currentProfile = nullptr;

    while (!in.atEnd()) {
        QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;

        if (line.startsWith("id:")) {
            file_id = line.mid(3).trimmed();
        }
        else if (!line.startsWith(" ") && line.endsWith(":")) { // Motor definition
            QString name = line.left(line.length() - 1).trimmed();
            QColor color = QColor::fromHsv(qrand() % 360, 200, 200);
            currentProfile = addMotor(name, color);
            if (!currentProfile) qWarning() << "Failed to create motor:" << name;
        }
        else if (line.startsWith("- [") && line.endsWith("]") && currentProfile) { // Node list
            QString nodes_str = line.mid(3, line.length() - 4).trimmed();
            nodes_str.remove(' ');

            if (nodes_str.isEmpty()) continue; // Skip empty `[]`

            QStringList node_pairs = nodes_str.split("],[", QString::SkipEmptyParts);
            for (const QString& pair : node_pairs) {
                QString cleaned_pair = pair;
                if (cleaned_pair.startsWith('[')) cleaned_pair.remove(0, 1);
                if (cleaned_pair.endsWith(']')) cleaned_pair.chop(1);
                QStringList values = cleaned_pair.split(',');

                if (values.size() == 2) {
                    bool okX, okY;
                    double time = values[0].toDouble(&okX);
                    double value = values[1].toDouble(&okY);
                    if (okX && okY) {
                        currentProfile->internalAddNode(QPointF(time, value)); // This now emits dataChanged
                    } else { qWarning() << "Failed to parse node values:" << pair; }
                } else { qWarning() << "Invalid node format:" << pair; }
            }
            // sortNodes() is called inside internalAddNode
        } else {
            qWarning() << "Unknown or misplaced YAML line:" << line;
        }
    }

    file.close();
    if (!m_profiles.isEmpty()) setActiveMotor(m_profiles.first());
    return true;
}
