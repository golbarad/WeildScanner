#include "cameraopencv.h"

//канал логирования
#define LCH     0


CameraOpenCv::CameraOpenCv(QObject *parent) : QObject(parent)
{
    //объект openCv cv::Mat имеет все свойства
    //чтобы быть зарегистрированым для Qt metacall
    qRegisterMetaType<cv::Mat>("cv::Mat");

    //если файла нет, то сделаем этот файл по умолчанию
    QFileInfo fi(PATH_NAME_CAMERA_PARAMS_FILE);
    if(!fi.exists()) { saveCameraParams(); }

    //загружаем параметры объекта
    cv::FileStorage fs(fi.absoluteFilePath().toUtf8().data(), cv::FileStorage::READ);
    if(fs.isOpened()) {
        fs["camera_id"] >> m_cameraId;
    } else {
        std::cout << "can't load file camera parameters" << std::endl;
    }
    fs.release();

    m_captureInterval = 20;
    m_frameCounter = 0;

    //создаем таймер зайвата кадров
    //(чтобы он вошел в поток указываем родителя
    //https://habr.com/ru/articles/467261/)
    m_timerCap = new QTimer(this);

    //создаем и стартуем отдельный поток
    m_thread = new QThread();
    m_thread->start();
    //переносим этот объект в этот поток
    moveToThread(m_thread);
}

CameraOpenCv::~CameraOpenCv()
{
   BLOG(LCH) << "start the actions from ~CameraOpenCv() \n"
              << "\tinvoke slotStartCamera() and wait it ...";
    LOGEND(LCH);
    stopCamera();

    BLOG(LCH) << "CameraOpenCv thread stop and wait end ... \n";
    m_thread->exit();
    m_thread->wait();
    BLOG(LCH) << " ok\n"; LOGEND(LCH);

    delete m_thread;
    delete m_timerCap;
}

void CameraOpenCv::saveCameraParams()
{
    //загружаем параметры объекта
    cv::FileStorage fs(PATH_NAME_CAMERA_PARAMS_FILE, cv::FileStorage::WRITE);
    if(fs.isOpened()) {
        fs << "camera_id" << m_cameraId;
    } else {
        std::cout << "can't save file camera parameters" << std::endl;
    }
    fs.release();
}

void CameraOpenCv::startCamera(int id, int fps)
{
    m_cameraId = id;
    if(fps <= 0) { fps = 1; }
    m_captureInterval = 1000 / fps;
    QTimer::singleShot(0, this, SLOT(slotStartCamera()));
    saveCameraParams();

    m_mutexWaitSlot.lock();
    m_waitSlot.wait(&m_mutexWaitSlot, 5000);
    m_mutexWaitSlot.unlock();
}

void CameraOpenCv::stopCamera()
{
    QTimer::singleShot(0, this, SLOT(slotStopCamera()));

    m_mutexWaitSlot.lock();
    m_waitSlot.wait(&m_mutexWaitSlot, 5000);
    m_mutexWaitSlot.unlock();
}

void CameraOpenCv::slotStartCamera()
{
    //BLOG(LCH) << "CameraOpenCv::slotStartCamera() ... \n";

    if(m_cap.isOpened()) {
        BLOG(LCH) << "\trelease before open camera \n";
        m_cap.release();
    }
    if(m_cap.open(m_cameraId, cv::CAP_ANY))
    {
       BLOG(LCH) << "\tcamera " << m_cameraId << " is open \n";
        //установить и проверить разрешение камеры
        //m_cap.set(cv::CAP_PROP_FRAME_WIDTH, 1024);
        //m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, 768);
        //
        //m_cap.set(cv::CAP_PROP_FRAME_WIDTH, 640);
        //m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, 480);
        m_cap.set(cv::CAP_PROP_FRAME_WIDTH, 320);
        m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, 240);

        //установить и проверить формат кадра
        //int fmtSet = CV_8UC3;//CV_8SC3 установка формата дает ошибку
        //VIDEOIO ERROR: V4L2: setting property #8 is not supported
        //но работает, но с современными камерами все хорошо и так
        //m_cap.set(cv::CAP_PROP_FORMAT, fmtSet);
        //int fmtGet = m_cap.get(cv::CAP_PROP_FORMAT);
        //if(fmtSet == fmtGet) {
        //    fprintf(stdout, "set format %i == %i - ok\n", fmtSet, fmtGet); fflush(stdout);
        //} else {
        //    fprintf(stdout, "set format %i != %i - bad\n", fmtSet, fmtGet); fflush(stdout);
        //}
        //закоментено, раскоментировать для проверки других камер

        //запускаем таймер захвата кадров
        BLOG(LCH) << "\tstart the camera timer for " << m_captureInterval << " ms \n";
        connect(m_timerCap, SIGNAL(timeout()), this, SLOT(captureCameraFrame()));
        m_timerCap->start(m_captureInterval);
    }
    else {
        BLOG(LCH) << "\tcan't open camera " << m_cameraId << "\n";
    }
    m_waitSlot.wakeAll();
    LOGEND(LCH);
}

void CameraOpenCv::slotStopCamera()
{
    BLOG(LCH) << "slotStopCamera():\n";
    if(m_timerCap->isActive()) {
        BLOG(LCH) << "\tstop timer\n";
        m_timerCap->stop();
        disconnect(m_timerCap, SIGNAL(timeout()), this, SLOT(captureCameraFrame()));
    }
    if(m_cap.isOpened()) {
        BLOG(LCH) << "\trelease camera\n";
        m_cap.release();
    }
    LOGEND(LCH);
    m_waitSlot.wakeAll();
}

void CameraOpenCv::captureCameraFrame()
{
    if(!m_cap.grab()) {
        fprintf(stdout, "m_cap.grab() return false\n"); fflush(stdout);
        return;
    }
    if(!m_cap.retrieve(m_matFrame)) {
        fprintf(stdout, "m_cap.retrieve() return false\n"); fflush(stdout);
        return;
    }
    //fprintf(stdout, "%i\n", m_frameCounter); fflush(stdout);

    emit frameCap(m_matFrame);
    m_frameCounter++;
}
