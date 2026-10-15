#pragma once
#include <QWidget>
#include "weilddetect.h"

class WeildProfileView : public QWidget
{
    Q_OBJECT
public:
    explicit WeildProfileView(QWidget *parent=nullptr) : QWidget(parent) {setMinimumSize(300,200);}
public slots:
    void showResult(WeildDetect::Result result) {m_result=std::move(result); update();}
protected:
    void paintEvent(QPaintEvent *) override;
private:
    WeildDetect::Result m_result;
};
