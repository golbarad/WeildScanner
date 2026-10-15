#include "easyimageview.h"
#include <QThread>
#include <QPolygonF>

EasyImageView::EasyImageView(QWidget *parent) : QWidget(parent)
{

    m_fps = 0;
    m_tmFps.restart();
}

void EasyImageView::updateImage(cv::Mat mat)
{
    // Legacy callers may reuse their buffer after this slot returns.
    WeildDetect::Result result;
    result.overlay = mat.clone();
    updateResult(result);
    m_status.clear();
}

void EasyImageView::updateResult(const WeildDetect::Result &result)
{
    Q_ASSERT(QThread::currentThread() == thread());
    // Pool and view live in the GUI thread. No separate queued image/points pair.
    m_pendingImage = result.overlay;
    m_laserPixels = result.laserPixels;
    m_detected = result.detected;
    m_status = result.overlay.empty() ? QString() :
        (result.predicted ? QStringLiteral("PREDICTED") :
         result.detected ? QStringLiteral("MEASURED") : QStringLiteral("LOST / UNCALIBRATED"));
    m_imageDirty = true;
    update(); // Qt coalesces repaint requests; only the latest frame is painted.
}

// void EasyImageView::updateFaces(QVector<FACE_INFO> *faces)
// {
//     m_faces = faces;
//     update();
// }

void EasyImageView::paintEvent(QPaintEvent */*event*/)
{
    if (m_imageDirty) {
        m_imageHdr = QImage();
        m_matBgr.release();
        const auto &mat = m_pendingImage;
        if (!mat.empty() && mat.depth() == CV_8U) {
            QImage::Format format = QImage::Format_RGB888;
            if (mat.channels() == 1) {
                m_matBgr = mat;
                format = QImage::Format_Grayscale8;
            } else if (mat.channels() == 3) {
                cv::cvtColor(mat, m_matBgr, cv::COLOR_BGR2RGB);
            } else if (mat.channels() == 4) {
                cv::cvtColor(mat, m_matBgr, cv::COLOR_BGRA2RGB);
            }
            if (!m_matBgr.empty()) {
                m_imageHdr = QImage(static_cast<const uchar *>(m_matBgr.data),
                    m_matBgr.cols, m_matBgr.rows, int(m_matBgr.step), format);
                const qint64 ms = qMax(qint64(1), m_tmFps.restart());
                m_fps = m_fps * 0.9f + 1000.0f / ms * 0.1f;
            }
        }
        m_imageDirty = false;
    }
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

    painter.save();
    painter.setClipRect(rcImage);
    painter.translate(rcImage.topLeft());
    painter.scale(double(rcImage.width()) / m_imageHdr.width(),
                  double(rcImage.height()) / m_imageHdr.height());
    painter.setRenderHint(QPainter::Antialiasing);
    QPen linePen(QColor(0, 255, 0));
    linePen.setWidthF(1.0);
    linePen.setCosmetic(true);
    painter.setPen(linePen);
    QPolygonF segment;
    segment.reserve(int(m_laserPixels.size()));
    auto drawSegment = [&] {
        if (segment.size() == 1) painter.drawPoint(segment.front());
        else if (!segment.empty()) painter.drawPolyline(segment);
        segment.clear();
    };
    for (size_t i = 0; i < m_laserPixels.size(); ++i) {
        const auto &point = m_laserPixels[i];
        if (i > 0 && point.x - m_laserPixels[i - 1].x > 1.0f)
            drawSegment();
        segment.append(QPointF(point.x + 0.5, point.y + 0.5));
    }
    drawSegment();
    painter.restore();

    // if(m_faces != NULL) {
    //     const double kx = (double)rcImage.width() / m_imageHdr.width();
    //     const double ky = (double)rcImage.height() / m_imageHdr.height();

    //     QPen penMatch(qRgb(0, 196, 0));
    //     penMatch.setWidth(2);
    //     QPen penNoMatch(qRgb(0, 196, 0));
    //     penNoMatch.setWidth(2);

    //     for(const FACE_INFO &face : *m_faces) {
    //         QRectF rcFace(
    //             rcImage.left() + face.rect.left() * kx,
    //             rcImage.top() + face.rect.top() * ky,
    //             face.rect.width() * kx,
    //             face.rect.height() * ky
    //         );

    //         painter.setPen(face.match ? penMatch : penNoMatch);
    //         painter.drawRect(rcFace);
    //     }
    // }

    QFont font("monospace", 11); painter.setFont(font);
    //частота кадров
    painter.setPen(qRgb(0, 196, 0));
    painter.drawText(0, 20, QString("FPS: %0").arg(m_fps, 4, 'f', 2));
    //разрешение картинки
    painter.drawText(0, 40, QString("%0x%01")
                     .arg(m_imageHdr.width())
                     .arg(m_imageHdr.height()));
    painter.setPen(m_detected ? QColor(0, 220, 0) : QColor(255, 100, 0));
    painter.drawText(rcImage.left() + 8, rcImage.bottom() - 12, m_status);
}
