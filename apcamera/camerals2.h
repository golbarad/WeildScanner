#pragma once

#include <QThread>
#include <atomic>
#include "icamera.h"


/*
    Класс инкапсулирующий захват изображений с профилоиетра
*/
class CameraLS2 : public ICamera
{
    Q_OBJECT
public:
    explicit CameraLS2(QObject *parent = nullptr);
    ~CameraLS2() override;

    OperationResult start(const Source &source) override;
    OperationResult stopCamera() override;

private:
    class Worker;
    QThread m_thread;
    Worker *const m_worker;
    quint64 m_generation = 0;
    std::atomic_bool m_framePending{false};
};
