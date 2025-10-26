#pragma once

#include <QObject>
#include <QVector>
#include <QPointF>
#include <QString>
#include <QColor>
#include <QJsonObject>
#include <QVariant>
#include <QTextStream> // For export/import

using MotionNode = QPointF; // Alias for node data type

/**
 * @brief Represents the data for a single motor's motion profile.
 * Stores nodes in REAL coordinates (ms, value).
 */
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

    // Calculates interpolated value at a specific time
    double sampleAt(double time) const;

    // --- Public internal functions for Undo/Redo ---
    int internalAddNode(const MotionNode& node);
    void internalRemoveNode(int index);
    void internalMoveNode(int index, const MotionNode& pos);
    void sortNodes(); // Sorts nodes by X-coordinate (time)
    void emitDataChanged(); // Emits dataChanged signal

public slots:
    // Setters for constraints
    void setYMin(double val);
    void setYMax(double val);
    void setMaxSlope(double val);
    // Applies Y min/max constraints to nodes
    void checkAllNodes();

signals:
    void dataChanged(); // Emitted when node data changes
    void constraintsChanged(); // Emitted when constraint properties change

private:
    // Basic validation check
    bool isNodeValid(const MotionNode& node, int indexToIgnore = -1) const;

    QString m_name;
    QColor m_color;
    QVector<MotionNode> m_nodes; // Stores nodes sorted by X

    // Constraint values
    double m_y_min = -100.0; // Default Y Min
    double m_y_max = 100.0;  // Default Y Max
    double m_max_slope = 1000.0; // Default Max Slope (units: Y-unit / ms)
};


/**
 * @brief Represents the overall document containing multiple motor profiles.
 */
class MotionDocument : public QObject {
    Q_OBJECT

public:
    explicit MotionDocument(QObject* parent = nullptr);
    ~MotionDocument(); // Cleans up motor profiles

    // Getters
    const QVector<MotorProfile*>& motorProfiles() const { return m_profiles; }
    MotorProfile* activeProfile() const { return m_activeProfile; }
    int activeProfileIndex() const;

    // YAML file operations
    bool saveToYAML(const QString& filename, const QString& id) const;
    bool loadFromYAML(const QString& filename);

public slots:
    // Document modification
    MotorProfile* addMotor(const QString& name, QColor color);
    void setActiveMotor(MotorProfile* profile);
    void removeMotor(MotorProfile* profile);

signals:
    void motorAdded(MotorProfile* profile);
    void documentCleared(); // Signal before loading new data
    void activeMotorChanged(MotorProfile* active, MotorProfile* previous);
    void modelChanged(); // Generic signal for add/remove motor

private:
    QVector<MotorProfile*> m_profiles; // List of all profiles
    MotorProfile* m_activeProfile = nullptr; // Pointer to the active one
};

// Allow storing MotorProfile* in QVariant
Q_DECLARE_METATYPE(MotorProfile*)
