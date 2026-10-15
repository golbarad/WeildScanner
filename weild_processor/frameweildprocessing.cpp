#include "frameweildprocessing.h"
#include <QThread>

void FrameWeildProcessing::processFrame(cv::Mat image, quint64 id, qint64 timestamp,
    quint64 generation, WeildDetect::Parameters parameters, WeildDetect::Calibration calibration)
{
    Q_ASSERT(QThread::currentThread() == thread());
    WeildDetect::Result result;
    result.frameId = id;
    //result.timestampNs = timestamp;
    try {
        QString error;
        if (m_detect.configure(parameters,calibration,&error))
            result=m_detect.process(image,id,timestamp);
        else result.diagnostic=error;
    }
    catch (const std::exception &e) {
        result.diagnostic=QString::fromUtf8(e.what());
    }
    result.generation=generation;
    emit processingFinished(result);
}
