#pragma once
#include "weilddetect.h"

// Accessed only by the pool's owner thread. All times are capture times, in seconds internally.
class WeildTracker
{
public:
    void reset();
    WeildDetect::Result update(WeildDetect::Result result, const WeildDetect::Parameters &parameters);
    WeildDetect::Result predict(qint64 timestamp, const WeildDetect::Parameters &parameters) const;
private:
    bool m_initialized = false;
    qint64 m_stateTime = 0, m_measurementTime = 0;
    cv::Point3d m_position, m_velocity;
    WeildDetect::Result m_lastMeasurement;
};
