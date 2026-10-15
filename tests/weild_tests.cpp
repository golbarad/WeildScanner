#include "weild_processor/weilddetect.h"
#include "weild_processor/weildsynthetic.h"
#include "weild_processor/weildtracker.h"
#include "weild_processor/weildprocessingpool.h"
#include "weild_processor/weildrobotoutput.h"
#include "apcamera/cameraopencv.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <iostream>
#include <stdexcept>
#include <cmath>

static int checks=0;
static void check(bool ok,const std::string &what) {
    ++checks;
    if(!ok) throw std::runtime_error(what);
}
static void near(double actual,double expected,double tolerance,const char *what) {
    check(std::abs(actual-expected)<=tolerance,std::string(what)+": got "+std::to_string(actual)+", expected "+std::to_string(expected));
}

static cv::Mat distortedFixture(const WeildDetect::Calibration &calibration)
{
    std::vector<cv::Point3d> xyz;
    for(double x=-100;x<100;x+=.04) {
        const double d=6*std::max(0.,1.-std::abs(x-12)/10.);
        xyz.emplace_back(x,-d/std::sqrt(2.),200+d/std::sqrt(2.));
    }
    std::vector<cv::Point2d> pixels;
    cv::projectPoints(xyz,cv::Vec3d(),cv::Vec3d(),calibration.cameraMatrix,calibration.distortion,pixels);
    cv::Mat image(calibration.imageSize,CV_8U,cv::Scalar(10));
    // OpenCV fixed-point coordinates preserve fractional raster placement.
    for(size_t i=1;i<pixels.size();++i)
        cv::line(image,cv::Point(cvRound(pixels[i-1].x*16),cvRound(pixels[i-1].y*16)),
            cv::Point(cvRound(pixels[i].x*16),cvRound(pixels[i].y*16)),cv::Scalar(220),2,cv::LINE_AA,4);
    return image;
}

int main(int argc,char **argv)
{
    QCoreApplication app(argc,argv);
    try {
        WeildDetect::Parameters p;
        auto c=WeildSynthetic::calibration();
        WeildDetect detector(p,c);
        auto image=WeildSynthetic::frame(2.5);
        auto r=detector.process(image,1,1000000000);
        check(r.detected,"Synthetic groove not detected: "+r.diagnostic.toStdString());
        near(r.width,20,.4,"width"); near(r.depth,6,.3,"depth");
        near(r.angle,2*std::atan(10./6)*180/CV_PI,2,"angle");
        near(r.centerCamera.x,2.5,.3,"root X");
        near(r.centerCamera.z,200+6/std::sqrt(2.),.3,"root Z");
        check(r.quality>.5,"quality");
        auto distortedCalibration=c.clone();
        distortedCalibration.distortion.at<double>(0,0)=-.25;
        distortedCalibration.distortion.at<double>(0,1)=.1;
        distortedCalibration.distortion.at<double>(0,2)=.005;
        auto distorted=WeildDetect(p,distortedCalibration).process(distortedFixture(distortedCalibration),2,2);
        check(distorted.detected,"distorted fixture: "+distorted.diagnostic.toStdString());
        near(distorted.width,20,.6,"distortion-corrected width");
        near(distorted.depth,6,.4,"distortion-corrected depth");
        near(distorted.centerCamera.x,12,.4,"distortion-corrected center");
        check(r.laserPixels.size()>600,"line coverage");
        double fractional=0;
        for(auto pixel:r.laserPixels) fractional+=std::abs(pixel.y-std::round(pixel.y));
        check(fractional>5,"line should contain subpixel coordinates");

        for(double offset:{-15.,0.,15.}) {
            auto moved=detector.process(WeildSynthetic::frame(offset,24,8,true,12,5),2,2000000000);
            check(moved.detected,"Noisy shifted groove: "+moved.diagnostic.toStdString());
            near(moved.centerCamera.x,offset,.4,"shifted center");
            near(moved.width,24,.7,"shifted width");
        }
        auto smoke=image.clone();
        smoke.convertTo(smoke,CV_8U,.6,40);
        cv::circle(smoke,{110,90},18,cv::Scalar(255),-1);
        cv::rectangle(smoke,{420,10,12,150},cv::Scalar(255),-1);
        auto noisy=detector.process(smoke,3,3000000000);
        check(noisy.detected,"Smoke/isolated glare fixture: "+noisy.diagnostic.toStdString());
        near(noisy.depth,6,.5,"smoke depth");
        check(!detector.process(cv::Mat::zeros(480,640,CV_8U),4,4).detected,"dark frame");
        check(!detector.process(cv::Mat(480,640,CV_8U,cv::Scalar(255)),4,4).detected,"saturated frame");
        check(!detector.process(WeildSynthetic::frame(0,20,0),4,4).detected,"flat surface is not a V groove");
        check(!detector.process(WeildSynthetic::frame(0,20,-6),4,4).detected,"ridge is not a groove");
        check(!detector.process(cv::Mat(),4,4).detected,"empty input");
        check(!detector.process(cv::Mat::zeros(480,640,CV_16U),4,4).detected,"unsupported format diagnosed");
        WeildDetect uncalibrated;
        auto uncal=uncalibrated.process(image,4,4);
        check(!uncal.detected && !uncal.laserPixels.empty() && uncal.profile.empty(),"no invented metric scale");
        auto wrongSize=detector.process(image(cv::Rect(0,0,320,480)),4,4);
        check(!wrongSize.detected && wrongSize.diagnostic.contains("size"),"resolution mismatch");
        auto invalid=c.clone(); invalid.cameraMatrix.at<double>(0,0)=0;
        check(!invalid.valid(),"invalid intrinsic matrix");
        invalid=c.clone(); invalid.laserPlane={0,0,0,0};
        check(!invalid.valid(),"invalid plane");
        invalid=c.clone(); invalid.robotTransformValid=true; invalid.cameraToRobot(0,0)=2;
        check(!invalid.valid(),"non-rigid robot transform");
        auto old=p; old.backgroundWindow=4; check(!old.valid(),"invalid kernel");
        old=p; old.roi={0,0,100,0}; check(!old.valid(),"invalid ROI");
        old=p; old.workingDistance=500;
        check(!WeildDetect(old,c).process(image,5,5).detected,"working distance gate");
        old=p; old.roi={600,0,200,480};
        check(!WeildDetect(old,c).process(image,5,5).detected,"clipped ROI without groove");

        QTemporaryDir dir;
        check(dir.isValid(),"temporary directory");
        QString error;
        check(c.save(dir.filePath("calibration.yml"),&error),"save calibration");
        WeildDetect::Calibration loaded;
        check(loaded.load(dir.filePath("calibration.yml"),&error),"load calibration");
        check(!loaded.robotTransformValid,"no implicit robot transform");
        check(c.save(dir.filePath(QString::fromUtf8("калибровка.xml")),&error),"Unicode XML save");
        check(loaded.load(dir.filePath(QString::fromUtf8("калибровка.xml")),&error),"Unicode XML load");
        check(p.save(dir.filePath("parameters.yml"),&error),"save parameters");
        WeildDetect::Parameters loadedParams;
        check(loadedParams.load(dir.filePath("parameters.yml"),&error),"load parameters");
        near(loadedParams.maxFitError,p.maxFitError,1e-9,"parameter round trip");

        WeildTracker tracker;
        //r.timestampNs=1000000000;
        check(tracker.update(r,p).detected,"initial tracking");
        //r.timestampNs+=10000000;
        r.centerCamera.x+=.1;
        auto tracked=tracker.update(r,p);
        check(tracked.detected,"moving tracking");
        auto predicted=tracker.predict(/*r.timestampNs+50000000*/0,p);
        check(predicted.predicted && !predicted.detected,"short loss prediction");
        check(predicted.trackedCamera.x>tracked.trackedCamera.x,"velocity prediction");
        check(!tracker.predict(/*r.timestampNs+200000000*/0,p).predicted,"prediction expires");
        auto jump=r;
        //jump.timestampNs+=20000000;
        jump.centerCamera.x+=50;
        auto rejected=tracker.update(jump,p);
        check(!rejected.detected && rejected.predicted,"tracking outlier rejection");
        check(!tracker.update(r,p).detected,"out-of-order rejection");
        //r.timestampNs+=500000000;
        check(tracker.update(r,p).detected,"reacquisition");
        auto packet=WeildRobotOutput::packet(r);
        check(!packet.contains("center") && packet["transport"]=="stub","robot stub without transform");

        // Real worker threads, bounded mailbox, generation reset, recording and capture shutdown.
        WeildProcessingPool pool;
        c.robotTransformValid=true; c.cameraToRobot(0,3)=100;
        check(pool.configure(p,c,&error),"pool config");
        check(pool.startRecording(dir.filePath("measurements.jsonl")),"start recording");
        QEventLoop loop;
        int measured=0, predictions=0, lost=0;
        quint64 lastId=0;
        bool monotonic=true, robotCorrect=true;
        QObject::connect(&pool,&WeildProcessingPool::measurementReady,&loop,[&](WeildDetect::Result result){
            if(result.detected) {
                ++measured; monotonic=monotonic && result.frameId>lastId; lastId=result.frameId;
                robotCorrect=robotCorrect && result.robotValid && std::abs(result.centerRobot.x-result.trackedCamera.x-100)<1e-7;
            }
            if(result.predicted) ++predictions;
            if(!result.detected && !result.predicted) ++lost;
        });
        for(int i=0;i<20;++i)
            pool.submitFrame(image, /*WeildDetect::monotonicNs()*/0);

        QTimer::singleShot(300,&loop,&QEventLoop::quit); loop.exec();
        check(measured>=1,"worker result");
        check(pool.droppedFrames()>=19,"bounded latest-frame mailbox");
        check(monotonic && robotCorrect,"ordered results and robot coordinates");
        check(lost>=1,"watchdog reports complete capture loss");
        check(predictions>=1,"watchdog predicts short capture loss");
        check(pool.path().size()==size_t(measured),"predictions excluded from measured path");
        pool.stopRecording();
        QFile recording(dir.filePath("measurements.jsonl")); check(recording.open(QIODevice::ReadOnly),"read recording");
        auto first=QJsonDocument::fromJson(recording.readLine()).object();
        check(first.contains("measured_root_camera_mm"),"recorded measurement schema");
        pool.submitFrame(image,/*WeildDetect::monotonicNs()*/0);
        QCoreApplication::processEvents(); // Dispatch before invalidating the generation.
        pool.reset();
        QTimer::singleShot(80,&loop,&QEventLoop::quit); loop.exec();
        check(pool.path().empty(),"old-generation pending frame discarded");
        p.maxPathPoints=2;
        check(pool.configure(p,c,&error),"bounded path config");
        {
            CameraOpenCv camera;
            QObject::connect(&camera,&CameraOpenCv::frameCaptured,&pool,
                [&](cv::Mat frame,qint64 time){pool.submitFrame(frame,time);},Qt::DirectConnection);
            camera.startSynthetic(80);
            QTimer::singleShot(400,&loop,&QEventLoop::quit); loop.exec();
            camera.stopCamera();
        }
        check(!pool.path().empty(),"synthetic capture through pool");
        check(pool.path().size()<=2,"bounded measured path");
        std::vector<double> times;
        for(int i=0;i<30;++i) {
            auto sample=detector.process(image,i+100,/*WeildDetect::monotonicNs()*/0);
            check(sample.detected,"benchmark measurement remains valid");
            times.push_back(sample.processingMs);
        }
        std::sort(times.begin(),times.end());
        std::cout << "PASS " << checks << " checks; width=" << r.width << " depth=" << r.depth
                  << " angle=" << r.angle << " detector_median_ms=" << times[times.size()/2]
                  << " detector_p95_ms=" << times[size_t(times.size()*.95)] << std::endl;
        return 0;
    } catch(const std::exception &e) {
        std::cerr << "FAIL after " << checks << " checks: " << e.what() << std::endl;
        return 1;
    }
}
