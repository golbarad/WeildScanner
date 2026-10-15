#pragma once
#include <QObject>
#include <QJsonObject>
#include "weilddetect.h"

// Protocol intentionally unspecified. This stub NEVER sends data to hardware.
// A future adapter can consume packetReady and implement the agreed robot protocol.
class WeildRobotOutput : public QObject
{
    Q_OBJECT
public:
    explicit WeildRobotOutput(QObject *parent = nullptr) : QObject(parent) {}
    QJsonObject lastPacket() const { return m_lastPacket; }
    static QJsonObject packet(const WeildDetect::Result &result);
public slots:
    void publish(WeildDetect::Result result);
signals:
    void packetReady(QJsonObject packet);
private:
    QJsonObject m_lastPacket;
};
