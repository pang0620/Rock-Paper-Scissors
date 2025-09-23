#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QGraphicsScene>
#include <opencv2/opencv.hpp>
#include <QProcess>
#include <QDebug>
#include <QTcpSocket> // Added for QTcpSocket
#include <QFile>
#include <QDir>

QT_BEGIN_NAMESPACE
namespace Ui { class mainwidget; }
QT_END_NAMESPACE

class mainwidget : public QWidget
{
    Q_OBJECT

public:
    explicit mainwidget(QWidget *parent = nullptr);
    ~mainwidget();
    void setDetectionUrl(const QString& url); // Already existed

private slots:
    void updateFrame1();
    void updateFrame2();

    void on_pushButton_clicked();	// detection process 실행 버튼
    void on_pushButton_2_clicked();   // Camera Open 버튼

    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

    // --- Slots for QTcpSocket --- //
    void onConnected();
    void onDisconnected();
    void onReadyReadSocket();
    void onErrorOccurred(QAbstractSocket::SocketError socketError);

    // --- Methods for Server Communication --- //
    void connectToServer();
    void sendHandToServer(const QString& hand);

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

    // --- Server Communication --- //
    QTcpSocket *m_socket;
    QString m_serverIp; // Hardcoded for now
    quint16 m_serverPort; // Hardcoded for now
    QString m_userId; // Hardcoded for now
    QString m_userPw; // Hardcoded for now

    int m_currentRound; // Current game round from server
    bool m_isReadyForHand; // Flag: true when START_ROUND is received
    QString m_lastDetectedGesture; // Holds the last detected gesture

    bool cameraOpened = false;
};

#endif // MAINWIDGET_H
