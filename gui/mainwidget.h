#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QGraphicsScene>
#include <opencv2/opencv.hpp>
#include <QProcess>
#include <QDebug>

QT_BEGIN_NAMESPACE
namespace Ui { class mainwidget; }
QT_END_NAMESPACE

class mainwidget : public QWidget
{
    Q_OBJECT

public:
    explicit mainwidget(QWidget *parent = nullptr);
    ~mainwidget();
    void setDetectionUrl(const QString& url);

private slots:
    void updateFrame1();
    void updateFrame2();

    void on_pushButton_clicked();	// detection process 실행 버튼
    void on_pushButton_2_clicked();   // Camera Open 버튼

    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    Ui::mainwidget *ui;

    cv::VideoCapture cap1;
    cv::VideoCapture cap2;
    QTimer timer1;
    QTimer timer2;

    QGraphicsScene *scene1;
    QGraphicsScene *scene2;

    QProcess *m_process;
    QString m_detectionUrl;

    bool cameraOpened = false;
};

#endif // MAINWIDGET_H
