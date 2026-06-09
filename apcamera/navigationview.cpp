#include "navigationview.h"

NavigationView::NavigationView(QWidget *parent) : QWidget(parent)
{
    m_scannerPos = NULL;
    m_objParams = NULL;
    setMinimumSize(320, 240);
}

double NavigationView::pixX(double physX)
{
    return 0.5 * rect().width() + m_kPix * physX;
}

double NavigationView::pixY(double physY)
{
    return 0.5 * rect().height() - m_kPix * physY;
}


void NavigationView::paintEvent(QPaintEvent */*event*/)
{
    if (m_scannerPos == NULL) return;

    m_kPix = 0.5 * rect().width() / 50;

    QPainter painter(this);
    QFont font20("monospace", 20);
    QFontMetrics fm20(font20); int fh20 = fm20.height(); int fw20 = fm20.horizontalAdvance("123");
    QFont font11("monospace", 11);
    QFontMetrics fm11(font11); int fh11 = fm11.height();
    QString str; //int tpY = fh;

    //нарисуем углы поворота осей
    painter.setPen(QPen(qRgb(196, 0, 0)));
    painter.setFont(font20);
    str =  "Front = " + QString::number(m_scannerPos->frontAxisRad * 180 / M_PI, 'f', 1);
    painter.drawText(rect().width() - fm20.horizontalAdvance(str) - 10, 1 * fh20, str);
    str =  "Back = " + QString::number(m_scannerPos->backAxisRad * 180 / M_PI, 'f', 1);
    painter.drawText(rect().width() - fm20.horizontalAdvance(str) - 10, 1 * fh20, str);


    //рассчитаем положение нулевой точки
    QVector3D vT = m_scannerPos->transformPos(0, 0, 0);//без смещения
    //QVector3D vT = m_scannerPos->transformPos(m_objParams->trjOffsetX, 0, 0);

    //нарисуем систему координат
    painter.drawLine(
                pixX(0.0), 0,
                pixX(0.0), rect().height()
                );
    painter.drawLine(
                0, pixY(0.0),
                rect().width(), pixY(0.0)
                );
    QRect rc; double r = 20;
    rc.setTopLeft(QPoint(pixX(r), pixY(r)));
    rc.setBottomRight(QPoint(pixX(-r), pixY(-r)));
    painter.drawEllipse(rc);

    if (!m_scannerPos->posDefined) return;

    //нарисуем курс
    QPen penCource; penCource.setColor(qRgb(0, 0, 255));
    penCource.setWidth(3);
    painter.setPen(penCource);
    double main = 30; double axis = 20;

    double xMainTopEdge  = (main / 2) * sin(m_scannerPos->courseRad);
    double xMainBotEdge  = - (main / 2) * sin(m_scannerPos->courseRad);
    double yMainTopEdge = -(main / 2) * cos(m_scannerPos->courseRad);
    double yMainBotEdge = (main / 2)* cos(m_scannerPos->courseRad);

    //double front_ang = 20 * 180 / M_PI;
    double xLeftTopEdge  = xMainTopEdge + (axis / 2) * cos(m_scannerPos->courseRad + m_scannerPos->frontAxisRad);
    double xRightTopEdge  = xMainTopEdge - (axis / 2) * cos(m_scannerPos->courseRad + m_scannerPos->frontAxisRad);
    double yLeftTopEdge  = yMainTopEdge + (axis / 2) * sin(m_scannerPos->courseRad + m_scannerPos->frontAxisRad);
    double yRightTopEdge  = yMainTopEdge - (axis / 2) * sin(m_scannerPos->courseRad + m_scannerPos->frontAxisRad);

    double xLeftBotEdge  = xMainBotEdge + (axis / 2) * cos(m_scannerPos->courseRad + m_scannerPos->backAxisRad);
    double xRightBotEdge  = xMainBotEdge - (axis / 2) * cos(m_scannerPos->courseRad + m_scannerPos->backAxisRad);
    double yLeftBotEdge  = yMainBotEdge + (axis / 2) * sin(m_scannerPos->courseRad + m_scannerPos->backAxisRad);
    double yRightBotEdge  = yMainBotEdge - (axis / 2) * sin(m_scannerPos->courseRad + m_scannerPos->backAxisRad);


    //главная ось
    painter.drawLine(
            pixX(xMainTopEdge), pixY(yMainTopEdge),
            pixX(xMainBotEdge), pixY(yMainBotEdge)
         );
    //передняя ось
    painter.drawLine(
                pixX(xLeftTopEdge), pixY(yLeftTopEdge),
                pixX(xRightTopEdge), pixY(yRightTopEdge)
                );

    //заднняя ось
    painter.drawLine(
                pixX(xRightBotEdge), pixY(yRightBotEdge),
                pixX(xLeftBotEdge), pixY(yLeftBotEdge)
                );


    //напишем важный текст
    QPen penForX; penForX.setColor(qRgb(196, 0, 0));
    painter.setPen(penForX); painter.setFont(font20);
    str =  "X = " + QString::number(vT.x(), 'f', 1);
    painter.drawText(0, 1 * fh20, str);

    m_scannerPos->pathDist = vT.y() + m_scannerPos->firstDistMm;
    // Первую строку оставляем как есть, она полностью совместима
    str = "Y = " + QString::number(m_scannerPos->pathDist, 'f', 1);
    painter.drawText(0, 2 * fh20, str);

    // Используем QString::asprintf взамен str.sprintf
    str = QString::asprintf("(%03i-%03i)", m_scannerPos->firstDistMm, m_scannerPos->lastDistMm);
    painter.drawText(0, 3 * fh20, str);

    // Форматирование double с явным указанием знака %+06.1f
    str = QString::asprintf("A = %+06.1f", m_scannerPos->courseDeg);
    // Для drawText с дробными координатами в Qt 6 используем QPointF
    painter.drawText(QPointF(0.3 * rect().width(), 0.5 * rect().height() + 2 * fh20), str);

    // Меняем шрифт и форматируем Z
    painter.setFont(font11);
    str = QString::asprintf("Z = %+06.1f", vT.z());
    painter.drawText(QPointF(0, rect().height() - 0.5 * fh11), str);


    //рисуем матрицу полопжения
    int mt_offset_x = 320;
    int mt_offset_y = 250;

    for (int i =0; i < m_scannerPos->mat4x4Pos.rows; i++)
    {
        for (int j =0; j < m_scannerPos->mat4x4Pos.cols; j++)
        {
            QString val = QString::number(m_scannerPos->mat4x4Pos.at<double>(i,j), 'f', 1);
            painter.drawText(mt_offset_x + j  * fw20,  mt_offset_y + i * fh20, val);
        }
    }


    //рисуем направляющий вектор (единичный векторо парралельный ленте)
    QVector3D v0 = m_scannerPos->transformPos(0, 0, 0);
    QVector3D v1 = m_scannerPos->transformPos(0, 1, 0);

    mt_offset_x = 460;
    mt_offset_y = 240;

    painter.drawText(mt_offset_x  +  fw20,  mt_offset_y + 1 * fh20, "V0.X " + QString::number(v0.x(), 'f',1));
    painter.drawText(mt_offset_x + 3 * fw20,  mt_offset_y + 1 * fh20, "V0.Y " + QString::number(v0.y(), 'f',1));
    painter.drawText(mt_offset_x + 5 * fw20,  mt_offset_y + 1 * fh20, "V0.Z " + QString::number(v0.z(), 'f',1));

    mt_offset_y = 250;
    painter.drawText(mt_offset_x  + fw20,  mt_offset_y + 2 * fh20,  "V1.X " + QString::number(v1.x(), 'f',1));
    painter.drawText(mt_offset_x + 3 * fw20,  mt_offset_y + 2 * fh20, "V1.Y " + QString::number(v1.y(), 'f',1));
    painter.drawText(mt_offset_x + 5 * fw20,  mt_offset_y + 2 * fh20, "V1.Z " + QString::number(v1.z(), 'f',1));
}

