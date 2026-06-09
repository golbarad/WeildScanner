#include "frameprocessingpool.h"

FrameProcessingPool::FrameProcessingPool(QObject *parent) : QObject(parent)
{
    //m_objParams = nullptr;
    m_zeroPositionMode = false;
    m_setZeroPosition = false;

    {
        //загружаем нулевую позицию
        cv::FileStorage fs("j:/WorkProjects/AutoWeldScanner/autopilot/zero_pos.xml", cv::FileStorage::READ);
        if(fs.isOpened()) {
            fs["mat4x4ZeroPos"] >> m_zeroPos.mat4x4ZeroPosInv;
            fs["firstDistMm"] >> m_zeroPos.firstDistMm;
        } else {
            std::cout << "can't load file zero position" << std::endl;
        }
        fs.release();
    }
    {
        //загружаем калибровку камеры
        cv::FileStorage fs("j:/WorkProjects/AutoWeldScanner/autopilot/DEDENDER_2577_320x240.xml", cv::FileStorage::READ);
        if(fs.isOpened()) {
            fs["camera_matrix"] >> m_intrinsic_matrix;
            fs["distortion_coefficients"] >> m_distortion_coeffs;
        }
        else  qDebug() << "can't load file camera calibration \n";

        fs.release();
    }

    m_dictionary = cv::makePtr<cv::aruco::Dictionary>(cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_1000));
    m_detectorParams = cv::makePtr<cv::aruco::DetectorParameters>();
    //попробуем подкрутить параметры поиска маркеров
    /*m_detectorParams->adaptiveThreshWinSizeMin;//3 меньше нельзя
    m_detectorParams->adaptiveThreshWinSizeMax;// = 50;//23
    m_detectorParams->adaptiveThreshWinSizeStep;// = 20;//10
    m_detectorParams->adaptiveThreshConstant;//7
    m_detectorParams->minMarkerPerimeterRate;
    m_detectorParams->maxMarkerPerimeterRate;
    m_detectorParams->polygonalApproxAccuracyRate;
    m_detectorParams->minCornerDistanceRate;
    m_detectorParams->minDistanceToBorder;
    m_detectorParams->minMarkerDistanceRate;
    m_detectorParams->cornerRefinementMethod;
    m_detectorParams->cornerRefinementWinSize;
    m_detectorParams->cornerRefinementMaxIterations;
    m_detectorParams->cornerRefinementMinAccuracy;
    m_detectorParams->markerBorderBits;
    m_detectorParams->perspectiveRemovePixelPerCell;
    m_detectorParams->perspectiveRemoveIgnoredMarginPerCell;
    m_detectorParams->maxErroneousBitsInBorderRate;
    m_detectorParams->minOtsuStdDev;
    m_detectorParams->errorCorrectionRate;*/

    //создаем и запускаем потоки обработки
    for (int i = 0; i < THREADS; i++) {
        m_threads[i] = new QThread(this);
        m_threads[i]->start();
    }

    //создаем и настраиваем калибратор начальной позиции
    //(используем один отдельный поток)
    m_procZeroPos = new FrameTapeSetZeroPos();
    m_procZeroPos->m_tapePos.intrinsic_matrix = &m_intrinsic_matrix;
    m_procZeroPos->m_tapePos.distortion_coeffs = &m_distortion_coeffs;
    m_procZeroPos->m_tapePos.dictionary = &m_dictionary;
    m_procZeroPos->m_tapePos.detectorParams = &m_detectorParams;
    m_procZeroPos->moveToThread(m_threads[0]);
    connect(m_procZeroPos, SIGNAL(processingFinished(FrameTapeSetZeroPos*)),
            this, SLOT(zeroPosProcessed(FrameTapeSetZeroPos*)));

    //создаем и настраиваем определителей рабочей позиции
    for(int i = 0; i < THREADS; i++) {
        m_procWorkPos[i] = new FrameTapeProcessing;
        m_procWorkPos[i]->m_tapePos.intrinsic_matrix = &m_intrinsic_matrix;
        m_procWorkPos[i]->m_tapePos.distortion_coeffs = &m_distortion_coeffs;
        m_procWorkPos[i]->m_tapePos.dictionary = &m_dictionary;
        m_procWorkPos[i]->m_tapePos.detectorParams = &m_detectorParams;
        m_procWorkPos[i]->moveToThread(m_threads[i]);
    }


    //соединяем сигналы окончания обработки
    for(int i = 0; i < THREADS; i++) {
        connect(m_procWorkPos[i], SIGNAL(processingFinished(FrameTapeProcessing*)),
                this, SLOT(tapeProcessed(FrameTapeProcessing*)));
    }

    /*m_procWorkPos = new FrameTapeProcessing;
    m_procWorkPos->m_tapePos.intrinsic_matrix = &m_intrinsic_matrix;
    m_procWorkPos->m_tapePos.distortion_coeffs = &m_distortion_coeffs;
    m_procWorkPos->m_tapePos.dictionary = &m_dictionary;
    m_procWorkPos->m_tapePos.detectorParams = &m_detectorParams;
    m_procWorkPos->m_zeroPos = &m_zeroPos;
    m_procWorkPos->m_scannerPos = &m_scannerPos;*/
}
//---------------------------------------------------------------------------
FrameProcessingPool::~FrameProcessingPool()
{
    //останавливаем все потоки обработки
    for(int i = 0; i < THREADS; i++) {
        QThread::msleep(10);
        m_threads[i]->exit();
        m_threads[i]->wait();
    }
}
//---------------------------------------------------------------------------
void FrameProcessingPool::setObjParams(OBJECT_PARAMS *params)
{
    for(int i = 0; i < THREADS; i++) {
        m_procWorkPos[i]->m_tapePos.objParams = params;
    }
}
//---------------------------------------------------------------------------
void FrameProcessingPool::zeroPositionMode(bool zpm)
{
    m_zeroPositionMode = zpm;
    if(!m_zeroPositionMode) {
        m_setZeroPosition = false;
    }
}
//---------------------------------------------------------------------------
void FrameProcessingPool::setZeroPosition()
{
    if(m_zeroPositionMode) {
        m_setZeroPosition = true;
    }
}
//---------------------------------------------------------------------------
void FrameProcessingPool::updateAxesAngle(double front, double back)
{
    m_scannerPos.frontAxisRad = front * M_PI / 180;
    m_scannerPos.backAxisRad = back * M_PI / 180;
    emit drawAxesAngle();
}
//---------------------------------------------------------------------------
void FrameProcessingPool::processFrame(cv::Mat mat)
{
    if(m_zeroPositionMode) {
        if(!m_procZeroPos->m_busy) {
            m_procZeroPos->processFrame(mat);
        }
    } else {
        //найдем свободный обработчик и передадим ему картинку
        for(int i = 0; i < THREADS; i++) {
            if(!m_procWorkPos[i]->m_busy) {
                //std::cout << "selected: " << i << " worker" << std::endl;
                m_procWorkPos[i]->processFrame(mat);
                return;
            }
        }
        //если мы здесь, то свободного обработчика не найдено
        std::cout << "it is no a free worker --- camera frame skip" << std::endl;
    }
}
//---------------------------------------------------------------------------
void FrameProcessingPool::tapeProcessed(FrameTapeProcessing *obj)
{
    //уведомляем окончание обработки картинки
    emit imageProcessed(obj->m_tapePos.matBgr);

    m_scannerPos.posDefined = obj->m_tapePos.posDefined;

    //если позиция ленты корректно определена
    if(obj->m_tapePos.posDefined)
    {
        //создадим копии векторов положения
        m_scannerPos.rvec = obj->m_tapePos.rvec;
        m_scannerPos.tvec = obj->m_tapePos.tvec;
        //найдем рабочую матрицу преобразования координат
        m_scannerPos.mat4x4Pos = m_zeroPos.mat4x4ZeroPosInv * obj->m_tapePos.mat4x4Pos;
        //зафиксируем дистанцию первого сейчас видимого маркера
        m_scannerPos.firstDistMm = obj->m_tapePos.firstDistMm;
        m_scannerPos.lastDistMm = obj->m_tapePos.lastDistMm;

        //пересчитаем курс в углах (угло вектора, напрявленного парарлельно ленте )
        QVector3D v0 = m_scannerPos.transformPos(0, 0, 0);
        QVector3D v1 = m_scannerPos.transformPos(0, 1, 0);
        double dx = v1.x() - v0.x();
        double dy = v1.y() - v0.y();

        m_scannerPos.courseRad = 0;
        if(dy != 0) { m_scannerPos.courseRad = -atan(dx / dy); }
        //угол направляющего вектора
        m_scannerPos.courseDeg = m_scannerPos.courseRad / M_PI * 180;

        //расстоние между начаом ленты и камерой
        m_scannerPos.pathOffset = v0.x();
        //автопилот может произвести действия
        emit autopilotAction();
    }

    //с этого момента поток не занят
    obj->m_busy = false;
}
//---------------------------------------------------------------------------
void FrameProcessingPool::zeroPosProcessed(FrameTapeSetZeroPos *obj)
{
    //уведомляем окончание обработки картинки
    emit imageProcessed(obj->m_tapePos.matBgr);

    if(obj->m_tapePos.posDefined) {
        if(m_setZeroPosition) {
            m_zeroPos.mat4x4ZeroPosInv = obj->m_tapePos.mat4x4Pos.inv();
            m_zeroPos.firstDistMm = obj->m_tapePos.firstDistMm;
            //сохраняем нулевую позицию в файле
            cv::FileStorage fs("j:/WorkProjects/AutoWeldScanner/autopilot/zero_pos.xml", cv::FileStorage::WRITE);
            if(fs.isOpened()) {
                fs << "mat4x4ZeroPos" << m_zeroPos.mat4x4ZeroPosInv;
                fs << "firstDistMm" << m_zeroPos.firstDistMm;
            } else {
                qDebug() << "can't save file zero position\n";
            }
            fs.release();
            m_setZeroPosition = false;
        }
    }

    //с этого момента поток не занят
    obj->m_busy = false;
}
