#ifndef NAVIGATIONVIEW_H
#define NAVIGATIONVIEW_H

#include <QWidget>
#include <QPainter>
#include "appstructs.h"

class NavigationView : public QWidget
{
    Q_OBJECT
public:
    explicit NavigationView(QWidget *parent = nullptr);


    //указатель на объект рабочего положения сканера
    SCANNER_POS *m_scannerPos;
    //параметры объекта контрол
    OBJECT_PARAMS *m_objParams;

    // Меняем сигнатуру: принимаем константную ссылку на структуру
    void setScanerPos(SCANNER_POS *pos);


protected:
    double pixX(double physX);
    double pixY(double physY);
    double m_kPix;
    SCANNER_POS m_localCopy;

    virtual void paintEvent(QPaintEvent *event);
};

#endif // NAVIGATIONVIEW_H

