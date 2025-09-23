#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QWidget>
#include <QGraphicsScene>
#include <QTimer>
#include <QProcess>
#include <QTcpSocket>
#include <opencv2/opencv.hpp>

QT_BEGIN_NAMESPACE
namespace Ui { class mainwidget; }
QT_END_NAMESPACE

class mainwidget : public QWidget
{
    Q_OBJECT

public:
    mainwidget(QWidget *parent = nullptr);
    ~mainwidget();

    void setDetectionUrl(const QString& url);

private slots:
    // --- UI 버튼 슬롯 --- //
    void on_pushButton_clicked();   // Ready 버튼
    void on_pushButton_2_clicked(); // Camera Open 버튼

    // --- 카메라 업데이트 --- //
    void updateFrame1();
    void updateFrame2();

    // --- 서버 통신 슬롯 --- //
    void onConnected();
    void onDisconnected();
    void onReadyReadSocket();
    void onErrorOccurred(QAbstractSocket::SocketError socketError);

    // --- Client 프로세스 출력 처리 --- //
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    Ui::mainwidget *ui;

    // --- 카메라 --- //
    QGraphicsScene *scene1;
    QGraphicsScene *scene2;
    cv::VideoCapture cap1;
    cv::VideoCapture cap2;
    QTimer timer1;
    QTimer timer2;
    bool cameraOpened = false;

    // --- Client 프로세스 --- //
    QProcess *m_process;
    QString m_detectionUrl;

    // --- 서버 통신 --- //
    QTcpSocket *m_socket;
    QString m_serverIp;
    int m_serverPort;
    QString m_userId;
    QString m_userPw;

    // --- 게임 상태 --- //
    int m_currentRound;
    bool m_isReadyForHand;

    // --- 내부 함수 --- //
    void connectToServer();
    void sendHandToServer(const QString& hand);
};

#endif // MAINWIDGET_H
