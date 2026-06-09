#ifndef FRAMETAPESETZEROPOS_H
#define FRAMETAPESETZEROPOS_H

#include <QObject>

#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>

#include "appstructs.h"

class   FrameTapeSetZeroPos : public QObject
{
    Q_OBJECT
public:
    explicit FrameTapeSetZeroPos(QObject *parent = nullptr);

    //флаг занятости
    bool m_busy;

    //данные текущей позиции ленты
    TAPE_POSITION m_tapePos;

signals:
    //сигнал окончания обработки
    void processingFinished(FrameTapeSetZeroPos *obj);

public slots:
    //начать обработку картинки,
    //(этот класс работает в основном потоке)
    void processFrame(cv::Mat mat);

protected slots:
    //начать обработку картинки: будет сделана внутренняя копия кадра, и
    //через QTimer::singleShot(), то есть межпоточно, запущена обработка
    //кадра. Слот processing() работает в собственном потоке.
    void processing();

};

#endif // FRAMETAPESETZEROPOS_H
