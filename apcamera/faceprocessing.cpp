#include "faceprocessing.h"

FaceProcessing::FaceProcessing(QObject *parent)
    : QObject{parent}
{
    m_busy = false;
}

void FaceProcessing::processFrame(cv::Mat mat)
{
    if(!thread()->isRunning()) { return; } //не помешает
    if(m_busy) { return; } //хотя по логике программы исключено

    //здесь мы находимся в основном потоке, и с этого момента
    //обработчик считается занятым, его освободит пул при получении
    //от сюда сигнала processingFinished() и это тоже будет
    //в основном потоке, так что синхронизация не требуется
    m_busy = true;
    //делаем локальную копию картинки
    m_detectFace.matBgr = mat.clone();
    //запускаем обработку через межпоточный вызов
    QTimer::singleShot(0, this, SLOT(processing()));
}

void FaceProcessing::processing()
{
    m_detectFace.faceDetect();
    emit processingFinished(this);
}
