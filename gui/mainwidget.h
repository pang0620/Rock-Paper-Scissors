#ifndef MAINWIDGET_H
#define MAINWIDGET_H

#include <QWidget>
#include <QTimer>
#include <QProcess>
#include <QGraphicsScene>
#include <QString>
#include <opencv2/opencv.hpp>   // OpenCV 사용 시
#include "networkmanager.h"    // 프로젝트 내 NetworkManager 헤더

namespace Ui {
class mainwidget;
}

class mainwidget : public QWidget
{
    Q_OBJECT

public:
    explicit mainwidget(QWidget *parent = nullptr);
    ~mainwidget();

    void setDetectionUrl(const QString& url);

private slots:
    // UI 버튼 슬롯
    void on_pushButton_clicked();
    void on_pushButton_2_clicked();

    // Network 관련 슬롯
    void onServerConnected();
    void onServerDisconnected();
    void onOpponentLeft();
    void onOpponentReady(const QString& opponentId);
    void onRoundStarted(int roundNumber, int opponentId);
    void onGameResultReceived(const QString& result);
    void onServerError(const QString& error);

    // 카메라 업데이트
    void updateFrame1();
    void updateFrame2();

    // 프로세스 처리
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    // 헬퍼
    void startConnectionSequence();
    void terminateProcessIfRunning();

    Ui::mainwidget *ui;

    // 그래픽 씬
    QGraphicsScene *scene1 = nullptr;
    QGraphicsScene *scene2 = nullptr;

    // OpenCV 캡처
    cv::VideoCapture cap1;
    cv::VideoCapture cap2;

    // 타이머
    QTimer timer1;
    QTimer timer2;

    // 외부 프로세스
    QProcess *m_process = nullptr;

    // 네트워크
    NetworkManager *m_networkManager = nullptr;
    QString m_serverIp;
    int m_serverPort = 0;
    QString m_userId;
    QString m_userPw;

    // 상태 / 설정
    QString m_detectionUrl;
    QString m_lastDetectedGesture;
    bool m_isReadyForHand = false;
    bool m_isOpponentStreamVisible = false;
    bool cameraOpened = false;     // <-- 이 변수가 빠져있으면 "undeclared" 오류 발생
    bool m_reconnecting = false;   // <-- 자동 재접속 플래그

    int m_opponentId = -1;
};

#endif // MAINWIDGET_H
