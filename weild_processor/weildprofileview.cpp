#include "weildprofileview.h"
#include <QPainter>
#include <QPainterPath>
#include <algorithm>

void WeildProfileView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(),QColor(24,29,36));
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::white);
    painter.drawText(12,22,tr("Профиль: X / Z камеры, мм"));
    if (m_result.profile.empty()) {
        painter.drawText(rect().adjusted(12,35,-12,-12),Qt::AlignCenter|Qt::TextWordWrap,
                         tr("Нет пространственного профиля\nЗагрузите калибровку или включите синтетику"));
        return;
    }
    double xmin=m_result.profile.front().x,xmax=xmin,zmin=m_result.profile.front().z,zmax=zmin;
    for (auto point:m_result.profile) {
        xmin=std::min(xmin,point.x); xmax=std::max(xmax,point.x);
        zmin=std::min(zmin,point.z); zmax=std::max(zmax,point.z);
    }
    const auto area=QRectF(rect()).adjusted(45,42,-20,-35);
    const double scale=std::min(area.width()/std::max(1.,xmax-xmin),area.height()/std::max(1.,zmax-zmin));
    auto screen=[&](cv::Point3d p){return QPointF(area.center().x()+(p.x-(xmin+xmax)*.5)*scale,
                                             area.center().y()+(p.z-(zmin+zmax)*.5)*scale);};
    painter.setPen(QColor(80,90,100)); painter.drawRect(area);
    painter.setPen(QColor(80,220,170));
    for (auto p:m_result.profile) painter.drawEllipse(screen(p),1.2,1.2);
    if(m_result.detected) {
        painter.setPen(QPen(QColor(255,180,60),2));
        painter.drawLine(screen(m_result.leftShoulder),screen(m_result.centerCamera));
        painter.drawLine(screen(m_result.centerCamera),screen(m_result.rightShoulder));
        painter.drawEllipse(screen(m_result.centerCamera),4,4);
    }
    painter.setPen(Qt::white);
    painter.drawText(12,height()-10,tr("X: %1 … %2; Z: %3 … %4").arg(xmin,0,'f',1).arg(xmax,0,'f',1)
                     .arg(zmin,0,'f',1).arg(zmax,0,'f',1));
}
