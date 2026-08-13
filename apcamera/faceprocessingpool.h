#ifndef FACEPROCESSINGPOOL_H
#define FACEPROCESSINGPOOL_H

#include <QObject>
#include "faceprocessing.h"

#define THREADS     4

class FaceProcessingPool : public QObject
{
    Q_OBJECT
public:
    explicit FaceProcessingPool(QObject *parent = nullptr);
    ~FaceProcessingPool();

    cv::Ptr<cv::FaceDetectorYN> m_detector;
    cv::Ptr<cv::FaceRecognizerSF> m_recognizer;

    //объект определения рабочего положения
    FaceProcessing *m_procFace[THREADS];

    //обьект распознавателя
    DETECTED_FACE m_detectFace;
    //распознанные лица
    QVector<FACE_INFO> m_faceInfo;

signals:
    //сигналы окончания обработки кадра
    void imageProcessed(cv::Mat mat);

public slots:
    //принять кадр от камеры для обработки
    void processFrame(cv::Mat mat);

protected slots:
    //лицо определено
    void faceProcessed(FaceProcessing *obj);

protected:
    //потоки обработки
    QThread *m_threads[THREADS];
};

#endif // FACEPROCESSINGPOOL_H
