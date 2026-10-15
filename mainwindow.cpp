#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include "apcamera/weildsynthetic.h"
#include <QFileDialog>
#include <QFormLayout>
#include <QScrollArea>
#include <QPushButton>
#include <QMenu>
#include <QMessageBox>
#include <QCloseEvent>
#include <QShowEvent>
#include <QTimer>
#include <QCoreApplication>
#include <algorithm>

LOGGER_ALLOC(1)

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setMinimumSize(1000,720);
    resize(1300,900);
    setWindowTitle(tr("WeildScanner — лазерный датчик шва"));
    m_weildPool=new WeildProcessingPool(this);
    m_robotOutput=new WeildRobotOutput(this);
    m_camera=new CameraOpenCv;
    m_hwTerm=new LogTerminal;
    m_hwTerm->connectLogCh(0);
    ui->tabWidget->addTab(m_hwTerm,tr("Диагностика"));
    ui->widget->hide(); // Legacy tape-navigation settings remain in the .ui file.
    m_sourceLabel=new QLabel(tr("Выберите камеру, видео или синтетический источник."));
    m_sourceLabel->setWordWrap(true);
    m_measurement=new QLabel(tr("Измерений пока нет."));
    m_measurement->setWordWrap(true);
    ui->verticalLayout_2->addWidget(m_sourceLabel);
    ui->verticalLayout_2->addWidget(m_measurement);
    createParameterPanel();

    // Only this bounded mailbox is called in the acquisition thread. No GUI work there.
    connect(m_camera,&CameraOpenCv::frameCaptured,m_weildPool,
        [this](cv::Mat frame,qint64 timestamp){m_weildPool->submitFrame(frame,timestamp);},Qt::DirectConnection);
    connect(m_weildPool,&WeildProcessingPool::measurementReady,ui->m_camViw,&EasyImageView::updateResult);
    connect(m_weildPool,&WeildProcessingPool::measurementReady,ui->m_navView,&WeildProfileView::showResult);
    connect(m_weildPool,&WeildProcessingPool::measurementReady,this,&MainWindow::showMeasurement);
    connect(m_weildPool,&WeildProcessingPool::measurementReady,m_robotOutput,&WeildRobotOutput::publish);
    connect(m_weildPool,&WeildProcessingPool::diagnostic,this,&MainWindow::report);
    connect(m_camera,&CameraOpenCv::captureError,this,&MainWindow::report);
    connect(m_camera,&CameraOpenCv::sourceFinished,this,[this]{report(tr("Видеофайл закончился."));});
    connect(ui->actionCamera0,&QAction::triggered,this,&MainWindow::switchToCamera_0);
    connect(ui->actionCamera1,&QAction::triggered,this,&MainWindow::switchToCamera_1);
    connect(ui->actionCamera2,&QAction::triggered,this,&MainWindow::switchToCamera_2);
    connect(ui->actionCamera3,&QAction::triggered,this,&MainWindow::switchToCamera_3);
    ui->menuCamera->setTitle(tr("Источник"));
    ui->menuCamera->addSeparator();
    ui->menuCamera->addAction(tr("Открыть видео…"),this,&MainWindow::openVideo);
    ui->menuCamera->addAction(tr("Синтетический V-шов"),this,&MainWindow::startSynthetic);
    ui->menuCamera->addAction(tr("Остановить"),this,[this]{
        m_camera->stopCamera(); m_weildPool->reset();
        ui->m_navView->showResult({});
        for(auto *a:{ui->actionCamera0,ui->actionCamera1,ui->actionCamera2,ui->actionCamera3}) a->setChecked(false);
        m_sourceLabel->setText(tr("Источник остановлен."));
        m_measurement->setText(tr("Измерения остановлены."));
        m_robotOutput->publish({});
    });
    ui->menuCalibration->setTitle(tr("Калибровка"));
    ui->actionZero_Position_mode->setText(tr("Загрузить калибровку…"));
    ui->actionSet_zero_position->setText(tr("Сохранить текущую калибровку…"));
    connect(ui->actionZero_Position_mode,&QAction::triggered,this,&MainWindow::loadCalibration);
    connect(ui->actionSet_zero_position,&QAction::triggered,this,&MainWindow::saveCalibration);
    auto *recordMenu=ui->menubar->addMenu(tr("Запись"));
    recordMenu->addAction(tr("Записывать измерения JSONL…"),this,[this]{
        QString path=QFileDialog::getSaveFileName(this,tr("Файл измерений"),"measurements.jsonl","JSONL (*.jsonl)");
        if (!path.isEmpty() && m_weildPool->startRecording(path)) report(tr("Запись: ")+path);
    });
    recordMenu->addAction(tr("Завершить запись"),this,[this]{m_weildPool->stopRecording(); report(tr("Запись завершена."));});
    if (QCoreApplication::arguments().contains("--synthetic")) QTimer::singleShot(0,this,&MainWindow::startSynthetic);
}
MainWindow::~MainWindow()
{
    delete m_camera; // Join capture before destroying its direct-connected mailbox.
    delete m_weildPool;
    delete ui;
}
void MainWindow::showEvent(QShowEvent *event)
{
    if (!QCoreApplication::arguments().contains("--smoke")) {
        QSettings s; s.beginGroup("MainForm");
        setGeometry(s.value("geometry",QRect(100,100,1300,900)).toRect());
        ui->splitter->restoreState(s.value("splitter").toByteArray());
        ui->splitter_2->restoreState(s.value("splitter2").toByteArray());
    }
    QMainWindow::showEvent(event);
}
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!QCoreApplication::arguments().contains("--smoke")) {
        QSettings s; s.beginGroup("MainForm");
        s.setValue("geometry",geometry()); s.setValue("splitter",ui->splitter->saveState());
        s.setValue("splitter2",ui->splitter_2->saveState());
    }
    QMainWindow::closeEvent(event);
}
void MainWindow::report(const QString &message)
{
    SLOG(0,message+"\n");
    statusBar()->showMessage(message,10000);
}
void MainWindow::configureSource(bool synthetic)
{
    m_camera->stopCamera();
    m_synthetic=synthetic;
    QString error;
    m_weildPool->configure(m_parameters,synthetic ? WeildSynthetic::calibration() : m_realCalibration,&error);
    ui->m_navView->showResult({});
    m_measurement->setText(tr("Ожидание нового кадра. Интерфейс робота: заглушка."));
    m_robotOutput->publish({});
    for(auto *a:{ui->actionCamera0,ui->actionCamera1,ui->actionCamera2,ui->actionCamera3}) a->setChecked(false);
}
void MainWindow::switchCamera(int id,bool on)
{
    if (!on) {
        m_camera->stopCamera(); m_weildPool->reset();
        m_sourceLabel->setText(tr("Источник остановлен."));
        return;
    }
    configureSource(false);
    QAction *actions[]={ui->actionCamera0,ui->actionCamera1,ui->actionCamera2,ui->actionCamera3};
    actions[id]->setChecked(true);
    m_sourceLabel->setText(tr("Камера %1 · запрос 80 FPS · %2").arg(id)
        .arg(m_realCalibration.valid() ? tr("калибровка загружена") : tr("Калибровка камеры остутствует")));
    const auto size=m_realCalibration.valid() ? m_realCalibration.imageSize : cv::Size(640,480);
    m_camera->startCamera(id,80,QSize(size.width,size.height));
}
void MainWindow::switchToDefCamera() {switchCamera(std::clamp(m_camera->m_cameraId,0,3),true);}
void MainWindow::switchToCamera_0(bool on) {switchCamera(0,on);}
void MainWindow::switchToCamera_1(bool on) {switchCamera(1,on);}
void MainWindow::switchToCamera_2(bool on) {switchCamera(2,on);}
void MainWindow::switchToCamera_3(bool on) {switchCamera(3,on);}

void MainWindow::startSynthetic()
{
    configureSource(true);
    m_sourceLabel->setText(tr("СИНТЕТИКА · ширина 20 мм, глубина 6 мм · тестовая калибровка"));
    m_camera->startSynthetic();
    // const auto size=m_realCalibration.valid() ? m_realCalibration.imageSize : cv::Size(640,480);
    // m_camera->startCamera(1,80,QSize(size.width,size.height));
}
void MainWindow::openVideo()
{
    const auto path=QFileDialog::getOpenFileName(this,tr("Видеозапись лазерного шва"),{},"Video (*.avi *.mp4 *.mkv);;All files (*)");
    if(path.isEmpty()) return;
    configureSource(false);
    m_sourceLabel->setText(tr("Видео: ")+path);
    //m_camera->startVideo(path);
}
void MainWindow::loadCalibration()
{
    auto path=QFileDialog::getOpenFileName(this,tr("Калибровка камеры, лазера и робота"),{},"OpenCV (*.yml *.yaml *.xml)");
    if(path.isEmpty()) return;
    auto next=m_realCalibration.clone(); QString error;
    if(!next.load(path,&error)) {QMessageBox::warning(this,tr("Калибровка"),error); return;}
    m_realCalibration=next;
    if(!m_synthetic) m_weildPool->configure(m_parameters,m_realCalibration);
    report(tr("Калибровка загружена: ")+path);
    if (!m_synthetic) m_sourceLabel->setText(tr("Калибровка загружена. Разрешение кадра должно быть %1×%2.")
        .arg(next.imageSize.width).arg(next.imageSize.height));
}
void MainWindow::saveCalibration()
{
    auto path=QFileDialog::getSaveFileName(this,tr("Калибровка"),"calibration.yml","OpenCV (*.yml *.xml)");
    if(path.isEmpty()) return;
    QString error;
    if(!m_weildPool->calibration().save(path,&error)) QMessageBox::warning(this,tr("Калибровка"),error);
}
void MainWindow::createParameterPanel()
{
    auto *panel=new QWidget;
    auto *form=new QFormLayout(panel);
    auto add=[&](const QString &name,double &value,double low,double high,int decimals){
        auto *editor=new QDoubleSpinBox;
        editor->setRange(low,high); editor->setDecimals(decimals); editor->setValue(value);
        m_parameterEditors.push_back({&value,editor}); form->addRow(name,editor);
    };
    add(tr("Рабочее Z, мм"),m_parameters.workingDistance,1,10000,1);
    add(tr("Допуск Z, мм"),m_parameters.distanceTolerance,1,10000,1);
    add(tr("Порог сигнала (0–255)"),m_parameters.minSignal,1,254,1);
    add(tr("Минимальное качество"),m_parameters.minQuality,0,1,2);
    add(tr("Ширина min, мм"),m_parameters.minWidth,.01,1000,2);
    add(tr("Ширина max, мм"),m_parameters.maxWidth,.02,1000,2);
    add(tr("Глубина min, мм"),m_parameters.minDepth,.01,1000,2);
    add(tr("Глубина max, мм"),m_parameters.maxDepth,.02,1000,2);
    add(tr("Ошибка аппроксимации, мм"),m_parameters.maxFitError,.01,100,2);
    add(tr("Прогноз, с"),m_parameters.predictionSeconds,0,2,3);
    auto *apply=new QPushButton(tr("Применить параметры"));
    connect(apply,&QPushButton::clicked,this,&MainWindow::applyParameters); form->addRow(apply);
    auto *load=new QPushButton(tr("Загрузить все параметры (включая ROI)…"));
    connect(load,&QPushButton::clicked,this,[this]{
        auto path=QFileDialog::getOpenFileName(this,tr("Параметры"),{},"OpenCV (*.yml *.yaml *.xml)");
        if(path.isEmpty()) return;
        QString error;
        if(!m_parameters.load(path,&error)) {QMessageBox::warning(this,tr("Параметры"),error);return;}
        fillParameters(); m_weildPool->configure(m_parameters,m_weildPool->calibration());
    }); form->addRow(load);
    auto *save=new QPushButton(tr("Сохранить все параметры…"));
    connect(save,&QPushButton::clicked,this,[this]{
        auto path=QFileDialog::getSaveFileName(this,tr("Параметры"),"weild-parameters.yml","OpenCV (*.yml *.xml)");
        if(path.isEmpty()) return;
        QString error;
        if(!m_parameters.save(path,&error)) QMessageBox::warning(this,tr("Параметры"),error);
    }); form->addRow(save);
    auto *scroll=new QScrollArea; scroll->setWidgetResizable(true); scroll->setWidget(panel);
    ui->tabWidget->addTab(scroll,tr("Параметры шва"));
}
void MainWindow::fillParameters()
{
    for(auto editor:m_parameterEditors) editor.second->setValue(*editor.first);
}
void MainWindow::applyParameters()
{
    const auto previous=m_parameters;
    for(auto editor:m_parameterEditors) *editor.first=editor.second->value();
    QString error;
    if(!m_weildPool->configure(m_parameters,m_weildPool->calibration(),&error)) {
        m_parameters=previous; fillParameters(); QMessageBox::warning(this,tr("Параметры"),error);
    } else report(tr("Параметры применены, трекер и буфер траектории сброшены."));
}
void MainWindow::showMeasurement(WeildDetect::Result r)
{
    QString text=tr("%1\nКачество: %2 · обработка: %3 мс\nТочек траектории: %4 · пропущено: %5\nИнтерфейс робота: заглушка")
        .arg(r.diagnostic).arg(r.quality,0,'f',2).arg(r.processingMs,0,'f',1)
        .arg(qulonglong(m_weildPool->path().size())).arg(m_weildPool->droppedFrames());
    if(r.detected || r.predicted) {
        text+=tr("\n%1 X/Y/Z камеры: %2 / %3 / %4 мм\nШирина: %5 мм · глубина: %6 мм · угол: %7°")
            .arg(r.predicted ? tr("ПРОГНОЗ") : tr("Измерение (фильтр)"))
            .arg(r.trackedCamera.x,0,'f',2).arg(r.trackedCamera.y,0,'f',2).arg(r.trackedCamera.z,0,'f',2)
            .arg(r.width,0,'f',2).arg(r.depth,0,'f',2).arg(r.angle,0,'f',1);
        if(r.robotValid) text+=tr("\nX/Y/Z робота: %1 / %2 / %3 мм").arg(r.centerRobot.x,0,'f',2)
            .arg(r.centerRobot.y,0,'f',2).arg(r.centerRobot.z,0,'f',2);
    }
    m_measurement->setText(text);
}
