#include "weilddetect.h"
#include "weildfileio.h"

namespace {
// One field list for both directions; validation remains in Parameters::valid().
template<class P, class F> void fields(P &p, F visit)
{
    visit("roi_x",p.roi.x); visit("roi_y",p.roi.y);
    visit("roi_width",p.roi.width); visit("roi_height",p.roi.height);
    visit("min_signal",p.minSignal); visit("background_window",p.backgroundWindow);
    visit("max_line_width",p.maxLineWidth); visit("max_saturated_width",p.maxSaturatedWidth);
    visit("max_pixel_jump",p.maxPixelJump); visit("min_points",p.minPoints);
    visit("min_coverage",p.minCoverage); visit("working_distance_mm",p.workingDistance);
    visit("distance_tolerance_mm",p.distanceTolerance); visit("min_width_mm",p.minWidth);
    visit("max_width_mm",p.maxWidth); visit("min_depth_mm",p.minDepth);
    visit("max_depth_mm",p.maxDepth); visit("min_angle_deg",p.minAngle);
    visit("max_angle_deg",p.maxAngle); visit("max_fit_error_mm",p.maxFitError);
    visit("max_profile_gap_mm",p.maxProfileGap); visit("min_quality",p.minQuality);
    visit("prediction_seconds",p.predictionSeconds); visit("max_tracking_jump_mm",p.maxTrackingJump);
    visit("max_result_age_seconds",p.maxResultAge); visit("max_path_points",p.maxPathPoints);
}
}
bool WeildDetect::Parameters::load(const QString &path, QString *error)
{
    try {
        cv::FileStorage fs;
        if (!WeildFileIO::read(path,fs,error)) return false;
        Parameters next=*this;
        bool typesValid=true;
        fields(next,[&](const char *name,auto &value){
            auto node=fs[name];
            if (node.empty()) return;
            if (!node.isInt() && !node.isReal()) { typesValid=false; return; }
            node >> value;
        });
        if (!typesValid) { if(error)*error="Parameters must be numeric"; return false; }
        if (!next.valid(error)) return false;
        *this=next;
        return true;
    } catch (const cv::Exception &e) { if(error)*error=QString::fromUtf8(e.what()); return false; }
}
bool WeildDetect::Parameters::save(const QString &path, QString *error) const
{
    if (!valid(error)) return false;
    try {
        cv::FileStorage fs("",WeildFileIO::format(path));
        fields(*this,[&](const char *name,const auto &value){fs << name << value;});
        return WeildFileIO::write(path,fs,error);
    } catch (const cv::Exception &e) { if(error)*error=QString::fromUtf8(e.what()); return false; }
}
