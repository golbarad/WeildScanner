#ifndef APPSTRUCTS_H
#define APPSTRUCTS_H
#include "apcamera/cvmetatypes.h"

#include "windows.h"
#include <QApplication>
#include <QObject>
#include <QtCore>
#include <QVector2D>
#include <QVector3D>

#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>

//смещение системы координат установочного стенда
#define STAND_OFFSET_X          -99.0
#define STAND_OFFSET_Y          100.0

//параметры коррекции трубы по умолчанию
//#define TUBE_CORRECT            true
//#define TUBE_RADIUS             200 //275 //215

#define NO_ACT_AT_DIST          0
#define ACT_AT_DIST_STOP        1
#define ACT_AT_DIST_REVERSE     2

//структура маркера ленты
struct TAPE_MARKER {
    //идентификатор маркера
    int id;
    //номер в исходном списке
    quint32 numInCorners;
    //номер парного маркера в исходном списке
    int numCoupleInCorners;
    //дистанция по двум маркерам по позиционной системе счисления
    //100 * (MARK_CM) + 1000 * (MARK_M - 100)
    int tapeDistMm;
    //каких маркеров больше: с минимальными
    //или максимальными значениями дистанции
    bool lowDist;
};

struct OBJECT_PARAMS {
    OBJECT_PARAMS();

    //корректировать окружность трубы
    bool tubeCorrect;
    //радиус трубы
    double tubeRadius;
    //смещение траектории
    double trjOffsetX;

    //угло отклонениея оси от ленты
    double controlOffsetAngle;

    //действия на нижней отметке
    int actLowDist;
    //нижняя отметка
    float lowDist;
    //действия на верхней отметке
    int actHighDist;
    //верхняя отметка
    float highDist;

    //время до остановки без навигации, ms
    //если == 0, то эта опция не активна
    int timeStopWithoutNav;
};

//структура позиции ленты
struct TAPE_POSITION {
    TAPE_POSITION();

    //указатель на параметры объекта
    OBJECT_PARAMS *objParams;

    //флаги обработки
    bool tapeDetected;
    bool posDefined;

    //обрабатываемая картинка
    cv::Mat matBgr;
    //начальная видимая отметка дистанции ленты
    int firstDistMm;
    //конечная видимая отметка дистанции ленты
    int lastDistMm;
    //вектора местоположения ленты
    cv::Mat rvec, tvec;
    //матрица положения
    cv::Mat mat4x4Pos;

    //указатели на матрицы калибровки камеры
    cv::Mat *intrinsic_matrix, *distortion_coeffs;
    //используется словарь на основе cv::aruco::DICT_6X6_1000
    cv::Ptr<cv::aruco::Dictionary> *dictionary;
    cv::Ptr<cv::aruco::DetectorParameters> *detectorParams;

    //список идентификаторов
    std::vector<int> ids;
    //двумерные координаты маркеров
    std::vector<std::vector<cv::Point2f> > corners, rejected;
    //список сантиметровых маркеров
    QList<TAPE_MARKER> listCentimeterMarkers;

    //найти маркера и сформировать структуру ленты
    bool tapeDetect();
    //найти позицию ленты относительно камеры на стенде
    bool tapeStandPosition();
    //найти и сохранить рабочую позицию
    bool tapeWorkPosition();
    //откорректировать координаты по трубе
    cv::Point3f tubeCoordCorrect(cv::Point3f v_in);

    //матрица положения из векторов положения
    static void mposFromRTvec(cv::Mat *rvec, cv::Mat *tvec, cv::Mat *mpos);
};

//структура нулевой позиции на стенде
struct TAPE_ZERO_POS {
    TAPE_ZERO_POS();

    //матрица привязки (обратная к позиции
    //опеределенной на калибровочном стенде)
    //(при нахождении на нулевой позиции матрица положения сканера
    // (будет единичной)
    cv::Mat mat4x4ZeroPosInv;
    //дистанция первого видимого маркера
    int firstDistMm;
};


//детекотор лци
struct DETECTED_FACE {
   //DETECTED_FACE();

    //флаг провекри лица
    bool checkFace = false;
    // SFace embedding эталонного лица
    cv::Mat originFace;

    // исходный кадр
    cv::Mat matBgr;
    // SFace embeddings найденных лиц
    std::vector<cv::Mat> currentFaces;
    //веткор соответстяи искомому лицу
    std::vector<bool> currentfaceMatch;

    bool faceDetected = false;

    //обьекты детекторов
    cv::Ptr<cv::FaceDetectorYN> detector;
    cv::Ptr<cv::FaceRecognizerSF> recognizer;

    bool faceDetect();
};

//парамтры обнаруженного лица
struct FACE_INFO {
    //FACE_INFO();

    QRectF rect;
    bool match;
    double score;
};


//структура позиции сканера
struct SCANNER_POS {
    SCANNER_POS();

    //позиция определена
    bool posDefined;

    //вектора местоположения ленты
    cv::Mat rvec, tvec;
    //матрица положения
    cv::Mat mat4x4Pos;
    //дистанция первого видимого маркера
    int firstDistMm;
    //дистанция последнего видимого маркера
    int lastDistMm;

    //текущий курс (угол вектора, парраельного физическому положению ленты)
    double courseRad, courseDeg;
    //текущие углы осей сканера
    double frontAxisRad, backAxisRad;

    //усредненное отклонение по оси X
    double pathOffset;
    //текущая дистанция
    double pathDist;

    //функции преобразования позиции
    QVector3D transformPos(double xS, double yS, double zS);
    void transformPos(double xS, double yS, double zS, cv::Mat *pos_rel_tape);
    void transformPos(cv::Mat *pos_rel_scan, cv::Mat *pos_rel_tape);
};

enum STEER_DIRECTION
{
    DIR_FORWARD, DIR_REVERSE
};


//состния процесса руления
enum STEER_STATE
{
    STEER_UNDEF = -1,
    STEER_DIR,          // руление прямо
    STEER_LEFT,        // руление влево
    STEER_RIGHT,       // руление  вправо
    STEER_ZERO,        // руление  в ноль
    STEER_OFFSET_LEFT, // смещение влево
    STEER_OFFSET_RIGHT, // смещение вправо
};




#endif // APPSTRUCTS_H
