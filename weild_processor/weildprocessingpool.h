#pragma once
#include <QObject>
#include <QThread>
#include <QTimer>
#include <QMutex>
#include <QFile>
#include <array>
#include <deque>
#include "frameweildprocessing.h"
#include "weildtracker.h"

/*
    Класс инкапсулирующий пул потгоков обработчиков изображений шва
*/
class WeildProcessingPool : public QObject
{
    Q_OBJECT
public:
    static constexpr int WorkerCount = 10;
    struct PathPoint {
        quint64 frameId;
        qint64 timestampNs;
        cv::Point3d measuredCamera, trackedCamera, robot;
        double width, depth, angle, quality;
    };

    explicit WeildProcessingPool(QObject *parent = nullptr);
    ~WeildProcessingPool() override;
    bool configure(const WeildDetect::Parameters &parameters,
                   const WeildDetect::Calibration &calibration, QString *error = nullptr);

    const WeildDetect::Parameters &parameters() const { return m_parameters; }
    const WeildDetect::Calibration &calibration() const { return m_calibration; }
    const std::deque<PathPoint> &path() const { return m_path; }

    quint64 droppedFrames() const;
    bool startRecording(const QString &fileName);
    void stopRecording();
    void reset();

    // Запуск обработки фрейма, возвращает копию захваченного кадра;
    // (работает в потоке камеры (соединеяется через DirectConnection))
    void submitFrame(const cv::Mat &image, qint64 timestampNs);

signals:
    void imageProcessed(FrameSynchronizer::DualFrame frames);
    void measurementReady(WeildDetect::Result result);
    void diagnostic(QString message);

private:

    struct Worker {
        QThread *thread=nullptr;
        FrameWeildProcessing *processor=nullptr;
        bool busy=false;
    };

    //структура последнего захваченного кадра
    struct Pending {
        cv::Mat image;
        quint64 id=0, generation=0;
        qint64 timestamp=0;
    };
    std::array<Worker,WorkerCount> m_workers;

    mutable QMutex m_mutex;
    Pending m_pending;
    //флаг запланированной отправки резльтата
    bool m_sendingScheduled=false;
    bool m_stopping = false;

    quint64 m_nextFrame=0, m_generation=0, m_dropped=0;
    quint64 m_lastAccepted=0;
    qint64 m_lastTimestamp=0;
    WeildDetect::Parameters m_parameters;
    WeildDetect::Calibration m_calibration;
    WeildTracker m_tracker;
    std::deque<PathPoint> m_path;

    QFile m_recording;
    QTimer m_watchdog;
    FrameSynchronizer::DualFrame m_lastOverlay;
    bool m_lostNotified=false;

    //слот завшения обработки кадра
    void weildProceed(int index, WeildDetect::Result result);
    // Добавляем захваченный результат в пул обрабочиков
    void sendResult();
    void deliver(WeildDetect::Result result);
    void checkTimeout();
};
