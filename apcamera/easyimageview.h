#ifndef EASYIMAGEVIEW_H
#define EASYIMAGEVIEW_H

/*
    EasyImageView - простой просмотрщик кадров cv::Mat
*/

#include <QWidget>
#include <QPainter>
#include <QElapsedTimer>
#include "appstructs.h"
#include "weild_processor/weilddetect.h"
#include <opencv2/opencv.hpp>

class EasyImageView : public QWidget
{
    Q_OBJECT
public:
    explicit EasyImageView(QWidget *parent = nullptr);
    //QVector<FACE_INFO> *m_faces;

signals:

public slots:
    // Отображение фрейма
    void updateImage(cv::Mat mat);
    void updateResult(const WeildDetect::Result &result);
    //void updateFaces(QVector<FACE_INFO> *faces);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    struct RenderFrame {
        cv::Mat pixels;
        QImage image;
        void setPixels(const cv::Mat &mat);
    };
    void drawOverlay(RenderFrame &frame, QPainter &painter, const QRect &imageRect) const;
    RenderFrame m_leftFrame, m_rightFrame;

    // Текущий фрейм
    FrameSynchronizer::DualFrame m_pendingFrames;
    std::vector<cv::Point2f> m_laserPixels;
    std::vector<cv::Point2d> m_landmarkPixels;
    QString m_status;
    bool m_detected = false;
    bool m_imageDirty = false;


    QElapsedTimer m_tmFps;
    float m_fps;
};

#endif // EASYIMAGEVIEW_H
