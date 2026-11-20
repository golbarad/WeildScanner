#pragma once
#include <QThread>
#include <QTimer>
#include <QDeadlineTimer>
#include <functional>
#include <opencv2/opencv.hpp>
#include <QMutex>
#include <QMutexLocker>
#include <QSharedPointer>
#include <QWaitCondition>
#include "icamera.h"

/*
    Класс инкапсулирующий работу с кмерой opencv
*/
class CameraOpenCv : public ICamera
{
    Q_OBJECT
public:
    explicit CameraOpenCv(QObject *parent = nullptr);
    ~CameraOpenCv() override;
    // Timeout stops waiting, not the queued operation.
    OperationResult start(const Source &source) override;
    OperationResult stopCamera() override;

signals:
    void frameCap(cv::Mat image);
private:

    //enum инкапсулирующий источник данныъх
    enum class Mode { Stopped, Camera, Video, Synthetic };
    Mode m_mode=Mode::Stopped;
    quint64 m_frameCounter=0;
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
    OperationResult startCamera(int id, int fps, QSize size);
    OperationResult startVideo(const QString &path, int fallbackFps);
    OperationResult startSynthetic(int fps);
    void stopInThread();
    void captureCameraFrame();
    void startTimer(int fps);
};
