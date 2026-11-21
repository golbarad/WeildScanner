#include "weilddetect.h"
#include "weildfileio.h"
#include <algorithm>
#include <QElapsedTimer>
#include <cmath>
#include <QtNumeric>
#include <QTranslator>
#include <stdexcept>

namespace {

bool fail(QString *error, const QString &message) {
    if (error) *error = message;
    return false;
}
bool isFinite(double value) { return std::isfinite(value); }

double median(std::vector<double> values) {
    auto middle = values.begin() + values.size() / 2;
    std::nth_element(values.begin(), middle, values.end());
    return *middle;
}
struct Line { double a = 0, b = 0, error = 0; };

bool fit(const std::vector<cv::Point2d> &points, Line &line) {
    if (points.size() < 6) return false;
    cv::Vec4f l;
    cv::fitLine(points, l, cv::DIST_HUBER, 0, 0.001, 0.001);
    if (std::abs(l[0]) < 1e-5) return false;
    line.a = l[1] / l[0];
    line.b = l[3] - line.a * l[2];
    std::vector<double> residuals;
    for (auto p : points)
        residuals.push_back(std::abs(p.y - line.a * p.x - line.b) / std::hypot(1., line.a));
    line.error = median(residuals);
    return isFinite(line.a) && isFinite(line.b);
}
cv::Point3d point(const cv::Vec3d &v) { return {v[0], v[1], v[2]}; }
}

bool WeildDetect::Parameters::valid(QString *error) const
{
    const double values[] = {minSignal, maxPixelJump, minCoverage, workingDistance,
        distanceTolerance, minWidth, maxWidth, minDepth, maxDepth, minAngle, maxAngle,
        maxFitError, maxProfileGap, minQuality, predictionSeconds, maxTrackingJump, maxResultAge};

    for (double v : values) {
        if (!isFinite(v))
            return fail(error, "Non-finite parameter");
    }
    if (target_rect.x < 0 || target_rect.y < 0 || target_rect.width < 0 || target_rect.height < 0 ||
        ((target_rect.width == 0) != (target_rect.height == 0))) {
        return fail(error, "Invalid ROI");
    }
    if (backgroundWindow < 5 || backgroundWindow > 501 || backgroundWindow % 2 == 0 ||
        maxLineWidth < 2 || maxLineWidth >= backgroundWindow || maxSaturatedWidth < 0 ||
        minSignal <= 0 || minSignal >= 255 || maxPixelJump <= 0 || minPoints < 20 ||
        minCoverage <= 0 || minCoverage > 1 || workingDistance <= 0 || distanceTolerance <= 0 ||
        minWidth <= 0 || maxWidth <= minWidth || minDepth <= 0 || maxDepth <= minDepth ||
        minAngle <= 0 || maxAngle >= 180 || maxAngle <= minAngle || maxFitError <= 0 ||
        maxProfileGap <= 0 || minQuality < 0 || minQuality > 1 || predictionSeconds < 0 ||
        predictionSeconds > 2 || maxTrackingJump <= 0 || maxResultAge <= 0 ||
        maxResultAge > 5 || maxPathPoints < 1)
    {
        return fail(error, "Invalid detector/tracker parameter range");
    }
    return true;
}

bool WeildDetect::Calibration::valid(QString *error) const
{
    if (cameraMatrix.type() != CV_64FC1 || cameraMatrix.rows != 3 || cameraMatrix.cols != 3 ||
        !cv::checkRange(cameraMatrix) || cameraMatrix.at<double>(0,0) <= 0 ||
        cameraMatrix.at<double>(1,1) <= 0 ||
        std::abs(cameraMatrix.at<double>(2,2) - 1) > 1e-9 ||
        std::abs(cameraMatrix.at<double>(2,0)) > 1e-9 ||
        std::abs(cameraMatrix.at<double>(2,1)) > 1e-9 ||
        std::abs(cameraMatrix.at<double>(0,1)) > 1e-9 ||
        std::abs(cameraMatrix.at<double>(1,0)) > 1e-9 || imageSize.width <= 0 || imageSize.height <= 0)
    {
        return fail(error, "Camera calibration is missing or invalid");
    }

    if (!distortion.empty() && (distortion.type() != CV_64FC1 ||
        (distortion.rows != 1 && distortion.cols != 1) || !cv::checkRange(distortion) ||
        (distortion.total() != 4 && distortion.total() != 5 && distortion.total() != 8 &&
         distortion.total() != 12 && distortion.total() != 14)))
        return fail(error, "Invalid distortion coefficients");

    for (double v : laserPlane.val) {
        if (!isFinite(v)) return fail(error, "Invalid laser plane");
    }
    const double normalLength=cv::norm(cv::Vec3d(laserPlane[0], laserPlane[1], laserPlane[2]));
    if (!isFinite(normalLength) || normalLength < 1e-9 ||
        std::abs(laserPlane[3]) < 1e-9) return fail(error, "Laser plane is missing or passes through camera");

    if (robotTransformValid)
    {
        cv::Mat transform(cameraToRobot);
        if (!cv::checkRange(transform)) return fail(error, "Invalid robot transform");
        for (int j=0; j<4; ++j)
            if (std::abs(cameraToRobot(3,j) - (j == 3 ? 1. : 0.)) > 1e-8)
                return fail(error, "Robot transform must be homogeneous");
        cv::Mat rotation = transform(cv::Rect(0,0,3,3));
        if (cv::norm(rotation.t()*rotation - cv::Mat::eye(3,3,CV_64F)) > 1e-6 ||
            std::abs(cv::determinant(rotation) - 1) > 1e-6)
            return fail(error, "Robot transform must contain a proper rotation");
    }
    return true;
}

WeildDetect::Calibration WeildDetect::Calibration::clone() const
{
    Calibration copy = *this;
    copy.cameraMatrix = cameraMatrix.clone();
    copy.distortion = distortion.clone();
    return copy;
}

bool WeildDetect::Calibration::load(const QString &path, QString *error)
{
    try {
        cv::FileStorage fs;
        if (!WeildFileIO::read(path,fs,error)) return false;
        Calibration next;
        fs["camera_matrix"] >> next.cameraMatrix;
        fs["distortion_coefficients"] >> next.distortion;
        fs["image_width"] >> next.imageSize.width;
        fs["image_height"] >> next.imageSize.height;
        cv::Mat plane, transform;
        fs["laser_plane"] >> plane;
        if (plane.total() != 4 || plane.channels() != 1)
            return fail(error, "laser_plane must contain four coefficients");
        plane.convertTo(plane, CV_64F);
        plane = plane.reshape(1, 1);
        for (int i=0; i<4; ++i) next.laserPlane[i] = plane.at<double>(0,i);
        if (!fs["camera_to_robot"].empty()) {
            fs["camera_to_robot"] >> transform;
            if (transform.rows != 4 || transform.cols != 4 || transform.channels() != 1)
                return fail(error, "camera_to_robot must be 4x4");
            transform.convertTo(transform, CV_64F);
            std::copy(transform.ptr<double>(), transform.ptr<double>()+16, next.cameraToRobot.val);
            next.robotTransformValid = true;
        }
        next.cameraMatrix.convertTo(next.cameraMatrix, CV_64F);
        if (!next.distortion.empty()) next.distortion.convertTo(next.distortion, CV_64F);
        if (!next.valid(error)) return false;
        *this = next.clone();
        return true;
    } catch (const cv::Exception &e) { return fail(error, QString::fromUtf8(e.what())); }
}

bool WeildDetect::Calibration::save(const QString &path, QString *error) const
{
    if (!valid(error)) return false;
    try {
        cv::FileStorage fs("",WeildFileIO::format(path));
        fs << "camera_matrix" << cameraMatrix << "distortion_coefficients" << distortion;
        fs << "image_width" << imageSize.width << "image_height" << imageSize.height;
        fs << "laser_plane" << cv::Mat(laserPlane).t();
        if (robotTransformValid) fs << "camera_to_robot" << cv::Mat(cameraToRobot);
        return WeildFileIO::write(path,fs,error);
    } catch (const cv::Exception &e) { return fail(error, QString::fromUtf8(e.what())); }
}

WeildDetect::WeildDetect() = default;
WeildDetect::WeildDetect(const Parameters &p, const Calibration &c)
{
    QString error;
    if(!configure(p,c,&error)) throw std::invalid_argument(error.toStdString());
}
bool WeildDetect::configure(const Parameters &p, const Calibration &c, QString *error)
{
    if (!p.valid(error)) return false;
    // Empty calibration deliberately permits pixel-only diagnostics.
    if (!c.cameraMatrix.empty() && !c.valid(error)) return false;
    m_parameters = p;
    m_calibration = c.clone();
    return true;
}

std::vector<cv::Point2f> WeildDetect::extractLine(const cv::Mat &gray) const
{
    const auto &p = m_parameters;
    cv::Rect target_rect = p.target_rect.empty() ? cv::Rect(0,0,gray.cols,gray.rows) :  p.target_rect & cv::Rect(0,0,gray.cols,gray.rows);
    if (target_rect.width < p.minPoints || target_rect.height < p.backgroundWindow) return {};
    cv::Mat smooth, signal;
    //увеличение контрастности между фоном и лазером
    cv::GaussianBlur(gray(target_rect), smooth, cv::Size(3,3), 0.7);
    //Выдление элементов ярче окружающего фона
    auto kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(1,p.backgroundWindow));
    cv::morphologyEx(smooth, signal, cv::MORPH_TOPHAT, kernel);

    std::vector<cv::Point2f> candidates;
    for (int x=0; x<target_rect.width; x++)
    {
        double best = 0;
        int yBest = -1;
        //прорхдимя по свсе  y
        for (int y=1; y< target_rect.height-1; y++)
        {
            //ищем самый ярких пиксель в  столбце
            const double value = signal.at<uchar>(y,x);
            if (value > best) {
                best = value; yBest = y;
            }
        }
        if (best < 10 || yBest < 1) continue;
        int low = yBest, high = yBest;

        //определяем диапазон сверху и снизу относительно пискеля (будут видны только горизонтальные линии))))
        while (low > 0 && signal.at<uchar>(low-1,x) > best * 0.5)
            low--;
        while (high+1 < target_rect.height && signal.at<uchar>(high+1,x) > best * 0.5)
            high++;
        if (high-low+1 > 50 || low == 0 || high == target_rect.height-1)
            continue;

        //считаем якрось пискелей
        int saturated = 0;
        double total=0, moment=0;
        //выделяем нужый диапазон
        const int length = std::max(2, (high-low+1));
        for (int y=std::max(0,yBest-length); y<=std::min(target_rect.height-1,yBest+length); y++)
        {
            if (gray.at<uchar>(y + target_rect.y,x + target_rect.x) >= 254)
                saturated++;
            const double weight = signal.at<uchar>(y,x);
            total += weight;
            moment += y*weight;
        }
        static double saa_last = 0;
        //std::cout << " saaaa "  << p.maxSaturatedWidth;
        if (p.maxSaturatedWidth - 0.01 < saa_last || saa_last > p.maxSaturatedWidth + 0.01) {
            saa_last = p.maxSaturatedWidth;
            std::cout << "a " << saa_last << " \n";
        }
        if (saturated > p.maxSaturatedWidth || total <= 0)
            continue;
        //если якрость попдает в диапазон - добавляем точку
        candidates.emplace_back(float(x+target_rect.x), float(moment/total+target_rect.y));
    }
    std::vector<cv::Point2f> line;
    for (size_t i=0; i<candidates.size(); ++i)
    {
        std::vector<double> neighbours;
        const size_t begin = i > 2 ? i-2 : 0;
        for (size_t j=begin; j<std::min(candidates.size(),i+3); ++j)
            neighbours.push_back(candidates[j].y);
        if (std::abs(candidates[i].y - median(neighbours)) <= p.maxPixelJump)
        line.push_back(candidates[i]);
    }
    return line;
}

bool WeildDetect::triangulate(Result &r, cv::Size size) const
{
    QString error;
    if (!m_calibration.valid(&error)) return fail(&r.diagnostic, error);
    if (size != m_calibration.imageSize)
        return fail(&r.diagnostic, "Frame size does not match calibration");

    std::vector<cv::Point2f> rays;
    cv::undistortPoints(r.laserPixels, rays, m_calibration.cameraMatrix, m_calibration.distortion);
    const auto plane = m_calibration.laserPlane;
    const cv::Vec3d normal(plane[0],plane[1],plane[2]);
    for (auto pixel : rays)
    {
        const cv::Vec3d ray(pixel.x,pixel.y,1);
        const double denominator = normal.dot(ray);
        if (std::abs(denominator) < 1e-8*cv::norm(normal)*cv::norm(ray)) continue;
        const double scale = -plane[3]/denominator;
        if (!isFinite(scale) || scale <= 0 ||
            std::abs(scale-m_parameters.workingDistance) > m_parameters.distanceTolerance)
            continue;
        r.profile.push_back(point(ray*scale));
    }
    if (r.profile.size() < size_t(m_parameters.minPoints))
        return fail(&r.diagnostic, "Too few valid triangulated points / distance outside range");
    return true;
}

bool WeildDetect::findGroove(Result &r) const
{
    const auto &p = m_parameters;
    cv::Vec3d normal(m_calibration.laserPlane[0],m_calibration.laserPlane[1],m_calibration.laserPlane[2]);
    const double length = cv::norm(normal);
    normal /= length;
    cv::Vec3d u = cv::Vec3d(1,0,0) - normal*normal[0];
    if (cv::norm(u) < 0.1)
        return fail(&r.diagnostic, "Laser plane cannot be profiled along image columns");
    u /= cv::norm(u);
    cv::Vec3d v = normal.cross(u);
    if (v[2] < 0) v = -v;
    if (std::abs(v[2]) < 0.05)
        return fail(&r.diagnostic, "Laser plane has insufficient depth sensitivity");
    const cv::Vec3d origin = normal*(-m_calibration.laserPlane[3]/length);
    std::vector<cv::Point2d> points;
    for (auto xyz : r.profile)
    {
        cv::Vec3d delta = cv::Vec3d(xyz.x,xyz.y,xyz.z)-origin;
        points.emplace_back(delta.dot(u), delta.dot(v));
    }
    std::sort(points.begin(),points.end(),[](auto a,auto b){return a.x<b.x;});
    for (size_t i=1; i<points.size(); ++i)
        if (points[i].x-points[i-1].x > p.maxProfileGap)
            return fail(&r.diagnostic, "Profile has a large gap");
    const size_t edge = std::max(size_t(6), points.size()/6);
    std::vector<cv::Point2d> surface(points.begin(),points.begin()+edge);
    surface.insert(surface.end(), points.end()-edge, points.end());

    Line baseline;
    if (!fit(surface,baseline) || baseline.error > p.maxFitError)
        return fail(&r.diagnostic, "Surface is not visible on both sides");
    std::vector<double> depths;
    const double baselineNorm = std::hypot(1.,baseline.a);
    for (auto xy : points) depths.push_back((xy.y-baseline.a*xy.x-baseline.b)/baselineNorm);

    // Median depth rejects a single isolated spike when locating the root.
    std::vector<double> smoothDepth = depths;
    for (size_t i=2; i+2<depths.size(); ++i)
        smoothDepth[i] = median({depths[i-2],depths[i-1],depths[i],depths[i+1],depths[i+2]});
    size_t peak = std::max_element(smoothDepth.begin(),smoothDepth.end())-smoothDepth.begin();
    double peakDepth = smoothDepth[peak];
    if (peak < edge || peak+edge >= points.size() || peakDepth < p.minDepth || peakDepth > p.maxDepth)
        return fail(&r.diagnostic, "No admissible V groove");
    std::vector<cv::Point2d> left, right;
    for (size_t i=edge; i+edge<points.size(); ++i)
    {
        if (depths[i] < std::max(peakDepth*.15, p.maxFitError) || depths[i] > peakDepth*.9) continue;
        (i<peak ? left : right).push_back(points[i]);
    }
    Line l, rr;
    if (!fit(left,l) || !fit(right,rr) || l.error > p.maxFitError || rr.error > p.maxFitError ||
        l.a <= baseline.a+0.05 || rr.a >= baseline.a-0.05)
        return fail(&r.diagnostic, "V flanks could not be fitted reliably");

    const double xLeft = (baseline.b-l.b)/(l.a-baseline.a);
    const double xRight = (baseline.b-rr.b)/(rr.a-baseline.a);
    const double xRoot = (rr.b-l.b)/(l.a-rr.a);
    cv::Point2d a(xLeft, baseline.a*xLeft+baseline.b);
    cv::Point2d b(xRight, baseline.a*xRight+baseline.b);
    cv::Point2d root(xRoot, l.a*xRoot+l.b);
    if (!(points.front().x < xLeft && xLeft < xRoot && xRoot < xRight && xRight < points.back().x))
        return fail(&r.diagnostic, "Groove shoulders outside visible profile");

    const double depth = (root.y-baseline.a*root.x-baseline.b)/baselineNorm;
    const double width = cv::norm(b-a);
    const auto vl = a-root, vr = b-root;
    const double angle = std::acos(std::clamp(vl.dot(vr)/(cv::norm(vl)*cv::norm(vr)),-1.,1.))*180/CV_PI;
    // Валидация четрехсегментой модели
    int inliers = 0;
    double squaredError = 0;
    for (auto xy : points) {
        const Line &segment = xy.x < xLeft || xy.x > xRight ? baseline : (xy.x < xRoot ? l : rr);
        double residual = std::abs(xy.y-segment.a*xy.x-segment.b)/std::hypot(1.,segment.a);
        if (residual <= p.maxFitError*3) ++inliers;
        squaredError += std::min(residual*residual, 100.*p.maxFitError*p.maxFitError);
    }
    const double inlierFraction = double(inliers)/points.size();
    const double rmse = std::sqrt(squaredError/points.size());
    if (width < p.minWidth || width > p.maxWidth || depth < p.minDepth || depth > p.maxDepth ||
        angle < p.minAngle || angle > p.maxAngle || inlierFraction < .85 || rmse > 2*p.maxFitError)
        return fail(&r.diagnostic, "Groove geometry or residual outside limits");

    r.quality *= inlierFraction * std::exp(-rmse/(3*p.maxFitError));
    if (r.quality < p.minQuality)
        return fail(&r.diagnostic, "Low measurement quality");

    auto xyz = [&](cv::Point2d xy) {
        return point(origin+u*xy.x+v*xy.y);
    };
    r.centerCamera = xyz(root);
    r.trackedCamera = r.centerCamera;
    r.leftShoulder = xyz(a);
    r.rightShoulder = xyz(b);
    r.width = width; r.depth = depth; r.angle = angle;
    r.detected = true;
    r.diagnostic = "Measured";
    return true;
}

// Обработка захваченного кадра
WeildDetect::Result WeildDetect::process(const cv::Mat &image, quint64 id, qint64 timestamp) const
{
    Result r;
    r.frameId=id;
    QElapsedTimer timer; timer.start();
    try {
        if (image.empty() || image.depth() != CV_8U ||
            (image.channels()!=1 && image.channels()!=3 && image.channels()!=4)) {
            r.diagnostic = "Expected nonempty Mono8, BGR8 or BGRA8 frame";
        }
        else {
            cv::Mat gray;
            if (image.channels()==1) gray=image;
            else cv::cvtColor(image,gray,image.channels()==3 ? cv::COLOR_BGR2GRAY : cv::COLOR_BGRA2GRAY);
            // Копируем ссылку на текущий фрейм
            // (сейчас для теста левый и правый фрейм ссылаютсся на один и тот же кадр)
            r.overlay.left_frame = image;
            r.overlay.right_frame = image;
            r.laserPixels = extractLine(gray);
            // Захватываемя область
            cv::Rect roi=m_parameters.target_rect.empty() ? cv::Rect(0,0,gray.cols,gray.rows) :
                m_parameters.target_rect & cv::Rect(0,0,gray.cols,gray.rows);
            r.quality = roi.width > 0 ? double(r.laserPixels.size())/roi.width : 0;
            if (r.laserPixels.size() < size_t(m_parameters.minPoints) || r.quality < m_parameters.minCoverage)
                r.diagnostic = QObject::tr("Laser line missing or insufficient coverage");
            else if (triangulate(r,image.size()))
            {
                r.quality *= double(r.profile.size())/r.laserPixels.size();
                if (findGroove(r)) {
                    std::vector<cv::Point3d> xyz{r.leftShoulder,r.centerCamera,r.rightShoulder};
                    cv::projectPoints(xyz,cv::Vec3d(),cv::Vec3d(),m_calibration.cameraMatrix,
                        m_calibration.distortion,r.landmarkPixels);
                }
            }
        }
    }
    catch (const cv::Exception &e) {
        r.detected=false;
        r.diagnostic=QString::fromUtf8(e.what());
    }
    catch (const std::exception &e) {
        r.detected=false;
        r.diagnostic=QString::fromUtf8(e.what());
    }
    r.processingMs= timer.elapsed();
    return r;
}
