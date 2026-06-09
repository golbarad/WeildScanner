#include "easyimageview.h"

EasyImageView::EasyImageView(QWidget *parent) : QWidget(parent)
{
    m_fps = 0;
    m_tmFps.restart();
}

void EasyImageView::updateImage(cv::Mat mat)
{
    //проверим матрицу картинки
    int cols = mat.cols; int rows = mat.rows;
    if(cols == 0 || rows == 0) { return; }

    //производим преобраование из BGR в RGB
    cv::cvtColor(mat, m_matBgr, cv::COLOR_BGR2RGB);

    //создаем картинку заголовок QImage из данных m_matBgr
    int w = m_matBgr.size().width, h = m_matBgr.size().height;
    m_imageHdr = QImage(m_matBgr.data, w, h, QImage::Format_RGB888);

    //считаем статистику работы камеры
    int ms = m_tmFps.elapsed(); if(ms == 0) { ms = 1000.0 / m_fps; }
    m_fps = m_fps * 0.9 + 1000.0 / ms * 0.1;
    m_tmFps.restart();

    update();
}

void EasyImageView::paintEvent(QPaintEvent */*event*/)
{
    if(rect().height() == 0 || rect().width() == 0) { return; }
    if(m_imageHdr.height() == 0 || m_imageHdr.width() == 0) { return; }

    QPainter painter(this);

    //это простой отображатель, учтем только соотношение сторон
    float aspWgt = (float)rect().width() / rect().height();
    float aspImg = (float)m_imageHdr.width() / m_imageHdr.height();
    if(aspWgt >= aspImg) {
        int w = aspImg * rect().height(), h = rect().height();
        int x = (rect().width() - w) / 2, y = 0;
        QRect rc(x, y, w, h);
        painter.drawImage(rc, m_imageHdr);
    } else {
        int w = width(), h = rect().width() / aspImg;
        int x = 0, y = (rect().height() - h) / 2;
        QRect rc(x, y, w, h);
        painter.drawImage(rc, m_imageHdr);
    }

    QFont font("monospace", 11); painter.setFont(font);
    //частота кадров
    painter.setPen(qRgb(0, 196, 0));
    painter.drawText(0, 20, QString("FPS: %0").arg(m_fps, 4, 'f', 2));
    //разрешение картинки
    painter.drawText(0, 40, QString("%0x%01")
                     .arg(m_imageHdr.width())
                     .arg(m_imageHdr.height()));
}
