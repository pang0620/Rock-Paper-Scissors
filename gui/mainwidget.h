#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QGraphicsScene>
#include <opencv2/opencv.hpp>

QT_BEGIN_NAMESPACE
namespace Ui { class mainwidget; }
QT_END_NAMESPACE

class mainwidget : public QWidget
{
    Q_OBJECT

public:
    explicit mainwidget(QWidget *parent = nullptr);
    ~mainwidget();

private slots:
    void on_pushButton_2_clicked();   // Camera Open 버튼
    void updateFrame1();
    void updateFrame2();

private:
    Ui::mainwidget *ui;

    cv::VideoCapture cap1;
    cv::VideoCapture cap2;
    QTimer timer1;
    QTimer timer2;

    QGraphicsScene *scene1;
    QGraphicsScene *scene2;

    bool cameraOpened = false;
};

#endif // MAINWIDGET_H
