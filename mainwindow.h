#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSettings>
#include "apcamera/cameraopencv.h"
#include "apcamera/frameprocessingpool.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

    //объект камеры
    CameraOpenCv *m_camera;
    //объект обработки кадров
    FrameProcessingPool *m_procPool;
    //параметры детектирования
    OBJECT_PARAMS m_objParams;


    //терминал событий оборудования
    LogTerminal *m_hwTerm;
    //терминал событий навигации
    LogTerminal *m_apTerm;


public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void showEvent(QShowEvent * event) override;
    void closeEvent(QCloseEvent * event) override;

    bool loadObjParams();
    bool saveObjParams();
    void formApply();
    void formFill();


public slots:
    //переключение камер
    void switchToDefCamera();
    void switchToCamera_0(bool on);
    void switchToCamera_1(bool on);
    void switchToCamera_2(bool on);
    void switchToCamera_3(bool on);

private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
