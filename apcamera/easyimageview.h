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
    //сделать копию картинки и отобразим ее
    void updateImage(cv::Mat mat);
    void updateResult(const WeildDetect::Result &result);
    //void updateFaces(QVector<FACE_INFO> *faces);

protected:
    virtual void paintEvent(QPaintEvent *event);

    // Заголовок изображения; данные удерживаются в m_matBgr.
    QImage m_imageHdr;
    // Mono8 хранится без преобразования; цветной кадр преобразуется при рисовании.
    cv::Mat m_matBgr;

    // GUI-thread mailbox: a new result replaces the previous unpainted frame.
    cv::Mat m_pendingImage;
    std::vector<cv::Point2f> m_laserPixels;
    QString m_status;
    bool m_detected = false;
    bool m_imageDirty = false;


    QElapsedTimer m_tmFps;
    float m_fps;
};

#endif // EASYIMAGEVIEW_H
