#pragma once
#include <QObject>
#include "weilddetect.h"


/*
    Класс инкапсулирующий упралвение сканированием
*/
class FrameWeildProcessing : public QObject
{
    Q_OBJECT
public:
    explicit FrameWeildProcessing(QObject *parent = nullptr) : QObject(parent) {}
    //запускаем анализ фрейма в отдельном потоке
    void processFrame(cv::Mat image, quint64 id, qint64 timestamp, quint64 generation,
                      WeildDetect::Parameters parameters, WeildDetect::Calibration calibration);
signals:
    void processingFinished(WeildDetect::Result result);
private:
    WeildDetect m_detect;
};
