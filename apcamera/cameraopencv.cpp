#include "cameraopencv.h"
#include "weild_processor/weildsynthetic.h"
#include <QSettings>
#include <QMetaMethod>
#include <algorithm>
#include <cmath>
#include <exception>

CameraOpenCv::CameraOpenCv(QObject *parent) : QObject(nullptr)
{
    Q_UNUSED(parent) // No GUI parent: the object moves to its own thread.
    qRegisterMetaType<cv::Mat>("cv::Mat");
    QSettings settings;
    m_cameraId=settings.value("Camera/id",0).toInt();
    m_timerCap=new QTimer(this);
    m_timerCap->setTimerType(Qt::PreciseTimer);
    connect(m_timerCap,&QTimer::timeout,this,&CameraOpenCv::captureCameraFrame);
    m_thread=new QThread;
    moveToThread(m_thread);
    m_thread->start();
}

CameraOpenCv::~CameraOpenCv()
{
    QThread *ownerThread=QThread::currentThread();
    if (ownerThread==m_thread)
        qFatal("CameraOpenCv must be destroyed outside its capture thread");

    // Destruction cannot abandon work that still accesses this object. Unlike an
    // ordinary command, cleanup must complete before members and children die.
    const auto result=runInCameraThread([this,ownerThread]{
        stopInThread();
        moveToThread(ownerThread);
        return thread()==ownerThread;
    },QDeadlineTimer(QDeadlineTimer::Forever));
    if (result!=OperationResult::Success)
        qFatal("CameraOpenCv could not finish cleanup in its capture thread");
    m_thread->quit();
    m_thread->wait();
    delete m_thread;
}

//Управаление камерой в отдельном потоке
//с блокирующим ождианием заршения операции operation
CameraOpenCv::OperationResult CameraOpenCv::runInCameraThread(
    std::function<bool()> operation, QDeadlineTimer deadline)
{
    if (QThread::currentThread()==m_thread)
        return OperationResult::WrongThread;
    if (!m_thread->isRunning())
        return OperationResult::ThreadNotRunning;

    auto state=QSharedPointer<Completion>::create();
    QMutexLocker lock(&state->mutex);
    QTimer::singleShot(0,this,[this,state,operation=std::move(operation)]{
        bool success=false;
        QString error;
        try {
            //выполняемая операция
            success=operation();
        } catch (const std::exception &e) {
            error=QString::fromUtf8(e.what());
        } catch (...) {
            error=QStringLiteral("Unknown camera operation error");
        }
        if (!error.isEmpty()) {
            qDebug() << "operation error";
            try {
                stopInThread();
            } catch (...) {}
        }
        {
            QMutexLocker lock(&state->mutex);
            state->success=success;
            state->done=true;
            state->condition.wakeAll();
        }
        if (!error.isEmpty()) emit captureError(error);
    });

    while (!state->done) {
        if (!state->condition.wait(&state->mutex,deadline) && !state->done) {
            lock.unlock();
            emit captureError(QStringLiteral("Camera command timed out; it may still complete"));
            return OperationResult::Timeout;
        }
    }
    return state->success ? OperationResult::Success : OperationResult::Failed;
}

void CameraOpenCv::saveCameraParams()
{
    QSettings settings;
    settings.setValue("Camera/id",m_cameraId);
}
void CameraOpenCv::startTimer(int fps)
{
    m_captureInterval=std::max(1,int(std::round(1000./std::clamp(fps,1,1000))));
    m_frameCounter=0;
    m_timerCap->start(m_captureInterval);
}

CameraOpenCv::OperationResult CameraOpenCv::startCamera(int id, int fps, QSize size)
{
    if (QThread::currentThread()==m_thread)
        return OperationResult::WrongThread;
    m_cameraId=id;
    saveCameraParams();
    return runInCameraThread([this,id,fps,size]{
        stopInThread();
        if (!m_cap.open(id,cv::CAP_ANY)) {
            emit captureError("Cannot open camera");
            return false;
        }
        m_cap.set(cv::CAP_PROP_FRAME_WIDTH,size.width());
        m_cap.set(cv::CAP_PROP_FRAME_HEIGHT,size.height());
        m_cap.set(cv::CAP_PROP_FPS,fps);
        m_cap.set(cv::CAP_PROP_BUFFERSIZE,1);
        m_source=Source::Camera;
        const double sourceFps=m_cap.get(cv::CAP_PROP_FPS);
        startTimer(fps);
        return true;
    });
}

CameraOpenCv::OperationResult CameraOpenCv::startVideo(const QString &path, int fps)
{
    return runInCameraThread([this,path,fps]{
        stopInThread();
        if (!m_cap.open(path.toStdString())) {
            emit captureError("Cannot open video");
            return false;
        }
        const double sourceFps=m_cap.get(cv::CAP_PROP_FPS);
        m_source=Source::Video;
        startTimer(std::isfinite(sourceFps) && sourceFps>=1 && sourceFps<=1000
                       ? int(std::round(sourceFps)) : fps);
        return true;
    });
}

CameraOpenCv::OperationResult CameraOpenCv::startSynthetic(int fps)
{
    return runInCameraThread([this,fps]{
        stopInThread();
        m_source=Source::Synthetic;
        startTimer(fps);
        return true;
    });
}
void CameraOpenCv::stopInThread()
{
    m_timerCap->stop();
    m_cap.release();
    m_source=Source::Stopped;
}
CameraOpenCv::OperationResult CameraOpenCv::stopCamera()
{
    return runInCameraThread([this]{stopInThread(); return true;});
}
void CameraOpenCv::captureCameraFrame()
{
    try {
        cv::Mat image;
        if (m_source==Source::Synthetic)
            image=WeildSynthetic::frame(4*std::sin(m_frameCounter*.02),20,6,
                                       m_frameCounter%240 < 232,unsigned(m_frameCounter+1));
        else if (m_source==Source::Stopped) return;
        else if (!m_cap.read(image) || image.empty()) {
            bool video=m_source==Source::Video;
            stopInThread();
            if (video) emit sourceFinished(); else emit captureError("Camera read failed");
            return;
        }
        //const auto timestamp=WeildDetect::monotonicNs();
        emit frameCaptured(image,/*timestamp*/0);
        if (isSignalConnected(QMetaMethod::fromSignal(&CameraOpenCv::frameCap))) emit frameCap(image.clone());
        ++m_frameCounter;
    } catch (const cv::Exception &e) { stopInThread(); emit captureError(QString::fromUtf8(e.what())); }
}
