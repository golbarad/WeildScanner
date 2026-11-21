#include "weildprocessingpool.h"
#include "weildrobotoutput.h"
#include <QMutexLocker>
#include <QJsonDocument>
#include <QJsonArray>

WeildProcessingPool::WeildProcessingPool(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<WeildDetect::Result>();
    qRegisterMetaType<FrameSynchronizer::DualFrame>();

    //создаем и настраиваем определителели сварного шва
    for (int i=0; i<WorkerCount; ++i) {
        auto &w=m_workers[i];
        w.thread=new QThread(this);
        w.processor=new FrameWeildProcessing;
        w.processor->moveToThread(w.thread);
        connect(w.thread,&QThread::finished,w.processor,&QObject::deleteLater);
        connect(w.processor,&FrameWeildProcessing::processingFinished,this,
                [this,i](WeildDetect::Result result){weildProceed(i,result);});
        w.thread->start();
    }
    m_watchdog.setInterval(33);
    connect(&m_watchdog,&QTimer::timeout,this,&WeildProcessingPool::checkTimeout);
    m_watchdog.start();
}

WeildProcessingPool::~WeildProcessingPool()
{
    m_watchdog.stop();
    { QMutexLocker lock(&m_mutex); m_stopping=true; m_pending={}; }
    for (auto &worker : m_workers) worker.thread->quit();
    for (auto &worker : m_workers) worker.thread->wait();
    stopRecording();
}

bool WeildProcessingPool::configure(const WeildDetect::Parameters &p,
    const WeildDetect::Calibration &c, QString *error)
{
    qDebug() << " gtkhjjjkrth56jkotyjk "  << m_parameters.maxSaturatedWidth;
    Q_ASSERT(QThread::currentThread()==thread());
    if (!p.valid(error) || (!c.cameraMatrix.empty() && !c.valid(error))) return false;
    m_parameters=p; m_calibration=c.clone();
    std::cout << " s "  << m_parameters.maxSaturatedWidth;
    reset();
    return true;
}
void WeildProcessingPool::reset()
{
    Q_ASSERT(QThread::currentThread()==thread());
    {
        QMutexLocker lock(&m_mutex);
        ++m_generation; m_pending={};
    }
    m_tracker.reset(); m_path.clear(); m_lastAccepted=0; m_lastTimestamp=0;
    m_lastOverlay={}; m_lostNotified=false;
    WeildDetect::Result cleared;
    //cleared.timestampNs=WeildDetect::monotonicNs();
    cleared.generation=m_generation;
    cleared.diagnostic="Reset: waiting for a new measurement";
    deliver(cleared);
}
quint64 WeildProcessingPool::droppedFrames() const
{
    QMutexLocker lock(&m_mutex); return m_dropped;
}

void WeildProcessingPool::sendResult()
{
    Pending pending;
    int index=-1;
    //находим свободный worker
    for (int i=0; i < WorkerCount; ++i) {
        if (!m_workers[i].busy) {index=i; break;}
    }
    {
        //отправляем захваченное изображение
        QMutexLocker lock(&m_mutex);
        m_sendingScheduled=false;
        if (m_stopping || m_pending.image.empty() || index < 0) return;
        pending=std::move(m_pending); m_pending={};
    }
    // if ((WeildDetect::monotonicNs()-pending.timestamp)*1e-9 > m_parameters.maxResultAge) {
    //     QMutexLocker lock(&m_mutex); ++m_dropped; return;
    // }
    auto &w=m_workers[index]; w.busy=true;
    const auto p=m_parameters;
    const auto c=m_calibration.clone();
    auto *processor=w.processor;
    // Запускаем обработку
    QMetaObject::invokeMethod(processor,
        [processor,pending,p,c]{
            processor->processFrame(pending.image,pending.id,pending.timestamp,pending.generation,p,c);
        },Qt::QueuedConnection);
}

void WeildProcessingPool::submitFrame(const cv::Mat &image, qint64 timestamp)
{    
    QMutexLocker lock(&m_mutex);
    if (m_stopping) return;
    // Если не обработали последний захваченный кадр - увеличим число дропов
    if (!m_pending.image.empty())
        ++m_dropped;
    // Копируем текущее изображение
    m_pending={image.clone(),++m_nextFrame,m_generation,timestamp};
    if (!m_sendingScheduled) {
        m_sendingScheduled=true;
        QMetaObject::invokeMethod(this,[this]{sendResult();},Qt::QueuedConnection);
    }
}

void WeildProcessingPool::weildProceed(int index, WeildDetect::Result result)
{
    m_workers[index].busy=false;
    if (result.generation==m_generation && result.frameId>m_lastAccepted)
        // result.timestampNs>m_lastTimestamp &&
        // (WeildDetect::monotonicNs()-result.timestampNs)*1e-9 <= m_parameters.maxResultAge)
    {
        m_lastAccepted=result.frameId;
        //m_lastTimestamp=result.timestampNs;
        m_lastOverlay=result.overlay;
        deliver(m_tracker.update(result,m_parameters));
    }
    else {
        //увеличим счетчик потеряных кадров
        QMutexLocker lock(&m_mutex); ++m_dropped;
    }
    sendResult();
}

void WeildProcessingPool::checkTimeout()
{
    const auto now=0; //WeildDetect::monotonicNs();
    // Ограничиваем ожидание результата
    if (!m_lastTimestamp || (now-m_lastTimestamp)*1e-9 <= m_parameters.maxResultAge || m_lostNotified) return;
    auto result=m_tracker.predict(now,m_parameters);
    // Future completions older than this published prediction are stale.
    m_lastTimestamp=now;
    result.frameId=m_lastAccepted; result.generation=m_generation;
    result.overlay=m_lastOverlay;
    if (!result.predicted) m_lostNotified=true;
    deliver(result);
}

void WeildProcessingPool::deliver(WeildDetect::Result r)
{
    if (r.detected || r.predicted) {
        m_lostNotified=false;
        if (m_calibration.robotTransformValid) {
            const auto p=r.trackedCamera;
            const cv::Vec4d xyz=m_calibration.cameraToRobot*cv::Vec4d(p.x,p.y,p.z,1);
            r.centerRobot={xyz[0],xyz[1],xyz[2]};
        }
    }
    if (r.detected) {
        m_path.push_back({r.frameId,/*r.timestampNs*/ 0,r.centerCamera,r.trackedCamera,
            r.centerRobot,r.width,r.depth,r.angle,r.quality}
        );
        while (m_path.size()>size_t(m_parameters.maxPathPoints)) m_path.pop_front();
    }
    if (m_recording.isOpen()) {
        auto json=WeildRobotOutput::packet(r);
        json["diagnostic"]=r.diagnostic;
        json["processing_ms"]=r.processingMs;
        json["dropped_frames"]=QString::number(droppedFrames());
        if (r.detected || r.predicted) {
            json["tracked_camera_mm"]=QJsonArray{r.trackedCamera.x,r.trackedCamera.y,r.trackedCamera.z};
            json["width_mm"]=r.width; json["depth_mm"]=r.depth; json["angle_deg"]=r.angle;
        }
        if (r.detected) json["measured_root_camera_mm"]=QJsonArray{r.centerCamera.x,r.centerCamera.y,r.centerCamera.z};
        const QByteArray line=QJsonDocument(json).toJson(QJsonDocument::Compact)+'\n';
        if (m_recording.write(line)!=line.size()) {
            emit diagnostic("Recording failed: "+m_recording.errorString());
            stopRecording();
        }
    }
    if (!r.overlay.empty()) {
        emit imageProcessed(r.overlay);
    }
    //r.profile[0] = cv::Point3d(1,1,1);
    emit measurementReady(r);
}
bool WeildProcessingPool::startRecording(const QString &name)
{
    stopRecording();
    m_recording.setFileName(name);
    if (!m_recording.open(QIODevice::WriteOnly|QIODevice::Append|QIODevice::Text)) {
        emit diagnostic(m_recording.errorString()); return false;
    }
    return true;
}
void WeildProcessingPool::stopRecording()
{
    if (m_recording.isOpen()) { m_recording.flush(); m_recording.close(); }
}
