#ifndef FACEPROCESSING_H
#define FACEPROCESSING_H

#include <QObject>
#include "appstructs.h"
#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>

class FaceProcessing : public QObject
{
    Q_OBJECT
public:
    explicit FaceProcessing(QObject *parent = nullptr);
    //флаг занятости
    bool m_busy;

    //обьект детектора
    DETECTED_FACE m_detectFace;

signals:
    //сигнал окончания обработки
    void processingFinished(FaceProcessing *obj);

public slots:
    //начать обработку картинки
    void processFrame(cv::Mat mat);

protected slots:
    //начать обработку картинки: будет сделана внутренняя копия кадра, и
    //через QTimer::singleShot(), то есть межпоточно, запущена обработка
    //кадра. Слот processing() работает в собственном потоке.
    void processing();

};

#endif // FACEPROCESSING_H
