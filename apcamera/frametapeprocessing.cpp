#include "frametapeprocessing.h"
#include "qdebug.h"

FrameTapeProcessing::FrameTapeProcessing(QObject *parent) : QObject(parent)
{
    m_busy = false;
}

void FrameTapeProcessing::processFrame(cv::Mat mat)
{
    if(!thread()->isRunning()) { return; } //не помешает
    if(m_busy) { return; } //хотя по логике программы исключено

    //здесь мы находимся в основном потоке, и с этого момента
    //обработчик считается занятым, его освободит пул при получении
    //от сюда сигнала processingFinished() и это тоже будет
    //в основном потоке, так что синхронизация не требуется
    m_busy = true;
    //делаем местную копию картинки
    m_tapePos.matBgr = mat.clone();

    //запускаем обработку через межпоточный вызов
    //(таймер для этого - наиболее простой вариант)
    QTimer::singleShot(0, this, SLOT(processing()));
}

void FrameTapeProcessing::processing()
{
    if(m_tapePos.tapeDetect()) {
        //если лента зафикисирована
        m_tapePos.tapeWorkPosition();
    }
    emit processingFinished(this);
}

