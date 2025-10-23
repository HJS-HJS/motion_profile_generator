#include "motionmodels.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QDebug>
#include <algorithm> // for std::sort
#include <qmath.h> // qBound, qAbs, fmod, qFloor

// 3번: YAML 헤더 포함
#include <yaml-cpp/yaml.h>
#include <fstream> // YAML 파일 쓰기

// --- MotorProfile 구현 ---
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

// 1번: 제약조건 적용 함수 (최대 기울기 로직 수정)
void MotorProfile::checkAllNodes() {
    bool dataWasChanged = false;

    // Y min/max 적용
    for (int i = 0; i < m_nodes.size(); ++i) {
        MotionNode node = m_nodes[i];
        double oldY = node.y();
        double clampedY = qBound(m_y_min, oldY, m_y_max);
        if (qAbs(oldY - clampedY) > 1e-6) {
            node.setY(clampedY);
            m_nodes[i] = node;
            dataWasChanged = true;
        }
    }

    // 최대 기울기 적용 (0번 노드는 기준점이므로 1번부터 시작)
    for (int i = 1; i < m_nodes.size(); ++i) {
        MotionNode& prevNode = m_nodes[i-1];
        MotionNode& currNode = m_nodes[i];

        double deltaX_ms = currNode.x() - prevNode.x();

        // 1번: deltaX가 0에 가까우면 기울기 제한을 적용하기 어려움 (무시)
        if (qAbs(deltaX_ms) < 1e-6) {
             qDebug() << "Warning: Nodes" << i-1 << "and" << i << "have the same timestamp. Skipping slope check.";
             continue;
        }

        double deltaY = currNode.y() - prevNode.y();
        double currentSlope = deltaY / deltaX_ms; // unit/ms

        // 1번: 최대 기울기(절댓값) 초과 검사 (부동소수점 오차 감안)
        if (qAbs(currentSlope) > m_max_slope + 1e-6) {
            // 최대 기울기에 맞춰 현재 노드(currNode)의 Y값 조정
            double allowedSlope = qBound(-m_max_slope, currentSlope, m_max_slope);
            double newDeltaY = allowedSlope * deltaX_ms;
            double newY = prevNode.y() + newDeltaY;

            // 조정된 Y값이 min/max 범위 안에 있도록 다시 제한
            newY = qBound(m_y_min, newY, m_y_max);

            if (qAbs(currNode.y() - newY) > 1e-6) {
                currNode.setY(newY);
                dataWasChanged = true;
                qDebug() << "Node" << i << " adjusted for slope constraint. New Y:" << newY;
            }
        }
    }

    if (dataWasChanged) {
        emit dataChanged(); // 뷰 갱신
    }
}


// Export용 선형 보간 함수 (time: ms)
double MotorProfile::sampleAt(double time_ms) const {
    if (m_nodes.isEmpty()) return 0.0;
    if (time_ms < 0.0) return 0.0;

    const MotionNode* prev = nullptr;
    const MotionNode* next = nullptr;

    for (int i = 0; i < m_nodes.size(); ++i) {
        if (m_nodes[i].x() <= time_ms) {
            prev = &m_nodes[i];
        } else {
            next = &m_nodes[i];
            break;
        }
    }

    if (!prev) {
        const MotionNode& firstNode = m_nodes.first();
        if (firstNode.x() < 1e-6) return firstNode.y();
        // 3번: X축 단위 ms 반영
        double t = time_ms / firstNode.x();
        return 0.0 * (1.0 - t) + firstNode.y() * t; // (0,0)에서 시작 가정
    }
    if (!next) return m_nodes.last().y();

    if (qAbs(next->x() - prev->x()) < 1e-6) {
        return prev->y();
    }

    // 3번: X축 단위 ms 반영
    double t = (time_ms - prev->x()) / (next->x() - prev->x());
    return prev->y() * (1.0 - t) + next->y() * t;
}

// JSON 쓰기 (x: ms)
void MotorProfile::write(QJsonObject& json) const {
    json["name"] = m_name;
    QJsonArray nodesArray;
    for (const MotionNode& node : m_nodes) {
        QJsonObject nodeObj;
        nodeObj["time_ms"] = node.x(); // 키 이름 변경
        nodeObj["value"] = node.y();
        nodesArray.append(nodeObj);
    }
    json["nodes"] = nodesArray;
}

// JSON 읽기 (x: ms)
void MotorProfile::read(const QJsonObject& json) {
    m_name = json["name"].toString(m_name);
    m_nodes.clear();
    QJsonArray nodesArray = json["nodes"].toArray();
    for (const QJsonValue& val : nodesArray) {
        QJsonObject nodeObj = val.toObject();
        // 키 이름 변경 (time_ms 또는 time 호환)
        double time_ms = nodeObj.contains("time_ms") ? nodeObj["time_ms"].toDouble() : nodeObj["time"].toDouble();
        m_nodes.append(MotionNode(time_ms, nodeObj["value"].toDouble()));
    }
    sortNodes();
}

// 샘플링 JSON 내보내기 (x: ms)
void MotorProfile::exportSamplesToJSON(QJsonObject& json, double sampleRateHz, double endTime_ms) const {
    json["name"] = m_name;
    QJsonArray nodesArray;
    if (sampleRateHz <= 0 || endTime_ms < 0) {
        json["nodes"] = nodesArray;
        return;
    }

    // 3번: 시간 단위 ms로 변경
    double dt_ms = 1000.0 / sampleRateHz; // 샘플링 간격 (ms)

    for (double time_ms = 0.0; time_ms <= endTime_ms; time_ms += dt_ms) {
        double value = sampleAt(time_ms);
        QJsonObject nodeObj;
        nodeObj["time_ms"] = time_ms; // 키 이름 변경
        nodeObj["value"] = value;
        nodesArray.append(nodeObj);
    }

    double lastSampleTime = (qFloor(endTime_ms / dt_ms)) * dt_ms;
    if (endTime_ms - lastSampleTime > 1e-6 && endTime_ms > 0.0) {
        double value = sampleAt(endTime_ms);
        QJsonObject nodeObj;
        nodeObj["time_ms"] = endTime_ms; // 키 이름 변경
        nodeObj["value"] = value;
        nodesArray.append(nodeObj);
    } else if (endTime_ms == 0.0 && dt_ms > 0.0) {
        double value = sampleAt(0.0);
        QJsonObject nodeObj;
        nodeObj["time_ms"] = 0.0; // 키 이름 변경
        nodeObj["value"] = value;
        nodesArray.append(nodeObj);
    }

    json["nodes"] = nodesArray;
}


bool MotorProfile::isNodeValid(const MotionNode& node, int indexToIgnore) const {
    if (node.x() < 0.0) {
        qDebug() << "X축 제한 위반:" << node.x();
        return false;
    }
    if (node.y() < m_y_min || node.y() > m_y_max) {
        qDebug() << "Y축 제한 위반:" << node.y();
        return false;
    }
    Q_UNUSED(indexToIgnore);
    return true;
}

// Y축 정규화 함수
double MotorProfile::getNormalizedY(double realY) const
{
    double range = m_y_max - m_y_min;
    if (qAbs(range) < 1e-6) {
        return NORMALIZED_Y_MIN + (NORMALIZED_Y_MAX - NORMALIZED_Y_MIN) / 2.0;
    }
    // realY가 min/max를 벗어나는 경우 대비하여 clamp
    realY = qBound(m_y_min, realY, m_y_max);
    double normalized = (realY - m_y_min) / range;
    return NORMALIZED_Y_MIN + normalized * (NORMALIZED_Y_MAX - NORMALIZED_Y_MIN);
}

// Y축 역정규화 함수
double MotorProfile::getRealY(double normalizedY) const
{
    double normRange = NORMALIZED_Y_MAX - NORMALIZED_Y_MIN;
    if (qAbs(normRange) < 1e-6) {
        return m_y_min;
    }
    // normalizedY가 0~100 벗어나는 경우 대비하여 clamp
    normalizedY = qBound(NORMALIZED_Y_MIN, normalizedY, NORMALIZED_Y_MAX);
    double normalized = (normalizedY - NORMALIZED_Y_MIN) / normRange;
    return m_y_min + normalized * (m_y_max - m_y_min);
}


// --- Undo/Redo용 내부 함수 ---
int MotorProfile::internalAddNode(const MotionNode& node) {
    m_nodes.append(node);
    sortNodes();
    return m_nodes.indexOf(node);
}
void MotorProfile::internalRemoveNode(int index) {
    if (index >= 0 && index < m_nodes.size()) {
        m_nodes.remove(index);
    }
}
void MotorProfile::internalMoveNode(int index, const MotionNode& pos) {
    if (index >= 0 && index < m_nodes.size()) {
        m_nodes[index] = pos;
    }
}
void MotorProfile::sortNodes() {
    std::sort(m_nodes.begin(), m_nodes.end(), [](const MotionNode& a, const MotionNode& b) {
        return a.x() < b.x();
    });
}
void MotorProfile::emitDataChanged() {
    emit dataChanged();
}

// --- 3번: YAML 내보내기 함수 구현 ---
void MotorProfile::exportNodesToYAML(YAML::Emitter& emitter) const {
    emitter << YAML::BeginSeq;
    for (const MotionNode& node : m_nodes) {
        emitter << YAML::Flow << YAML::BeginSeq << node.x() << node.y() << YAML::EndSeq;
    }
    emitter << YAML::EndSeq;
}

void MotorProfile::exportSamplesToYAML(YAML::Emitter& emitter, double sampleRateHz, double endTime_ms) const {
    emitter << YAML::BeginSeq;
    if (sampleRateHz <= 0 || endTime_ms < 0) {
        emitter << YAML::EndSeq;
        return;
    }

    double dt_ms = 1000.0 / sampleRateHz; // 샘플링 간격 (ms)

    for (double time_ms = 0.0; time_ms <= endTime_ms; time_ms += dt_ms) {
        double value = sampleAt(time_ms);
        emitter << YAML::Flow << YAML::BeginSeq << time_ms << value << YAML::EndSeq;
    }

    double lastSampleTime = (qFloor(endTime_ms / dt_ms)) * dt_ms;
    if (endTime_ms - lastSampleTime > 1e-6 && endTime_ms > 0.0) {
        double value = sampleAt(endTime_ms);
        emitter << YAML::Flow << YAML::BeginSeq << endTime_ms << value << YAML::EndSeq;
    } else if (endTime_ms == 0.0 && dt_ms > 0.0) {
        double value = sampleAt(0.0);
        emitter << YAML::Flow << YAML::BeginSeq << 0.0 << value << YAML::EndSeq;
    }

    emitter << YAML::EndSeq;
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
void MotionDocument::setActiveMotor(MotorProfile* profile) {
    if (profile != m_activeProfile) {
        MotorProfile* oldActive = m_activeProfile;
        m_activeProfile = profile;
        emit activeMotorChanged(m_activeProfile, oldActive);
    }
}
void MotionDocument::removeMotor(MotorProfile* profile) {
    if (!profile) return;
    int index = m_profiles.indexOf(profile);
    if (index == -1) return;
    m_profiles.removeAt(index);
    if (m_activeProfile == profile) {
        m_activeProfile = nullptr;
        if (!m_profiles.isEmpty()) {
            setActiveMotor(m_profiles.first());
        } else {
            setActiveMotor(nullptr);
        }
    }
    emit modelChanged();
    profile->deleteLater();
}
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
        QColor color = QColor::fromHsv(qrand() % 360, 200, 200);
        MotorProfile* profile = new MotorProfile("", color, this);
        profile->read(motorObj);
        m_profiles.append(profile);
        emit motorAdded(profile);
    }
    emit modelChanged();
    if (!m_profiles.isEmpty()) {
        setActiveMotor(m_profiles.first());
    }
    return true;
}

// --- Export 로직 (CSV용 - 시간 단위 ms로 변경) ---
bool MotionDocument::exportNodesToStream(QTextStream& stream) const {
    if (!m_activeProfile) return false;
    stream << "Motor: " << m_activeProfile->name() << "\n";
    stream << "Time (ms),Value\n"; // 단위 변경
    for (const MotionNode& node : m_activeProfile->nodes()) {
        stream << node.x() << "," << node.y() << "\n";
    }
    return true;
}
bool MotionDocument::exportSamplesToStream(QTextStream& stream, double sampleRateHz, double endTime_ms) const {
    if (!m_activeProfile || sampleRateHz <= 0) return false;
    stream << "Motor: " << m_activeProfile->name() << "\n";
    stream << "SampleRate: " << sampleRateHz << " Hz\n";
    stream << "Time (ms),Value\n"; // 단위 변경
    double dt_ms = 1000.0 / sampleRateHz; // ms 단위
    if (m_activeProfile->nodes().isEmpty()) return true;
    double startTime_ms = 0.0; // 0ms부터
    if (endTime_ms < startTime_ms) endTime_ms = m_activeProfile->nodes().last().x();
    for (double time_ms = startTime_ms; time_ms <= endTime_ms; time_ms += dt_ms) {
        double value = m_activeProfile->sampleAt(time_ms);
        stream << time_ms << "," << value << "\n";
    }
     double lastSampleTime = (qFloor(endTime_ms / dt_ms)) * dt_ms;
     if (endTime_ms - lastSampleTime > 1e-6 && endTime_ms > 0.0) {
        double value = m_activeProfile->sampleAt(endTime_ms);
        stream << endTime_ms << "," << value << "\n";
    }
    return true;
}

// 3번: YAML 내보내기 함수 구현
bool MotionDocument::exportDocumentToYAML(YAML::Emitter& emitter, bool sample, double sampleRateHz, double endTime_ms) const
{
    emitter << YAML::BeginMap;
    for(MotorProfile* profile : m_profiles) {
        emitter << YAML::Key << profile->name().toStdString(); // 모터 이름을 키로 사용
        emitter << YAML::Value;
        if (sample) {
            profile->exportSamplesToYAML(emitter, sampleRateHz, endTime_ms);
        } else {
            profile->exportNodesToYAML(emitter);
        }
    }
    emitter << YAML::EndMap;
    return emitter.good();
}