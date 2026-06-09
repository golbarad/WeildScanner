#ifndef FRAMEPROCESSINGPOOL_H
#define FRAMEPROCESSINGPOOL_H

#include <QObject>
#include <QThread>

#include "frametapesetzeropos.h"
#include "frametapeprocessing.h"

#define THREADS     2

class FrameProcessingPool : public QObject
{
    Q_OBJECT
public:
    explicit FrameProcessingPool(QObject *parent = nullptr);
    ~FrameProcessingPool();

    //указатель на параметры объекта
    //OBJECT_PARAMS *m_objParams;
    void setObjParams(OBJECT_PARAMS *params);

    //объект определения нулевой позиции камеры
    FrameTapeSetZeroPos *m_procZeroPos;
    //режим установки нулевой позиции
    bool m_zeroPositionMode;
    //следующий кадр установит нулевую позицию
    bool m_setZeroPosition;

    //объект нулевого положения
    TAPE_ZERO_POS m_zeroPos;

    //объект определения рабочего положения
    FrameTapeProcessing *m_procWorkPos[THREADS];
    //объект рабочего положения сканера
    SCANNER_POS m_scannerPos;

    //матрицы калибровки камеры
    cv::Mat m_intrinsic_matrix, m_distortion_coeffs;
    //словарь маркеров на основе cv::aruco::DICT_6X6_1000
    cv::Ptr<cv::aruco::Dictionary> m_dictionary;
    //параметры определения маркеров
    cv::Ptr<cv::aruco::DetectorParameters> m_detectorParams;

signals:
    //обновляем виждет при изменении углов осей робота
    void drawAxesAngle();
    //сигналы окончания обработки кадра
    void imageProcessed(cv::Mat mat);
    //можно производить автопилотирование
    void autopilotAction();

public slots:
    //установка режима нулевой позиции
    void zeroPositionMode(bool zpm);
    //следующий кадр установит нулевую позицию
    void setZeroPosition();
    //принять кадр от камеры для обработки
    void processFrame(cv::Mat mat);
    //обновление угов осей сканера
    void updateAxesAngle(double front, double back);

protected slots:
    //рабочая позиция ленты определена
    void tapeProcessed(FrameTapeProcessing *obj);
    //нулевая позиция ленты определена
    void zeroPosProcessed(FrameTapeSetZeroPos *obj);

protected:
    //потоки обработки
    QThread *m_threads[THREADS];
};

#endif // FRAMEPROCESSINGPOOL_H
