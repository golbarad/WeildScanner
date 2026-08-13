#include "faceprocessingpool.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <iostream>
#include "logger.h"

static QString faceModelPath(const QString &fileName)
{
    QStringList dirs;
    dirs << QCoreApplication::applicationDirPath();
#ifdef PATH_NAME_FACE_MODELS_DIR
    dirs << QString(PATH_NAME_FACE_MODELS_DIR);
#endif

    for(const QString &dir : dirs) {
        QFileInfo fi(QDir(dir).filePath(fileName));
        if(fi.exists() && fi.isFile()) {
            return fi.absoluteFilePath();
        }
    }

    return QString();
}

FaceProcessingPool::FaceProcessingPool(QObject *parent) : QObject(parent)
{
    //создаем и запускаем потоки обработки
    for (int i = 0; i < THREADS; i++) {
        m_threads[i] = new QThread(this);
        m_threads[i]->start();
    }

    //параметры распознавания
    try {
        const QString detectorPath = faceModelPath("face_detection_yunet_2023mar.onnx");
        const QString recognizerPath = faceModelPath("face_recognition_sface_2021dec.onnx");

        if(detectorPath.isEmpty() || recognizerPath.isEmpty()) {
            std::cout << "face detection disabled: ONNX model files not found" << std::endl;
            std::cout << "expected: face_detection_yunet_2023mar.onnx and face_recognition_sface_2021dec.onnx" << std::endl;
#ifdef PATH_NAME_FACE_MODELS_DIR
            std::cout << "put models to: " << QString(PATH_NAME_FACE_MODELS_DIR).toStdString() << std::endl;
#endif
        } else {
            m_detector = cv::FaceDetectorYN::create(detectorPath.toStdString(), "", cv::Size(320, 320), 0.9f, 0.3f,5000);
            m_recognizer = cv::FaceRecognizerSF::create(recognizerPath.toStdString(),"");
        }
    } catch(const cv::Exception &e) {
        m_detector.release();
        m_recognizer.release();
        BLOG(0) << "face detection disabled: " << e.what();
    }

    //создаем и настраиваем дектор лиц
    for(int i = 0; i < THREADS; i++) {
        m_procFace[i] = new FaceProcessing;
        m_procFace[i]->m_detectFace.detector = m_detector;
        m_procFace[i]->m_detectFace.recognizer = m_recognizer;
        m_procFace[i]->m_detectFace.checkFace = false;
        m_procFace[i]->moveToThread(m_threads[i]);
    }

    //соединяем сигналы окончания обработки
    for(int i = 0; i < THREADS; i++) {
        connect(m_procFace[i], SIGNAL(processingFinished(FaceProcessing*)),
                this, SLOT(faceProcessed(FaceProcessing*)));
    }

}
//---------------------------------------------------------------------------
FaceProcessingPool::~FaceProcessingPool()
{
    //останавливаем все потоки обработки
    for(int i = 0; i < THREADS; i++) {
        QThread::msleep(10);
        m_threads[i]->exit();
        m_threads[i]->wait();
    }
}
//---------------------------------------------------------------------------
// void FaceProcessingPool::setObjParams(OBJECT_PARAMS *params)
// {
//     for(int i = 0; i < THREADS; i++) {
//         m_procFace[i]->m_detetcFace.objParams = params;
//     }
// }
//---------------------------------------------------------------------------
void FaceProcessingPool::processFrame(cv::Mat mat)
{
    if(m_detector.empty() || m_recognizer.empty()) { return; }

    //найдем свободный обработчик и передадим ему картинку
    for(int i = 0; i < THREADS; i++) {
        if(!m_procFace[i]->m_busy) {
            //std::cout << "selected: " << i << " worker" << std::endl;
            m_procFace[i]->processFrame(mat);
            return;
        }
    }
    //если мы здесь, то свободного обработчика не найдено
    std::cout << "it is no a free worker --- camera frame skip" << std::endl;
}
//---------------------------------------------------------------------------
void FaceProcessingPool::faceProcessed(FaceProcessing *obj)
{
    //уведомляем окончание обработки картинки
    emit imageProcessed(obj->m_detectFace.matBgr);
    // очищаем результаты предыдущего кадра
    m_faceInfo.clear();
    if (obj->m_detectFace.faceDetected)
    {        
        const auto &faces = obj->m_detectFace.currentFaces;
        const auto &matches = obj->m_detectFace.currentfaceMatch;
        //пройдемся по всем найденым лицам
        for (size_t i = 0; i < faces.size(); ++i)
        {
            const cv::Mat &face = faces[i];
            if (face.empty() || face.cols < 4)
                continue;
            //получаем геометрию лица
            FACE_INFO info;
            info.rect = QRectF(
                face.at<float>(0, 0),
                face.at<float>(0, 1),
                face.at<float>(0, 2),
                face.at<float>(0, 3));

            //проверяем соответсия лица искомомоу
            if (i < matches.size())
                info.match = matches[i];
            else
                info.match = false;
            //
            m_faceInfo.append(info);
        }
    }

    // с этого момента поток не занят
    obj->m_busy = false;
}

