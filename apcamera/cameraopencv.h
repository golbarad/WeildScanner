#ifndef CAMERAOPENCV_H
#define CAMERAOPENCV_H

/*
    CameraOpenCv - класс объекта камеры

*/

#include <QObject>
#include <QThread>
#include <QTimer>
#include <QMutex>
#include <QWaitCondition>
#include <QApplication>
#include <QFileInfo>

#include <opencv2/opencv.hpp>
#include "logger/logger.h"


class CameraOpenCv : public QObject
{
    Q_OBJECT
public:
    explicit CameraOpenCv(QObject *parent = nullptr);
    ~CameraOpenCv();

    //сохранить параметры камеры
    void saveCameraParams();

    //открыть и стартовать камеру cv::VideoCapture
    void startCamera(int id, int fps);
    //остановить и закрыть камеру
    void stopCamera();

    //идентификатор камеры openCv
    int m_cameraId;
    //интервал таймера захвата
    int m_captureInterval;
    //счет захваченных кадров
    quint32 m_frameCounter;

signals:
    //камера захватила новый кадр
    void frameCap(cv::Mat mat);

protected slots:
    //перевызов соотв открытых функций
    void slotStartCamera();
    void slotStopCamera();

    //захватить новый кадр
    void captureCameraFrame();

protected:
    //синхронизация выполнения слотов
    QMutex m_mutexWaitSlot;
    QWaitCondition m_waitSlot;

    //объект захвата кадров
    cv::VideoCapture m_cap;
    //таймер захвата кадров
    QTimer *m_timerCap;
    //кадр в формате cv::Mat
    cv::Mat m_matFrame;

    //собственный поток
    QThread *m_thread;
};

Q_DECLARE_METATYPE(cv::Mat)

#endif // CAMERAOPENCV_H
