#include "weildtracker.h"
#include <algorithm>

void WeildTracker::reset()
{
    m_initialized=false; m_stateTime=m_measurementTime=0;
    m_position={}; m_velocity={}; m_lastMeasurement={};
}

WeildDetect::Result WeildTracker::predict(qint64 timestamp, const WeildDetect::Parameters &p) const
{
    WeildDetect::Result out;
    //out.timestampNs=timestamp;
    out.diagnostic="Lost";
    if (!m_initialized) return out;
    if (timestamp < m_stateTime) { out.diagnostic="Out-of-order timestamp"; return out; }
    const double gap=(timestamp-m_measurementTime)*1e-9;
    if (gap > p.predictionSeconds || p.predictionSeconds <= 0) return out;
    out=m_lastMeasurement;
    out.detected=false; out.predicted=true;
    //out.timestampNs=timestamp;
    out.trackedCamera=m_position+m_velocity*((timestamp-m_stateTime)*1e-9);
    out.quality=m_lastMeasurement.quality*std::max(0.,1.-gap/p.predictionSeconds);
    out.diagnostic="Predicted";
    out.laserPixels.clear(); out.landmarkPixels.clear(); out.profile.clear();
    return out;
}

WeildDetect::Result WeildTracker::update(WeildDetect::Result r, const WeildDetect::Parameters &p)
{
    // if (m_initialized && r.timestampNs <= m_stateTime) {
    //     r.detected=false; r.predicted=false;
    //     r.diagnostic="Out-of-order timestamp";
    //     return r;
    // }
    // if (m_initialized && (r.timestampNs-m_measurementTime)*1e-9 > p.predictionSeconds) reset();
    bool rejected=false;
    if (r.detected && m_initialized) {
        const double dt=/*(r.timestampNs-m_stateTime) * */1e-9;
        const auto expected=m_position+m_velocity*dt;
        const auto residual=r.centerCamera-expected;
        if (cv::norm(residual) > p.maxTrackingJump) { r.detected=false; rejected=true; }
        else {
            m_position=expected+residual*.65;
            m_velocity+=residual*(.15/std::max(dt,1e-3));
        }
    }
    if (r.detected) {
        if (!m_initialized) { m_position=r.centerCamera; m_velocity={}; }
        m_initialized=true;
        //m_stateTime=m_measurementTime=r.timestampNs;
        r.trackedCamera=m_position;
        m_lastMeasurement=r;
        return r;
    }
    auto out=predict(0,/*r.timestampNs*/p);
    out.frameId=r.frameId; out.generation=r.generation;
    out.overlay=r.overlay; out.laserPixels=r.laserPixels; out.profile=r.profile;
    out.landmarkPixels=r.landmarkPixels;
    out.processingMs=r.processingMs;
    if (rejected) out.diagnostic += ": tracking jump rejected";
    else if (!r.diagnostic.isEmpty()) out.diagnostic += ": " + r.diagnostic;
    return out;
}
