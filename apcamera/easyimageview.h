#ifndef EASYIMAGEVIEW_H
#define EASYIMAGEVIEW_H

/*
    EasyImageView - простой просмотрщик кадров cv::Mat

    Взято почти без исправлений из этого проекта:
    home/jury/scanenergy/autopilot/app/src/
*/

#include <QWidget>
#include <QPainter>
#include <QElapsedTimer>
#include "appstructs.h"
#include <opencv2/opencv.hpp>

class EasyImageView : public QWidget
{
    Q_OBJECT
public:
    explicit EasyImageView(QWidget *parent = nullptr);
    QVector<FACE_INFO> *m_faces;

signals:

public slots:
    //сделать копию картинки и отобразим ее
    void updateImage(cv::Mat mat);
    //void updateFaces(QVector<FACE_INFO> *faces);

protected:
    virtual void paintEvent(QPaintEvent *event);

    //картинка-заголовок для данных m_matBgr
    QImage m_imageHdr;
    //при получении картинки в слоте updateImage()
    //будет произведено преобразование из BGR в RGB
    //в эту картинку и она хранит данные для m_imageHdr
    cv::Mat m_matBgr;


    QElapsedTimer m_tmFps;
    float m_fps;
};

#endif // EASYIMAGEVIEW_H
