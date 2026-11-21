#include "apcamera/easyimageview.h"
#include "weild_processor/weildprocessingpool.h"
#include "weild_processor/weildsynthetic.h"
#include <QSignalSpy>
#include <QTest>

class DualFrameTests : public QObject
{
    Q_OBJECT
private slots:
    void detectorSharesUnmodifiedFrame()
    {
        const auto image=WeildSynthetic::frame(2.5);
        const auto original=image.clone();
        const WeildDetect detector({},WeildSynthetic::calibration());
        const auto result=detector.process(image,1,0);

        QVERIFY2(result.detected,qPrintable(result.diagnostic));
        QCOMPARE(result.landmarkPixels.size(),size_t(3));
        QVERIFY(result.overlay.left_frame.data==image.data);
        QVERIFY(result.overlay.right_frame.data==image.data);
        QCOMPARE(cv::norm(image,original,cv::NORM_INF),0.0);
    }

    void poolPublishesAndClearsPair()
    {
        WeildProcessingPool pool;
        QVERIFY(pool.configure({},WeildSynthetic::calibration()));
        QSignalSpy frames(&pool,&WeildProcessingPool::imageProcessed);
        QSignalSpy measurements(&pool,&WeildProcessingPool::measurementReady);
        const auto image=WeildSynthetic::frame(2.5);
        pool.submitFrame(image,0);
        QTRY_COMPARE_WITH_TIMEOUT(measurements.size(),1,2000);
        QCOMPARE(frames.size(),1);

        const auto pair=qvariant_cast<FrameSynchronizer::DualFrame>(frames.first().first());
        const auto result=qvariant_cast<WeildDetect::Result>(measurements.first().first());
        QVERIFY(!pair.left_frame.empty());
        QVERIFY(pair.left_frame.data==pair.right_frame.data);
        QVERIFY(pair.left_frame.data!=image.data); // The pool owns a snapshot of the input.
        QVERIFY(result.overlay.left_frame.data==pair.left_frame.data);
        QVERIFY(result.overlay.right_frame.data==pair.right_frame.data);

        pool.reset();
        const auto cleared=qvariant_cast<WeildDetect::Result>(measurements.last().first());
        QVERIFY(cleared.overlay.empty());
        QCOMPARE(frames.size(),1);
    }

    void viewRendersEachSide()
    {
        EasyImageView view;
        view.resize(244,120);
        const auto render=[&] {
            QImage canvas(view.size(),QImage::Format_RGB32);
            view.render(&canvas);
            return canvas;
        };
        const auto blank=render();

        WeildDetect::Result result;
        result.overlay.left_frame=cv::Mat(120,120,CV_8UC1,cv::Scalar(64));
        result.overlay.right_frame=cv::Mat(120,120,CV_8UC3,cv::Scalar(30,60,120));
        result.landmarkPixels={{60,60}};
        view.updateResult(result);
        const auto both=render();
        QCOMPARE(both.pixelColor(30,70),QColor(64,64,64));
        QCOMPARE(both.pixelColor(154,70),QColor(120,60,30));
        QVERIFY(both.pixelColor(65,60).red()>200);
        QCOMPARE(both.pixelColor(189,60),QColor(120,60,30));

        result.overlay.left_frame.release();
        view.updateResult(result);
        QCOMPARE(render().pixelColor(122,70),QColor(120,60,30));

        view.updateResult({});
        QCOMPARE(render(),blank);
    }
};

QTEST_MAIN(DualFrameTests)
#include "dualframe_tests.moc"
