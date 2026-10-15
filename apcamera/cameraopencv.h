#pragma once
#include <QObject>
#include <QThread>
#include <QTimer>
#include <QSize>
#include <QDeadlineTimer>
#include <functional>
#include <opencv2/opencv.hpp>
#include <QMutex>
#include <QMutexLocker>
#include <QSharedPointer>
#include <QWaitCondition>
#include "cvmetatypes.h"
#include "logger/logger.h"

class CameraOpenCv : public QObject
{
    Q_OBJECT
public:
    enum class OperationResult { Success, Failed, Timeout, WrongThread, ThreadNotRunning };
    explicit CameraOpenCv(QObject *parent = nullptr);
    ~CameraOpenCv() override;
    void saveCameraParams();
    // Called from the owner thread. Timeout stops waiting, not the queued operation.
    OperationResult startCamera(int id, int fps, QSize size = QSize(640,480));
    OperationResult startVideo(const QString &path, int fallbackFps = 60);
    OperationResult startSynthetic(int fps = 80);
    OperationResult stopCamera();

    int m_cameraId=0;
    int m_captureInterval=12;
    quint64 m_frameCounter=0;
signals:
    void frameCap(cv::Mat image);
    void frameCaptured(cv::Mat image, qint64 timestampNs);
    void captureError(QString error);
    void sourceFinished();
private:

    //enum инкапсулирующий источник данныъх
    enum class Source { Stopped, Camera, Video, Synthetic };
    Source m_source=Source::Stopped;
    cv::VideoCapture m_cap;
    QTimer *m_timerCap=nullptr;

    //Структура управляющая conditon осущетсвляющим блокировку
    //потока для управления камерой
    struct Completion {
        QMutex mutex;
        QWaitCondition condition;
        bool done=false;
        bool success=false;
    };


    QThread *m_thread=nullptr;


    OperationResult runInCameraThread(std::function<bool()> operation,
                                      QDeadlineTimer deadline = QDeadlineTimer(5000));
    void stopInThread();
    void captureCameraFrame();
    void startTimer(int fps);
};
