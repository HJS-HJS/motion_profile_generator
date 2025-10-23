#pragma once

#include <QObject>
#include <QVector>
#include <QPointF>
#include <QString>
#include <QColor>
#include <QJsonObject>
#include <QVariant> // Q_DECLARE_METATYPE을 위해 추가

// 5. 노드 (시간-x, 값-y)
using MotionNode = QPointF; 

// 8. 개별 모터 프로파일
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

    // 7. 제약 조건 Getters
    double yMin() const { return m_y_min; }
    double yMax() const { return m_y_max; }
    double maxSlope() const { return m_max_slope; }

    int nodeCount() const { return m_nodes.size(); }
    MotionNode nodeAt(int index) const;

    // JSON 직렬화/역직렬화
    void read(const QJsonObject& json);
    void write(QJsonObject& json) const;

public slots:
    // 7. 제약 조건 Setters
    void setYMin(double val);
    void setYMax(double val);
    void setMaxSlope(double val);

    // 2, 5. 노드 관리
    bool addNode(const MotionNode& node);
    bool updateNode(int index, const MotionNode& node);
    void deleteNode(int index);

signals:
    // 뷰(View)가 이 신호들을 구독하여 그래프를 갱신합니다.
    void dataChanged(); // 노드가 추가, 삭제, 수정될 때
    void constraintsChanged(); // 제약조건이 변경될 때

private:
    // 7. 노드 추가/수정 시 호출할 제약조건 검사 함수
    bool isNodeValid(const MotionNode& node, int indexToIgnore) const;
    void sortNodes(); // 노드를 시간순으로 정렬

    QString m_name;
    QColor m_color;
    QVector<MotionNode> m_nodes;

    double m_y_min = -100.0;
    double m_y_max = 100.0;
    double m_max_slope = 1000.0;
};

// 10. 전체 모터 프로파일을 관리하는 문서 클래스
class MotionDocument : public QObject {
    Q_OBJECT

public:
    explicit MotionDocument(QObject* parent = nullptr);
    ~MotionDocument();

    const QVector<MotorProfile*>& motorProfiles() const { return m_profiles; }
    MotorProfile* activeProfile() const { return m_activeProfile; }
    int activeProfileIndex() const;

public slots:
    MotorProfile* addMotor(const QString& name, QColor color);
    // 8. QTreeWidget에서 사용하기 쉽도록 포인터 기반으로 변경
    void setActiveMotor(MotorProfile* profile); 
    
    // 11, 12. 저장 및 불러오기
    bool saveToFile(const QString& filename) const;
    bool loadFromFile(const QString& filename);

signals:
    void motorAdded(MotorProfile* profile);
    void documentCleared(); // 불러오기 전에 호출
    void activeMotorChanged(MotorProfile* active, MotorProfile* previous);
    void modelChanged(); // 저장/불러오기 후

private:
    QVector<MotorProfile*> m_profiles;
    MotorProfile* m_activeProfile = nullptr;
};

// QTreeWidget에서 QVariant로 사용하기 위해 MetaType으로 선언
Q_DECLARE_METATYPE(MotorProfile*)

