#include "easyimageview.h"

EasyImageView::EasyImageView(QWidget *parent) : QWidget(parent)
{
    m_faces = NULL;
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

// void EasyImageView::updateFaces(QVector<FACE_INFO> *faces)
// {
//     m_faces = faces;
//     update();
// }

void EasyImageView::paintEvent(QPaintEvent */*event*/)
{
    if(rect().height() == 0 || rect().width() == 0) { return; }
    if(m_imageHdr.height() == 0 || m_imageHdr.width() == 0) { return; }

    QPainter painter(this);

    //это простой отображатель, учтем только соотношение сторон
    float aspWgt = (float)rect().width() / rect().height();
    float aspImg = (float)m_imageHdr.width() / m_imageHdr.height();
    QRect rcImage;
    if(aspWgt >= aspImg) {
        int w = aspImg * rect().height(), h = rect().height();
        int x = (rect().width() - w) / 2, y = 0;
        rcImage = QRect(x, y, w, h);
        painter.drawImage(rcImage, m_imageHdr);
    } else {
        int w = width(), h = rect().width() / aspImg;
        int x = 0, y = (rect().height() - h) / 2;
        rcImage = QRect(x, y, w, h);
        painter.drawImage(rcImage, m_imageHdr);
    }

    if(m_faces != NULL) {
        const double kx = (double)rcImage.width() / m_imageHdr.width();
        const double ky = (double)rcImage.height() / m_imageHdr.height();

        QPen penMatch(qRgb(0, 196, 0));
        penMatch.setWidth(2);
        QPen penNoMatch(qRgb(0, 196, 0));
        penNoMatch.setWidth(2);

        for(const FACE_INFO &face : *m_faces) {
            QRectF rcFace(
                rcImage.left() + face.rect.left() * kx,
                rcImage.top() + face.rect.top() * ky,
                face.rect.width() * kx,
                face.rect.height() * ky
            );

            painter.setPen(face.match ? penMatch : penNoMatch);
            painter.drawRect(rcFace);
        }
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
