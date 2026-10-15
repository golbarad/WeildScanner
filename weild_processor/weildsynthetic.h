#pragma once
#include "weilddetect.h"

// Deterministic, physically projected Mono8 fixture. Never used as real calibration.
class WeildSynthetic
{
public:
    static WeildDetect::Calibration calibration();
    static cv::Mat frame(double centerMm = 0, double widthMm = 20, double depthMm = 6,
                         bool visible = true, unsigned seed = 1, double noise = 2);
};
