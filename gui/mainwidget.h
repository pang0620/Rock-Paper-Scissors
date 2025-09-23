#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QGraphicsScene>
#include <opencv2/opencv.hpp>
#include <QProcess>
#include <QDebug>
#include <QFile>
#include <QDir>
#include "networkmanager.h"
#include <QCoreApplication>
#include <QTextEdit>

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

    // --- Slots for NetworkManager signals --- //
    void onServerConnected();
    void onServerDisconnected();
    void onRoundStarted(int roundNumber, int opponentId);
    void onGameResultReceived(const QString& result);
    void onServerError(const QString& error);
    void onOpponentReady(const QString& opponentId);
    void onOpponentLeft();

private slots:
    void startConnectionSequence(); // New slot for initiating connection

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

    // --- Network & Game State --- //
    NetworkManager *m_networkManager;
    QString m_serverIp;
    quint16 m_serverPort;
    QString m_userId;
    QString m_userPw;

    int m_opponentId; // ID of the opponent
    bool m_isReadyForHand; // Flag: true when START_ROUND is received
    QString m_lastDetectedGesture; // Holds the last detected gesture
    bool m_reconnecting = false; // Flag to indicate automatic reconnection is in progress

    bool cameraOpened = false;
    QPixmap m_placeholderImage; // Placeholder image for opponent's view
};

#endif // MAINWIDGET_H
