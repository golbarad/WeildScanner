#pragma once

#include <QObject>
#include <QSize>
#include <QString>
#include <variant>
#include "cvmetatypes.h"

class ICamera : public QObject
{
    Q_OBJECT
public:
    enum class OperationResult { Success, Failed, Timeout, WrongThread, ThreadNotRunning, UnsupportedSource, Pending };

    struct Device { int id; int fps; QSize size = QSize(640,480); };
    struct Video { QString path; int fallbackFps = 60; };
    struct Synthetic { int fps = 80; };
    struct Network {
        QString address = QStringLiteral("192.168.0.146");
        int fps = 10;
        quint16 port = 11681;
        int timeoutMs = 1000;
    };
    using Source = std::variant<Device, Video, Synthetic, Network>;

    ~ICamera() override = default;
    // Call from the owning code's thread, not a capture worker. Pending means queued.
    virtual OperationResult start(const Source &source) = 0;
    virtual OperationResult stopCamera() = 0;

signals:
    void frameCaptured(cv::Mat image, qint64 timestampNs);
    void captureError(QString error);
    void sourceFinished();

protected:
    explicit ICamera(QObject *parent = nullptr) : QObject(parent)
    {
        qRegisterMetaType<cv::Mat>("cv::Mat");
    }
};
