#include "easyimageview.h"
#include <QThread>
#include <QPolygonF>

namespace {
QRect drawFrame(QPainter &painter, const QImage &image, const QRect &bounds)
{
    if (image.isNull() || bounds.isEmpty()) return {};
    QRect imageRect(QPoint(), image.size().scaled(bounds.size(), Qt::KeepAspectRatio));
    if (imageRect.isEmpty()) return {};
    imageRect.moveCenter(bounds.center());
    painter.drawImage(imageRect, image);
    return imageRect;
}
}

EasyImageView::EasyImageView(QWidget *parent) : QWidget(parent)
{

    m_fps = 0;
    m_tmFps.restart();
}

void EasyImageView::updateImage(cv::Mat mat)
{
    WeildDetect::Result result;
    // Клонируем кадры
    result.overlay.left_frame = mat.clone();
    result.overlay.right_frame = mat.clone();
    updateResult(result);
    m_status.clear();
}

void EasyImageView::updateResult(const WeildDetect::Result &result)
{
    Q_ASSERT(QThread::currentThread() == thread());
    m_pendingFrames = result.overlay;
    m_laserPixels = result.laserPixels;
    m_landmarkPixels = result.landmarkPixels;
    m_detected = result.detected;
    m_status = result.overlay.empty() ? QString() :
        (result.predicted ? QStringLiteral("PREDICTED") :
         result.detected ? QStringLiteral("MEASURED") : QStringLiteral("LOST / UNCALIBRATED"));
    m_imageDirty = true;
    update();
}

void EasyImageView::RenderFrame::setPixels(const cv::Mat &mat)
{
    image = QImage();
    pixels.release();
    if (mat.empty() || mat.depth() != CV_8U) return;
    auto format = QImage::Format_RGB888;
    if (mat.channels() == 1) {
        pixels = mat;
        format = QImage::Format_Grayscale8;
    } else if (mat.channels() == 3) {
        cv::cvtColor(mat, pixels, cv::COLOR_BGR2RGB);
    } else if (mat.channels() == 4) {
        cv::cvtColor(mat, pixels, cv::COLOR_BGRA2RGB);
    }
    if (!pixels.empty()) {
        image = QImage(static_cast<const uchar *>(pixels.data),
            pixels.cols, pixels.rows, int(pixels.step), format);
    }
}

// Отрисовка найденных линий
void EasyImageView::drawOverlay(RenderFrame &frame, QPainter &painter, const QRect &rcImage) const
{
    painter.save();
    painter.setClipRect(rcImage);
    painter.translate(rcImage.topLeft());
    painter.scale(double(rcImage.width()) / frame.image.width(),
                  double(rcImage.height()) / frame.image.height());
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
    painter.setPen(QPen(QColor(255, 180, 0), 2));
    for (const auto &point : m_landmarkPixels)
        painter.drawEllipse(QPointF(point.x + 0.5, point.y + 0.5), 5, 5);
    painter.restore();
}

void EasyImageView::paintEvent(QPaintEvent */*event*/)
{
    if (m_imageDirty) {
        m_leftFrame.setPixels(m_pendingFrames.left_frame);
        m_rightFrame.setPixels(m_pendingFrames.right_frame);
        if (!m_leftFrame.image.isNull() || !m_rightFrame.image.isNull()) {
            const qint64 ms = qMax(qint64(1), m_tmFps.restart());
            m_fps = m_fps * 0.9f + 1000.0f / ms * 0.1f;
        }
        m_imageDirty = false;
    }
    if (rect().isEmpty() || (m_leftFrame.image.isNull() && m_rightFrame.image.isNull())) return;

    QPainter painter(this);
    QRect leftBounds = rect(), rightBounds = rect();
    if (!m_leftFrame.image.isNull() && !m_rightFrame.image.isNull()) {
        const int gap = width() > 8 ? 4 : 0;
        leftBounds.setWidth((width() - gap) / 2);
        rightBounds.setLeft(leftBounds.width() + gap);
    }
    // Рисуем изображение с каждой камеры
    const QRect leftRect = drawFrame(painter, m_leftFrame.image, leftBounds);
    const QRect rightRect = drawFrame(painter, m_rightFrame.image, rightBounds);
    if (!leftRect.isEmpty()) drawOverlay(m_leftFrame, painter, leftRect);
    if (!leftRect.isEmpty()) drawOverlay(m_rightFrame, painter, rightRect);

    painter.setFont(QFont("monospace", 11));
    painter.setPen(qRgb(0, 196, 0));
    painter.drawText(0, 20, QString("FPS: %0").arg(m_fps, 4, 'f', 2));
    const auto drawSize = [&](const QImage &image, const QRect &bounds) {
        painter.save();
        painter.setClipRect(bounds);
        painter.drawText(bounds.left(), 40, QStringLiteral("%1x%2").arg(image.width()).arg(image.height()));
        painter.restore();
    };
    if (!leftRect.isEmpty()) drawSize(m_leftFrame.image, leftBounds);
    if (!rightRect.isEmpty()) drawSize(m_rightFrame.image, rightBounds);
    const QRect statusRect = leftRect.isEmpty() ? rightRect : leftRect;
    if (!statusRect.isEmpty())
    {
        painter.setClipRect(statusRect);
        painter.setPen(m_detected ? QColor(0, 220, 0) : QColor(255, 100, 0));
        painter.drawText(statusRect.left() + 8, statusRect.bottom() - 12, m_status);
    }
}
