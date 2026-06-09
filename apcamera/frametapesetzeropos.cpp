#include "frametapesetzeropos.h"

FrameTapeSetZeroPos::FrameTapeSetZeroPos(QObject *parent) : QObject(parent)
{
    m_busy = false;
    //m_setZeroPosition = false;
    //m_zeroPos = nullptr;
}

void FrameTapeSetZeroPos::processFrame(cv::Mat mat)
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
    //processing();
}

void FrameTapeSetZeroPos::processing()
{
    if(m_tapePos.tapeDetect()) {
        m_tapePos.tapeStandPosition();
    }
    emit processingFinished(this);
}

