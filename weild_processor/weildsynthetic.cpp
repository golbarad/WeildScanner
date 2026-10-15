#include "weildsynthetic.h"
#include <cmath>

WeildDetect::Calibration WeildSynthetic::calibration()
{
    WeildDetect::Calibration c;
    c.imageSize={640,480};
    c.cameraMatrix=cv::Mat(cv::Matx33d(800,0,320,0,800,240,0,0,1)).clone();
    c.distortion=cv::Mat::zeros(1,5,CV_64F);
    c.laserPlane={0,1,1,-200};
    return c;
}
cv::Mat WeildSynthetic::frame(double center, double width, double depth, bool visible,
                              unsigned seed, double noise)
{
    const auto c=calibration();
    cv::Mat image(c.imageSize,CV_32F,cv::Scalar(12));
    if (visible) {
        // At the surface: Y=0, Z=200. A unit depth in the laser plane moves
        // the point by (0,-1/sqrt(2),+1/sqrt(2)), preserving y+z=200.
        std::vector<cv::Point3d> xyz;
        for (double x=-100; x<=100; x+=.02) {
            const double d=width>0 ? depth*std::max(0.,1.-std::abs(x-center)/(width*.5)) : 0;
            xyz.emplace_back(x,-d/std::sqrt(2.),200+d/std::sqrt(2.));
        }
        std::vector<cv::Point2d> pixels;
        cv::projectPoints(xyz,cv::Vec3d(),cv::Vec3d(),c.cameraMatrix,c.distortion,pixels);
        std::vector<double> row(c.imageSize.width,-1);
        for (auto p : pixels) {
            int x=int(std::round(p.x));
            if (x>=0 && x<image.cols) row[x]=p.y;
        }
        for (int x=0; x<image.cols; ++x) if (row[x]>=0)
            for (int y=std::max(0,int(row[x])-6); y<std::min(image.rows,int(row[x])+7); ++y)
                image.at<float>(y,x)+=float(210*std::exp(-.5*std::pow((y-row[x])/1.1,2)));
    }
    cv::Mat disturbance(image.size(),CV_32F);
    cv::RNG rng(seed);
    rng.fill(disturbance,cv::RNG::NORMAL,0,noise);
    image+=disturbance;
    cv::Mat output; image.convertTo(output,CV_8U);
    return output;
}
