#include "mainwindow.h"
#include "./ui_mainwindow.h"

//запускаем систему логирования
//0 канал - общие события
//1 канал - события камеры и навигации
LOGGER_ALLOC(1)

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    //Cоздаем основные объекты приложения
    m_camera = new CameraOpenCv();
    // //m_camera->startCamera(0, 50);
    // //switchToCamera_0(true);
    // m_procPool = new FrameProcessingPool();
    // //вид процесса навигации
    // ui->m_navView->m_scannerPos =  &m_procPool->m_scannerPos;
    // m_procPool->setObjParams(&m_objParams);

    m_FacePool = new FaceProcessingPool();
    ui->m_camViw->m_faces =  &m_FacePool->m_faceInfo;

    //логирование
    LOGGER(0)->copy_to_stdout = true;
    LOGGER(0)->copy_to_stderr = true;
    LOGGER(1)->copy_to_stdout = true;
    LOGGER(1)->copy_to_stderr = true;

    //терминал событий
    m_hwTerm = new LogTerminal();
    m_hwTerm->connectLogCh(0);
    //вкладка терминалов
    ui->tabWidget->addTab(m_hwTerm, "Hardware");


    //так подключаем камеру сразу к отображателю
    connect(m_camera, SIGNAL(frameCap(cv::Mat)), ui->m_camViw, SLOT(updateImage(cv::Mat)));
    //в рабочем варианте камеру подключаем к пулу обработчиков и другие объекты
    // connect(m_camera, SIGNAL(frameCap(cv::Mat)), m_procPool, SLOT(processFrame(cv::Mat)));
    // connect(m_procPool, SIGNAL(imageProcessed(cv::Mat)), ui->m_camViw, SLOT(updateImage(cv::Mat)));
    // connect(m_procPool, SIGNAL(imageProcessed(cv::Mat)), ui->m_navView, SLOT(update()));
    // connect(m_procPool, SIGNAL(drawAxesAngle()), ui->m_navView, SLOT(update()));
    connect(m_camera, SIGNAL(frameCap(cv::Mat)), m_FacePool, SLOT(processFrame(cv::Mat)));
    //connect(m_FacePool, SIGNAL(u), ui->m_navView, SLOT(update()));

    //меню подключения камеры
    connect(ui->actionCamera0, SIGNAL(triggered(bool)), this, SLOT(switchToCamera_0(bool)));
    connect(ui->actionCamera1, SIGNAL(triggered(bool)), this, SLOT(switchToCamera_1(bool)));
    connect(ui->actionCamera2, SIGNAL(triggered(bool)), this, SLOT(switchToCamera_2(bool)));
    connect(ui->actionCamera3, SIGNAL(triggered(bool)), this, SLOT(switchToCamera_3(bool)));

    //меню СNC калибровка сканера на стенде
    connect(ui->actionZero_Position_mode, SIGNAL(toggled(bool)), m_procPool, SLOT(zeroPositionMode(bool)));
    connect(ui->actionSet_zero_position, SIGNAL(triggered(bool)), m_procPool, SLOT(setZeroPosition()));

    //switchToCamera_0(true);
    switchToDefCamera();
    qDebug() << "startTing";
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::showEvent(QShowEvent * event)
{
    QSettings setting(qApp->organizationName(), qApp->applicationName());
    setting.beginGroup("MainForm");
    setGeometry(setting.value("geometry", QRect(400, 100, 1300, 900)).toRect());
    ui->splitter->restoreState(setting.value("splitter").toByteArray());
    ui->splitter_2->restoreState(setting.value("splitter2").toByteArray());
    setting.endGroup();
    event->accept();
}


void MainWindow::closeEvent(QCloseEvent * event)
{
    QSettings setting(qApp->organizationName(), qApp->applicationName());
    setting.beginGroup("MainForm");
    setting.setValue("geometry", QVariant(geometry()));
    setting.setValue("splitter", QVariant(ui->splitter->saveState()));
    setting.setValue("splitter2", QVariant(ui->splitter_2->saveState()));
    setting.endGroup();
    event->accept();
}

void MainWindow::switchToDefCamera()
{
    switch (m_camera->m_cameraId) {
    case 0: switchToCamera_0(true); break;
    case 1: switchToCamera_1(true); break;
    case 2: switchToCamera_2(true); break;
    case 3: switchToCamera_3(true); break;
    }
}

void MainWindow::switchToCamera_0(bool on)
{
    if(on) {
        ui->actionCamera0->setChecked(true);
        ui->actionCamera1->setChecked(false);
        ui->actionCamera2->setChecked(false);
        ui->actionCamera3->setChecked(false);
        m_camera->startCamera(0, 25);
    } else {
        m_camera->stopCamera();
    }
}
void MainWindow::switchToCamera_1(bool on)
{
    if(on) {
        ui->actionCamera0->setChecked(false);
        ui->actionCamera1->setChecked(true);
        ui->actionCamera2->setChecked(false);
        ui->actionCamera3->setChecked(false);
        m_camera->startCamera(1, 25);
    } else {
        m_camera->stopCamera();
    }
}
void MainWindow::switchToCamera_2(bool on)
{
    if(on) {
        ui->actionCamera0->setChecked(false);
        ui->actionCamera1->setChecked(false);
        ui->actionCamera2->setChecked(true);
        ui->actionCamera3->setChecked(false);
        m_camera->startCamera(2, 25);
    } else {
        m_camera->stopCamera();
    }
}
void MainWindow::switchToCamera_3(bool on)
{
    if (on)
    {
        ui->actionCamera0->setChecked(false);
        ui->actionCamera1->setChecked(false);
        ui->actionCamera2->setChecked(false);
        ui->actionCamera3->setChecked(true);
        m_camera->startCamera(3, 25);
    }
    else {
        m_camera->stopCamera();
    }
}

bool MainWindow::loadObjParams()
{
    //если файла нет, то сделаем этот файл по умолчанию
    QFileInfo fi(PATH_NAME_AUTOPILOT_PARAMS_FILE);
    if(!fi.exists()) { saveObjParams(); }

    //загружаем параметры объекта
    cv::FileStorage fs(fi.absoluteFilePath().toUtf8().data(), cv::FileStorage::READ);
    if(fs.isOpened()) {
        fs["tubeCorrect"] >> m_objParams.tubeCorrect;
        fs["tubeRadius"] >> m_objParams.tubeRadius;
        fs["trjOffsetX"] >> m_objParams.trjOffsetX;
        fs["actLowDist"] >> m_objParams.actLowDist;
        fs["lowDist"] >> m_objParams.lowDist;
        fs["actHighDist"] >> m_objParams.actHighDist;
        fs["highDist"] >> m_objParams.highDist;
        fs["timeStopWithoutNav"] >> m_objParams.timeStopWithoutNav;
        //переносим загруженные параметры в форму
        formFill();
    } else {
        std::cout << "can't load file object parameters" << std::endl;
        return false;
    }
    fs.release();
    return true;
}

bool MainWindow::saveObjParams()
{
    //загружаем параметры объекта
    cv::FileStorage fs(PATH_NAME_AUTOPILOT_PARAMS_FILE, cv::FileStorage::WRITE);
    if(fs.isOpened()) {
        fs << "tubeCorrect" << m_objParams.tubeCorrect;
        fs << "tubeRadius" << m_objParams.tubeRadius;
        fs << "trjOffsetX" << m_objParams.trjOffsetX;
        fs << "actLowDist" << m_objParams.actLowDist;
        fs << "lowDist" << m_objParams.lowDist;
        fs << "actHighDist" << m_objParams.actHighDist;
        fs << "highDist" << m_objParams.highDist;
        fs << "timeStopWithoutNav" << m_objParams.timeStopWithoutNav;
    } else {
        std::cout << "can't save file object parameters" << std::endl;
        return false;
    }
    fs.release();
    return true;
}

void MainWindow::formApply()
{
    m_objParams.tubeCorrect = ui->cb_TubeCorrect->isChecked();
    m_objParams.tubeRadius = 0.5 * ui->le_TubeDiametr->text().toDouble();
    if(m_objParams.tubeRadius < 100) { m_objParams.tubeRadius = 100; }
    m_objParams.trjOffsetX = ui->le_Offset->text().toDouble();
    if(m_objParams.trjOffsetX < -10) { m_objParams.trjOffsetX = -10; }
    if(m_objParams.trjOffsetX > 10) { m_objParams.trjOffsetX = 10; }
    if(m_objParams.highDist - m_objParams.lowDist < 50) {
        m_objParams.highDist = m_objParams.lowDist + 50;
    }

    formFill();
    saveObjParams();
}

void MainWindow::formFill()
{
    //переносим загруженные параметры в форму
    ui->cb_TubeCorrect->setChecked(m_objParams.tubeCorrect);
    ui->le_TubeDiametr->setText(QString("%0").arg(2.0 * m_objParams.tubeRadius));
    ui->le_Offset->setText(QString("%0").arg(m_objParams.trjOffsetX));
}