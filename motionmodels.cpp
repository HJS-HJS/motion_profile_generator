#include "motionmodels.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDebug>
#include <algorithm> // for std::sort

// --- MotorProfile 구현 ---
// (이전 Qt 6 코드와 동일)
MotorProfile::MotorProfile(const QString& name, QColor color, QObject* parent)
    : QObject(parent), m_name(name), m_color(color) {
}
MotionNode MotorProfile::nodeAt(int index) const {
    if (index < 0 || index >= m_nodes.size()) {
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
    if (m_max_slope != val) {
        m_max_slope = val;
        emit constraintsChanged();
    }
}
bool MotorProfile::addNode(const MotionNode& node) {
    if (!isNodeValid(node, -1)) { 
        return false;
    }
    m_nodes.append(node);
    sortNodes();
    emit dataChanged();
    return true;
}
bool MotorProfile::updateNode(int index, const MotionNode& node) {
    if (index < 0 || index >= m_nodes.size()) {
        return false;
    }
    if (!isNodeValid(node, index)) { 
        return false;
    }
    m_nodes[index] = node;
    sortNodes(); 
    emit dataChanged();
    return true;
}
void MotorProfile::deleteNode(int index) {
    if (index < 0 || index >= m_nodes.size()) {
        return;
    }
    m_nodes.remove(index);
    emit dataChanged(); 
}
void MotorProfile::sortNodes() {
    std::sort(m_nodes.begin(), m_nodes.end(), [](const MotionNode& a, const MotionNode& b) {
        return a.x() < b.x();
    });
}
bool MotorProfile::isNodeValid(const MotionNode& node, int indexToIgnore) const {
    if (node.y() < m_y_min || node.y() > m_y_max) {
        qDebug() << "Y축 제한 위반:" << node.y();
        return false;
    }
    Q_UNUSED(indexToIgnore); 
    return true;
}
void MotorProfile::read(const QJsonObject& json) {
    m_name = json["name"].toString(m_name);
    m_color = QColor(json["color"].toString(m_color.name()));
    m_y_min = json["y_min"].toDouble(m_y_min);
    m_y_max = json["y_max"].toDouble(m_y_max);
    m_max_slope = json["max_slope"].toDouble(m_max_slope);

    m_nodes.clear();
    QJsonArray nodesArray = json["nodes"].toArray();
    for (const QJsonValue& val : nodesArray) {
        QJsonObject nodeObj = val.toObject();
        m_nodes.append(MotionNode(nodeObj["time"].toDouble(), nodeObj["value"].toDouble()));
    }
    sortNodes();
}
void MotorProfile::write(QJsonObject& json) const {
    json["name"] = m_name;
    json["color"] = m_color.name();
    json["y_min"] = m_y_min;
    json["y_max"] = m_y_max;
    json["max_slope"] = m_max_slope;

    QJsonArray nodesArray;
    for (const MotionNode& node : m_nodes) {
        QJsonObject nodeObj;
        nodeObj["time"] = node.x();
        nodeObj["value"] = node.y();
        nodesArray.append(nodeObj);
    }
    json["nodes"] = nodesArray;
}


// --- MotionDocument 구현 ---

MotionDocument::MotionDocument(QObject* parent) : QObject(parent) {}

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

// 8. QTreeWidget에서 사용하기 위해 포인터 기반으로 변경됨
void MotionDocument::setActiveMotor(MotorProfile* profile) {
    if (profile != m_activeProfile) {
        MotorProfile* oldActive = m_activeProfile;
        m_activeProfile = profile; // profile은 nullptr일 수 있음
        emit activeMotorChanged(m_activeProfile, oldActive);
    }
}

// 11. JSON 파일로 저장 (Qt 6와 동일)
bool MotionDocument::saveToFile(const QString& filename) const {
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "파일 쓰기 실패:" << filename;
        return false;
    }
    QJsonArray motorsArray;
    for (MotorProfile* profile : m_profiles) {
        QJsonObject motorObj;
        profile->write(motorObj);
        motorsArray.append(motorObj);
    }
    QJsonObject rootObj;
    rootObj["motors"] = motorsArray;
    file.write(QJsonDocument(rootObj).toJson());
    file.close();
    return true;
}

// 12. JSON 파일에서 불러오기 (Qt 6와 동일)
bool MotionDocument::loadFromFile(const QString& filename) {
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "파일 읽기 실패:" << filename;
        return false;
    }
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "잘못된 JSON 형식:" << filename;
        return false;
    }

    emit documentCleared();
    qDeleteAll(m_profiles);
    m_profiles.clear();
    m_activeProfile = nullptr;

    QJsonObject rootObj = doc.object();
    QJsonArray motorsArray = rootObj["motors"].toArray();
    for (const QJsonValue& val : motorsArray) {
        QJsonObject motorObj = val.toObject();
        MotorProfile* profile = addMotor("", Qt::black); 
        profile->read(motorObj);
    }
    
    emit modelChanged(); // 모델 로드 완료 신호
    
    // 불러오기 후 첫 번째 모터를 활성 모터로 설정
    if (!m_profiles.isEmpty()) {
        setActiveMotor(m_profiles.first());
    }
    
    return true;
}

