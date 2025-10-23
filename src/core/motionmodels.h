#pragma once

#include <QObject>
#include <QVector>
#include <QPointF>
#include <QString>
#include <QColor>
#include <QJsonObject>
#include <QVariant>
#include <QTextStream> // Export용
#include <qmath.h> // qMax 사용

// 3번: MotionNode의 X 좌표 단위는 밀리초(ms)입니다.
using MotionNode = QPointF;

const double NORMALIZED_Y_MIN = 0.0;
const double NORMALIZED_Y_MAX = 100.0;

// 3번: YAML 헤더 포함
#include <yaml-cpp/yaml.h>

class MotorProfile : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString name READ name)
    Q_PROPERTY(QColor color READ color)

public:
    explicit MotorProfile(const QString& name, QColor color, QObject* parent = nullptr);

    // Getters
    const QString& name() const { return m_name; }
    const QColor& color() const { return m_color; }
    const QVector<MotionNode>& nodes() const { return m_nodes; }
    double yMin() const { return m_y_min; }
    double yMax() const { return m_y_max; }
    double maxSlope() const { return m_max_slope; }
    int nodeCount() const { return m_nodes.size(); }
    MotionNode nodeAt(int index) const;

    // 2번: 최대 절대 Y값 계산 함수
    double getMaxAbsY() const { return qMax(qAbs(m_y_min), qAbs(m_y_max)); }

    // Export를 위한 선형 보간 함수 (time: ms)
    double sampleAt(double time_ms) const;

    // JSON 직렬화/역직렬화 (x: ms)
    void read(const QJsonObject& json);
    void write(QJsonObject& json) const;
    void exportSamplesToJSON(QJsonObject& json, double sampleRateHz, double endTime_ms) const; // 시간 단위 ms로 변경

    // 3번: YAML 내보내기 함수 (시간 단위 ms로 변경)
    void exportNodesToYAML(YAML::Emitter& emitter) const;
    void exportSamplesToYAML(YAML::Emitter& emitter, double sampleRateHz, double endTime_ms) const;

    // Y축 정규화 변환 함수
    double getNormalizedY(double realY) const;
    double getRealY(double normalizedY) const;

    // --- Undo/Redo를 위한 공개 내부 함수 (node.x(): ms) ---
    int internalAddNode(const MotionNode& node);
    void internalRemoveNode(int index);
    void internalMoveNode(int index, const MotionNode& pos);
    void sortNodes();
    void emitDataChanged();

public slots:
    void setYMin(double val);
    void setYMax(double val);
    void setMaxSlope(double val);
    void checkAllNodes();

signals:
    void dataChanged();
    void constraintsChanged();

private:
    bool isNodeValid(const MotionNode& node, int indexToIgnore) const;

    QString m_name;
    QColor m_color;
    QVector<MotionNode> m_nodes; // x: ms, y: real

    double m_y_min = -100.0;
    double m_y_max = 100.0;
    double m_max_slope = 1.0; // 기울기 단위: unit/ms
};

class MotionDocument : public QObject {
    Q_OBJECT
public:
    explicit MotionDocument(QObject* parent = nullptr);
    ~MotionDocument();

    const QVector<MotorProfile*>& motorProfiles() const { return m_profiles; }
    MotorProfile* activeProfile() const { return m_activeProfile; }
    int activeProfileIndex() const;

    // Export 로직 (시간 단위 ms로 변경)
    bool exportNodesToStream(QTextStream& stream) const;
    bool exportSamplesToStream(QTextStream& stream, double sampleRateHz, double endTime_ms) const;
    // 3번: YAML 내보내기 함수
    bool exportDocumentToYAML(YAML::Emitter& emitter, bool sample, double sampleRateHz, double endTime_ms) const;

public slots:
    MotorProfile* addMotor(const QString& name, QColor color);
    void setActiveMotor(MotorProfile* profile);
    void removeMotor(MotorProfile* profile);

    bool saveToFile(const QString& filename) const;
    bool loadFromFile(const QString& filename);

signals:
    void motorAdded(MotorProfile* profile);
    void documentCleared();
    void activeMotorChanged(MotorProfile* active, MotorProfile* previous);
    void modelChanged();

private:
    QVector<MotorProfile*> m_profiles;
    MotorProfile* m_activeProfile = nullptr;
};

Q_DECLARE_METATYPE(MotorProfile*)