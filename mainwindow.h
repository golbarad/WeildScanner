#pragma once
#include <QMainWindow>
#include <QSettings>
#include <QDoubleSpinBox>
#include <QLabel>
#include <vector>
#include "apcamera/cameraopencv.h"
#include "apcamera/weildprocessingpool.h"
#include "apcamera/weildrobotoutput.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent=nullptr);
    ~MainWindow() override;
    void showEvent(QShowEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
public slots:
    void switchToDefCamera();
    void switchToCamera_0(bool on);
    void switchToCamera_1(bool on);
    void switchToCamera_2(bool on);
    void switchToCamera_3(bool on);
private:
    Ui::MainWindow *ui;
    CameraOpenCv *m_camera=nullptr;
    WeildProcessingPool *m_weildPool=nullptr;
    WeildRobotOutput *m_robotOutput=nullptr;
    LogTerminal *m_hwTerm=nullptr;
    QLabel *m_measurement=nullptr;
    QLabel *m_sourceLabel=nullptr;
    WeildDetect::Parameters m_parameters;
    WeildDetect::Calibration m_realCalibration;
    bool m_synthetic=false;
    std::vector<std::pair<double*,QDoubleSpinBox*>> m_parameterEditors;
    void switchCamera(int id, bool on);
    void startSynthetic();
    void openVideo();
    void loadCalibration();
    void saveCalibration();
    void createParameterPanel();
    void applyParameters();
    void fillParameters();
    void showMeasurement(WeildDetect::Result result);
    void report(const QString &message);
    void configureSource(bool synthetic);
};
