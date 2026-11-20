#include "weildrobotoutput.h"
#include <QJsonArray>

QJsonObject WeildRobotOutput::packet(const WeildDetect::Result &r)
{
    QJsonObject out{{"schema",1},{"frame_id",QString::number(r.frameId)},
        {"generation",QString::number(r.generation)},
        {"timestamp_ns",QString::number(/*r.timestampMs*/0)}, {"clock","steady_local"},
        {"units","mm"},{"coordinate_frame","robot"},
        {"status",r.predicted ? "predicted" : (r.detected ? "measured" : "lost")},
        {"quality",r.quality},{"robot_valid",true}, {"transport","stub"}};
    if (r.detected || r.predicted) {
        out["center"]=QJsonArray{r.centerRobot.x,r.centerRobot.y,r.centerRobot.z};
        out["width_mm"]=r.width; out["depth_mm"]=r.depth; out["angle_deg"]=r.angle;
    }
    return out;
}
void WeildRobotOutput::publish(WeildDetect::Result result)
{
    m_lastPacket=packet(result);
    emit packetReady(m_lastPacket);
}
