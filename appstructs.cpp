#include "appstructs.h"
#include "logger/logger.h"
#include "qdebug.h"

OBJECT_PARAMS::OBJECT_PARAMS()
{
    tubeCorrect = true;
    tubeRadius = 210;
    trjOffsetX = 0;

    actLowDist = 0;
    lowDist = 0;
    actHighDist = 0;
    highDist = 100;
    timeStopWithoutNav = 0;
}
//---------------------------------------------------------------------------
TAPE_POSITION::TAPE_POSITION()
{
    objParams = nullptr;
    tapeDetected = false;
    posDefined = false;

    firstDistMm = 0;
    lastDistMm = 0;
    mat4x4Pos = cv::Mat::eye(4, 4, CV_64F);
    rvec = cv::Mat::eye(3, 1, CV_64F);
    tvec = cv::Mat::eye(3, 1, CV_64F);
    intrinsic_matrix = nullptr;
    distortion_coeffs = nullptr;
    dictionary = nullptr;
    detectorParams = nullptr;
}
//---------------------------------------------------------------------------
bool TAPE_POSITION::tapeDetect()
{
    //список метровых маркеров
    QList<TAPE_MARKER> listMeterMarkers;

    //с этого момента лента и позиция не определены
    //до момента окончания обработки
    tapeDetected = false;
    posDefined = false;

    ids.clear();
    corners.clear(); rejected.clear();
    listCentimeterMarkers.clear();

    //найдем позиции векторов на ленте и отрисуем их
    cv::aruco::Dictionary localDict;
    if (dictionary != nullptr) {
        localDict = *(dictionary->get());
    } else {
        localDict = cv::aruco::getPredefinedDictionary(cv::aruco::DICT_6X6_1000);
    }
    cv::aruco::DetectorParameters localParams;
    if (detectorParams != nullptr) {
        localParams = *(detectorParams->get());
    } else {
        localParams = cv::aruco::DetectorParameters();
    }

    cv::aruco::ArucoDetector detector(localDict, localParams);
    detector.detectMarkers(matBgr, corners, ids, rejected);


    cv::aruco::drawDetectedMarkers(matBgr, corners, ids);
    if(corners.size() == 0) { return false; }

    //создаем списки метровых и сантиметровый маркеров
    for(unsigned int i = 0; i < corners.size(); i++) {
        TAPE_MARKER marker;
        marker.id = ids[i];
        marker.numInCorners = i;
        marker.numCoupleInCorners = -1;
        if(marker.id >= 100) {
            //это метровый маркер
            listMeterMarkers.append(marker);
        } else {
            //это сантиметровый маркер
            listCentimeterMarkers.append(marker);
        }
    }

    //для каждого сантиметрового маркера найдем ему парный метровый маркер
    //по тестовому углу между узловыми векторами маркеров vc3c0 и vc3m3
    //в именах вектроров используется обозначения:
    //с3 - третий узел сантиметрового маркера,
    //с0 - нулевой узел сантиметрового маркера,
    //m3 - третий узел метрового маркера
    //сначала начало, потом конец вектора
    for(int j = 0; j < listCentimeterMarkers.size(); j++) {
        TAPE_MARKER centimeterMarker = listCentimeterMarkers[j];
        QVector2D vc3c0(
                    corners[centimeterMarker.numInCorners][0].x - corners[centimeterMarker.numInCorners][3].x,
                    corners[centimeterMarker.numInCorners][0].y - corners[centimeterMarker.numInCorners][3].y
                );
        vc3c0.normalize();
        for(int i = 0; i < listMeterMarkers.size(); i++) {
            TAPE_MARKER meterMarker = listMeterMarkers[i];
            QVector2D vc3m3(
                        corners[meterMarker.numInCorners][3].x - corners[centimeterMarker.numInCorners][3].x,
                        corners[meterMarker.numInCorners][3].y - corners[centimeterMarker.numInCorners][3].y
                    );
            vc3m3.normalize();
            float cosCouple = QVector2D::dotProduct(vc3c0, vc3m3);
            //if(cosCouple > 1) { cosCouple = 1; }
            //if(cosCouple < -1) { cosCouple = -1; }
            //по величине cosCouple достоверно находится парный маркер
            if(cosCouple >= 0.99) {
                listCentimeterMarkers[j].numCoupleInCorners = listMeterMarkers[i].numInCorners;
                //назначим маркеру дистанцию по такой формуле, всего может быть почти километр, 900 метров
                int tapeDistMm = listCentimeterMarkers[j].id * 10 + (listMeterMarkers[i].id - 100) * 1000;
                listCentimeterMarkers[j].tapeDistMm = tapeDistMm;
                //отрисуем парные маркера
                //fprintf(stdout, "acos = %.5f\n", cosCouple); fflush(stdout);
                float xB = corners[meterMarker.numInCorners][3].x;
                float yB = corners[meterMarker.numInCorners][3].y;
                float xE = corners[centimeterMarker.numInCorners][0].x;
                float yE = corners[centimeterMarker.numInCorners][0].y;
                //используем штатное рисование openCv
                cv::line(matBgr,
                         cv::Point(xB, yB),
                         cv::Point(xE, yE),
                         cv::Scalar(255, 0, 255), 3);
                break;
            }
        }
    }
    //уберем сантиметровые маркера не нашедшие свою метровую пару
    for(int i = listCentimeterMarkers.size() - 1; i >= 0; i--) {
        if(listCentimeterMarkers[i].numCoupleInCorners < 0) {
            listCentimeterMarkers.removeAt(i);
        }
    }

    //обработаем нахлест: сначала найдем минимальную
    //и максимальную дистанцию маркеров
    int minDist = 0, maxDist = 0;
    for(int i = 0; i < listCentimeterMarkers.size(); i++) {
        if(i == 0) {
            minDist = maxDist = listCentimeterMarkers[i].tapeDistMm;
        } else {
            if(listCentimeterMarkers[i].tapeDistMm < minDist) {
                minDist = listCentimeterMarkers[i].tapeDistMm;
            }
            if(listCentimeterMarkers[i].tapeDistMm > maxDist) {
                maxDist = listCentimeterMarkers[i].tapeDistMm;
            }
        }
    }

    //при наличии нахлеста
    if(maxDist - minDist > 200) {
        //посчитаем каких маркеров больше: с минимальными
        //или максимальными значениями маркеров
        int lowDistCounter = 0, bigDistCounter = 0;
        for(int i = 0; i < listCentimeterMarkers.size(); i++) {
            bool lowDist = listCentimeterMarkers[i].tapeDistMm < maxDist - 200;
            if(lowDist) {
                lowDistCounter++;
                listCentimeterMarkers[i].lowDist = true;
            } else {
                bigDistCounter++;
                listCentimeterMarkers[i].lowDist = false;
            }
        }
        //уберем маркера, которых меньше
        if(lowDistCounter >= bigDistCounter) {
            for(int i = listCentimeterMarkers.size() - 1; i >= 0; i--) {
                if(!listCentimeterMarkers[i].lowDist) {
                    listCentimeterMarkers.removeAt(i);
                }
            }
        } else {
            for(int i = listCentimeterMarkers.size() - 1; i >= 0; i--) {
                if(listCentimeterMarkers[i].lowDist) {
                    listCentimeterMarkers.removeAt(i);
                }
            }
        }
    }

    //ограничение на минимальное количество сантиметровых
    if(listCentimeterMarkers.size() < 1) { return false; }
    //std::cout << "pairs: " << listCentimeterMarkers.size() << std::endl;

    //отсортируем список сантиметровых маркеров по дистанции
    for(int j = 0; j < listCentimeterMarkers.size() - 1; j++) {
        for(int i = 0; i < listCentimeterMarkers.size() - 1; i++) {
            if(listCentimeterMarkers[i].tapeDistMm > listCentimeterMarkers[i + 1].tapeDistMm) {
                TAPE_MARKER centimeterMarker = listCentimeterMarkers[i];
                listCentimeterMarkers[i] = listCentimeterMarkers[i + 1];
                listCentimeterMarkers[i + 1] = centimeterMarker;
            }
        }
    }
    //теперь можно определить наименьший/наибольший номер маркера
    firstDistMm = listCentimeterMarkers.first().tapeDistMm;
    lastDistMm = listCentimeterMarkers.last().tapeDistMm;

    //return false;
    return tapeDetected = true;
}
//---------------------------------------------------------------------------
bool TAPE_POSITION::tapeStandPosition()
{
    //определим положение ленты относительно камеры
    //назначаем узлам пиксельные и физические координаты
    std::vector<cv::Point2f> image_points;
    std::vector<cv::Point3f> object_points;

    posDefined = false;

    //Вертикальное расположение ленты:
    //Ось X перпендикулярна ленте направлена вправо (все маркеры имеют отрицательную X координату)
    //Ось Y направлена вниз по правилам openCv, (парралельно ленте) против хода отметок ленты (все маркеры имеют отрицательную Y координату)
    //Ось Z направлена от камеры по правилу правой тройки (в отличие от openGl где Z направлена к камере)
    for(int i = 0; i < listCentimeterMarkers.size(); i++) {
        int numInCorners = listCentimeterMarkers[i].numInCorners;
        for(unsigned int n = 0; n < corners[numInCorners].size(); n++) {
            //пиксельная координата узла
            image_points.push_back(corners[numInCorners][n]);
            //физическая координата узла
            cv::Point3f v;
            if(n == 0) {
                v.x = 0 - 15 + STAND_OFFSET_X;
                v.y = 0 - (listCentimeterMarkers[i].tapeDistMm + 0) + STAND_OFFSET_Y;
                v.z = 0;
                object_points.push_back(v);
                continue;
            }
            if(n == 1) {
                v.x = 0 - 15 + STAND_OFFSET_X;
                v.y = 0 - (listCentimeterMarkers[i].tapeDistMm + 8) + STAND_OFFSET_Y;
                v.z = 0;
                object_points.push_back(v);
                continue;
            }
            if(n == 2) {
                v.x = 0 - 7 + STAND_OFFSET_X;
                v.y = 0 - (listCentimeterMarkers[i].tapeDistMm + 8) + STAND_OFFSET_Y;
                v.z = 0;
                object_points.push_back(v);
                continue;
            }
            if(n == 3) {
                v.x = 0 - 7 + STAND_OFFSET_X;
                v.y = 0 - (listCentimeterMarkers[i].tapeDistMm + 0) + STAND_OFFSET_Y;
                v.z = 0;
                object_points.push_back(v);
                continue;
            }
        }
        numInCorners = listCentimeterMarkers[i].numCoupleInCorners;
        for(unsigned int n = 0; n < corners[numInCorners].size(); n++) {
            //пиксельная координата узла
            image_points.push_back(corners[numInCorners][n]);
            //физическая координата узла
            cv::Point3f v;
            if(n == 0) {
                v.x = 0 - 25 + STAND_OFFSET_X;
                v.y = 0 - (listCentimeterMarkers[i].tapeDistMm + 0) + STAND_OFFSET_Y;
                v.z = 0;
                object_points.push_back(v);
                continue;
            }
            if(n == 1) {
                v.x = 0 - 25 + STAND_OFFSET_X;
                v.y = 0 - (listCentimeterMarkers[i].tapeDistMm + 8) + STAND_OFFSET_Y;
                v.z = 0;
                object_points.push_back(v);
                continue;
            }
            if(n == 2) {
                v.x = 0 - 17 + STAND_OFFSET_X;
                v.y = 0 - (listCentimeterMarkers[i].tapeDistMm + 8) + STAND_OFFSET_Y;
                v.z = 0;
                object_points.push_back(v);
                continue;
            }
            if(n == 3) {
                v.x = 0 - 17 + STAND_OFFSET_X;
                v.y = 0 - (listCentimeterMarkers[i].tapeDistMm + 0) + STAND_OFFSET_Y;
                v.z = 0;
                object_points.push_back(v);
                continue;
            }
        }
    }
    //находим позицию ленты
    bool m_positionCorrect = cv::solvePnP(
                object_points,
                image_points,
                *intrinsic_matrix,
                *distortion_coeffs,
                rvec, tvec,
                false
                );

    if(!m_positionCorrect) { return false; }

    //находим матрицу позиции
    mposFromRTvec(&rvec, &tvec, &mat4x4Pos);

    return posDefined = true;
}
//---------------------------------------------------------------------------
bool TAPE_POSITION::tapeWorkPosition()
{
    //определим положение ленты относительно камеры
    //назначаем узлам пиксельные и физические координаты
    std::vector<cv::Point2f> image_points;
    std::vector<cv::Point3f> object_points;

    posDefined = false;

    //Вертикальное расположение ленты:
    //Ось X перпендикулярна ленте направлена вправо (все маркеры имеют отрицательную X координату)
    //Ось Y направлена вниз по правилам openCv, против хода отметок ленты (все маркеры имеют отрицательную Y координату)
    //Ось Z направлена от камеры по правилу правой тройки (в отличие от openGl где Z направлена к камере)
    for(int i = 0; i < listCentimeterMarkers.size(); i++) {
        int numInCorners = listCentimeterMarkers[i].numInCorners;
        for(unsigned int n = 0; n < corners[numInCorners].size(); n++) {
            //пиксельная координата узла
            image_points.push_back(corners[numInCorners][n]);
            //физическая координата узла
            cv::Point3f v;
            if(n == 0) {
                v.x = 0 - 15 + STAND_OFFSET_X;
                v.y = 0 - (listCentimeterMarkers[i].tapeDistMm + 0 - firstDistMm) + STAND_OFFSET_Y;
                v.z = 0;
                if(objParams->tubeCorrect) {
                    v = tubeCoordCorrect(v);
                }
                object_points.push_back(v);
                continue;
            }
            if(n == 1) {
                v.x = 0 - 15 + STAND_OFFSET_X;
                v.y = 0 - (listCentimeterMarkers[i].tapeDistMm + 8 - firstDistMm) + STAND_OFFSET_Y;
                v.z = 0;
                if(objParams->tubeCorrect) {
                    v = tubeCoordCorrect(v);
                }
                object_points.push_back(v);
                continue;
            }
            if(n == 2) {
                v.x = 0 - 7 + STAND_OFFSET_X;
                v.y = 0 - (listCentimeterMarkers[i].tapeDistMm + 8 - firstDistMm) + STAND_OFFSET_Y;
                v.z = 0;
                if(objParams->tubeCorrect) {
                    v = tubeCoordCorrect(v);
                }
                object_points.push_back(v);
                continue;
            }
            if(n == 3) {
                v.x = 0 - 7 + STAND_OFFSET_X;
                v.y = 0 - (listCentimeterMarkers[i].tapeDistMm + 0 - firstDistMm) + STAND_OFFSET_Y;
                v.z = 0;
                if(objParams->tubeCorrect) {
                    v = tubeCoordCorrect(v);
                }
                object_points.push_back(v);
                continue;
            }
        }
        numInCorners = listCentimeterMarkers[i].numCoupleInCorners;
        for(unsigned int n = 0; n < corners[numInCorners].size(); n++) {
            //пиксельная координата узла
            image_points.push_back(corners[numInCorners][n]);
            //физическая координата узла
            cv::Point3f v;
            if(n == 0) {
                v.x = 0 - 25 + STAND_OFFSET_X;
                v.y = 0 - (listCentimeterMarkers[i].tapeDistMm + 0 - firstDistMm) + STAND_OFFSET_Y;
                v.z = 0;
                if(objParams->tubeCorrect) {
                    v = tubeCoordCorrect(v);
                }
                object_points.push_back(v);
                continue;
            }
            if(n == 1) {
                v.x = 0 - 25 + STAND_OFFSET_X;
                v.y = 0 - (listCentimeterMarkers[i].tapeDistMm + 8 - firstDistMm) + STAND_OFFSET_Y;
                v.z = 0;
                if(objParams->tubeCorrect) {
                    v = tubeCoordCorrect(v);
                }
                object_points.push_back(v);
                continue;
            }
            if(n == 2) {
                v.x = 0 - 17 + STAND_OFFSET_X;
                v.y = 0 - (listCentimeterMarkers[i].tapeDistMm + 8 - firstDistMm) + STAND_OFFSET_Y;
                v.z = 0;
                if(objParams->tubeCorrect) {
                    v = tubeCoordCorrect(v);
                }
                object_points.push_back(v);
                continue;
            }
            if(n == 3) {
                v.x = 0 - 17 + STAND_OFFSET_X;
                v.y = 0 - (listCentimeterMarkers[i].tapeDistMm + 0 - firstDistMm) + STAND_OFFSET_Y;
                v.z = 0;
                if(objParams->tubeCorrect) {
                    v = tubeCoordCorrect(v);
                }
                object_points.push_back(v);
                continue;
            }
        }
    }


    //находим позицию ленты
    bool m_positionCorrect = cv::solvePnP(
                object_points,
                image_points,
                *intrinsic_matrix,
                *distortion_coeffs,
                rvec, tvec,
                false
                );

    if(!m_positionCorrect) { return false; }

    //находим матрицу позиции
    mposFromRTvec(&rvec, &tvec, &mat4x4Pos);

    return posDefined = true;
}
//---------------------------------------------------------------------------
cv::Point3f TAPE_POSITION::tubeCoordCorrect(cv::Point3f v_in)
{
    cv::Point3f v_out = v_in;

    float ang = v_in.y / objParams->tubeRadius;
    v_out.y = objParams->tubeRadius * sin(ang);
    v_out.z = objParams->tubeRadius * (1 - cos(ang));

    return v_out;
}
//---------------------------------------------------------------------------
void TAPE_POSITION::mposFromRTvec(cv::Mat *rvec, cv::Mat *tvec, cv::Mat *mpos)
{
    //находим матрицу поворота для rvec
    cv::Mat mrot = cv::Mat::eye(3, 3, CV_64F);
    cv::Rodrigues(*rvec, mrot);

    //копируем соотвестсвующие элементы в матрицу
    mpos->at<double>(0, 0) = mrot.at<double>(0, 0);
    mpos->at<double>(0, 1) = mrot.at<double>(0, 1);
    mpos->at<double>(0, 2) = mrot.at<double>(0, 2);
    mpos->at<double>(0, 3) = tvec->at<double>(0, 0);
    //
    mpos->at<double>(1, 0) = mrot.at<double>(1, 0);
    mpos->at<double>(1, 1) = mrot.at<double>(1, 1);
    mpos->at<double>(1, 2) = mrot.at<double>(1, 2);
    mpos->at<double>(1, 3) = tvec->at<double>(0, 1);
    //
    mpos->at<double>(2, 0) = mrot.at<double>(2, 0);
    mpos->at<double>(2, 1) = mrot.at<double>(2, 1);
    mpos->at<double>(2, 2) = mrot.at<double>(2, 2);
    mpos->at<double>(2, 3) = tvec->at<double>(0, 2);

    //последняя строка есть нули и единица
    mpos->at<double>(3, 0) = 0.0;
    mpos->at<double>(3, 1) = 0.0;
    mpos->at<double>(3, 2) = 0.0;
    mpos->at<double>(3, 3) = 1.0;
}
//---------------------------------------------------------------------------
//TAPE_ZERO_POS////////////////////////////////
//---------------------------------------------------------------------------
TAPE_ZERO_POS::TAPE_ZERO_POS()
{
    mat4x4ZeroPosInv = cv::Mat::eye(4, 4, CV_64F);
    firstDistMm = 0;
}
//---------------------------------------------------------------------------
//SCANNER_POS//////////////////////////////////
//---------------------------------------------------------------------------
SCANNER_POS::SCANNER_POS()
{
    posDefined = false;

    mat4x4Pos = cv::Mat::eye(4, 4, CV_64F);
    rvec = cv::Mat::eye(3, 1, CV_64F);
    tvec = cv::Mat::eye(3, 1, CV_64F);
    firstDistMm = 0; lastDistMm = 0;
    courseRad = courseDeg = 0;
    pathOffset = 0;
    pathDist = 0;

}
//---------------------------------------------------------------------------
//переводим координаты точки из базиса ijk в базис сканера
//---------------------------------------------------------------------------
QVector3D SCANNER_POS::transformPos(double xS, double yS, double zS)
{
    cv::Mat pos_rel_tape = cv::Mat::eye(4, 1, CV_64F);
    transformPos(xS, yS, zS, &pos_rel_tape);
    //вектор в 3хмерном пространстве
    QVector3D vT(
                0 - pos_rel_tape.at<double>(0, 0),
                STAND_OFFSET_Y + pos_rel_tape.at<double>(1, 0),
                pos_rel_tape.at<double>(2, 0)
                );
    return vT;
}
//---------------------------------------------------------------------------
void SCANNER_POS::transformPos(double xS, double yS, double zS, cv::Mat *pos_rel_tape)
{
    cv::Mat pos_rel_scan = cv::Mat::eye(4, 1, CV_64F);
    pos_rel_scan.at<double>(0, 0) = xS;
    pos_rel_scan.at<double>(1, 0) = yS;
    pos_rel_scan.at<double>(2, 0) = zS;
    pos_rel_scan.at<double>(3, 0) = 1;
    transformPos(&pos_rel_scan, pos_rel_tape);
}
//---------------------------------------------------------------------------
void SCANNER_POS::transformPos(cv::Mat *pos_rel_scan, cv::Mat *pos_rel_tape)
{
    *pos_rel_tape = mat4x4Pos * (*pos_rel_scan);
}
//----------------------------------------------------------------\\-------

