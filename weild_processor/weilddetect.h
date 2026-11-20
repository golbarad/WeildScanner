#pragma once

#include <QString>
#include <QMetaType>
#include "cvmetatypes.h"
#include <opencv2/opencv.hpp>
#include <vector>

/* Класс инкапуслирующий отсанированный снимок. Каждый экземпляр
    FrameWeildProcessing владеет собственным экземпляром детектора.
    Линейные размеры указаны в миллиметрах, углы — в градусах;
    ориентация осей камеры соответствует стандарту OpenCV.
*/
class WeildDetect
{
public:
    struct Parameters {
        cv::Rect target_rect;         // Обрабатываемая область изображения
        double minSignal = 18;        //Интенсивность с вычетом фонового сигнала, 8бит
        int backgroundWindow = 31;    // Vertical top-hat kernel, odd.
        int maxLineWidth = 12;
        double maxSaturatedWidth = 5;
        double maxPixelJump = 12;

        int minPoints = 40;
        double minCoverage = 0.55;
        double workingDistance = 200; //расстояние до камеры по оси Z
        double distanceTolerance = 100;
        double minWidth = 1;
        double maxWidth = 60;
        double minDepth = 0.5;
        double maxDepth = 30;
        double minAngle = 20;
        double maxAngle = 150;
        double maxFitError = 0.4;
        double maxProfileGap = 3;
        double minQuality = 0.5;
        double predictionSeconds = 0.15;
        double maxTrackingJump = 5;
        double maxResultAge = 0.10;
        int maxPathPoints = 10000;
        bool valid(QString *error = nullptr) const;
        bool load(const QString &path, QString *error = nullptr);
        bool save(const QString &path, QString *error = nullptr) const;
    };

    struct Calibration {
        cv::Mat cameraMatrix;
        cv::Mat distortion;
        cv::Size imageSize;
        cv::Vec4d laserPlane{0, 0, 0, 0}; // n.X + d = 0, в захваченом фрейме, d в mm.
        cv::Matx44d cameraToRobot = cv::Matx44d::eye();
        bool robotTransformValid = false; // Матрица не калибрована.
        bool valid(QString *error = nullptr) const;
        bool load(const QString &path, QString *error = nullptr);
        bool save(const QString &path, QString *error = nullptr) const;
        Calibration clone() const;
    };

    struct Result {
        quint64 frameId = 0;
        quint64 generation = 0;
        bool detected = false;        // Флаг наличия шва  в теуцшем фрейме
        bool predicted = false;
        QString diagnostic;
        double quality = 0;
        double width = 0, depth = 0, angle = 0;
        double processingMs = 0;      //время обработки кадра (мс)
        cv::Point3d centerCamera;             // Центр камеры C = R^t
        cv::Point3d trackedCamera;
        cv::Point3d centerRobot;
        cv::Point3d leftShoulder, rightShoulder;
        std::vector<cv::Point2f> laserPixels; // Пиксели лазера
        std::vector<cv::Point3d> profile;     // Профиль шва
        cv::Mat overlay;                      // Исходный кадр без разметки (только чтение).
    };

    explicit WeildDetect(const Parameters &parameters, const Calibration &calibration);
    WeildDetect();
    bool configure(const Parameters &parameters, const Calibration &calibration,
                   QString *error = nullptr);
    const Parameters &parameters() const { return m_parameters; }
    const Calibration &calibration() const { return m_calibration; }
    //хендлер обработки кадра
    Result process(const cv::Mat &image, quint64 frameId, qint64 timestampNs) const;

private:
    Parameters m_parameters;
    Calibration m_calibration;
    //детектирование линии
    std::vector<cv::Point2f> extractLine(const cv::Mat &gray) const;
    bool triangulate(Result &result, cv::Size imageSize) const;
    bool findGroove(Result &result) const;
};

Q_DECLARE_METATYPE(WeildDetect::Result)
