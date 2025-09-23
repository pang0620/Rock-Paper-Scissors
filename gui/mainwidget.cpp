#include "mainwidget.h"
#include "ui_mainwidget.h"
#include <QImage>
#include <QPixmap>
#include <QDebug>
#include <QMessageBox>
#include <QTimer>
#include <QGraphicsRectItem>

mainwidget::mainwidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::mainwidget)
{
    ui->setupUi(this);

    m_detectionUrl = "http://127.0.0.1:8081/?action=stream";

    m_serverIp = "10.10.16.37";
    m_serverPort = 5000;
    m_userId = "37";
    m_userPw = "PASSWD";

    scene1 = new QGraphicsScene(this);
    scene2 = new QGraphicsScene(this);

    ui->graphicsView->setScene(scene1);
    ui->graphicsView_2->setScene(scene2);

    m_process = new QProcess(this);
    m_networkManager = new NetworkManager(this);

    connect(&timer1, &QTimer::timeout, this, &mainwidget::updateFrame1);
    connect(&timer2, &QTimer::timeout, this, &mainwidget::updateFrame2);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &mainwidget::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &mainwidget::onReadyReadStandardError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &mainwidget::onProcessFinished);

    // NetworkManager signals
    connect(m_networkManager, &NetworkManager::connected, this, &mainwidget::onServerConnected);
    connect(m_networkManager, &NetworkManager::disconnected, this, &mainwidget::onServerDisconnected);
    connect(m_networkManager, &NetworkManager::roundStarted, this, &mainwidget::onRoundStarted);
    connect(m_networkManager, &NetworkManager::gameResultReceived, this, &mainwidget::onGameResultReceived);
    connect(m_networkManager, &NetworkManager::serverError, this, &mainwidget::onServerError);
    connect(m_networkManager, &NetworkManager::opponentLeft, this, &mainwidget::onOpponentLeft);
    connect(m_networkManager, &NetworkManager::opponentReady, this, &mainwidget::onOpponentReady);

    ui->pushButton->setEnabled(false);  // Ready button initially
    ui->pushButton_2->setEnabled(true); // Camera Open initially
}

mainwidget::~mainwidget()
{
    if (cap1.isOpened()) cap1.release();
    if (cap2.isOpened()) cap2.release();

    terminateProcessIfRunning();
    delete ui;
}

void mainwidget::setDetectionUrl(const QString &url)
{
    m_detectionUrl = url;
    qDebug() << "Detection URL set to:" << m_detectionUrl;
}

// --- UI Button Slots --- //
void mainwidget::on_pushButton_clicked()
{
    ui->textEdit->append("'Ready' button clicked.");

    if (!m_lastDetectedGesture.isEmpty() && m_lastDetectedGesture != "Nothing" && m_lastDetectedGesture != "Unknown") {
        m_networkManager->sendHandToServer(m_lastDetectedGesture);
        ui->pushButton->setEnabled(false);
        ui->textEdit->append(QString("You played: %1").arg(m_lastDetectedGesture));
    } else {
        ui->textEdit->append("No valid gesture to send. Show a valid hand to the camera.");
    }
}

void mainwidget::on_pushButton_2_clicked()
{
    if (!cameraOpened) {
        startConnectionSequence();
        ui->pushButton_2->setText("Camera Close");
    } else {
        timer1.stop();
        timer2.stop();
        if (cap1.isOpened()) cap1.release();
        if (cap2.isOpened()) cap2.release();
        scene1->clear();
        scene2->clear();
        m_networkManager->disconnectFromServer();

        cameraOpened = false;
        ui->pushButton_2->setText("Camera Open");
    }
}

// --- Network Slots --- //
void mainwidget::onServerConnected()
{
    ui->textEdit->append("Connected to server!");
    ui->pushButton_2->setEnabled(false);

    QString auth = QString("%1:%2\n").arg(m_userId).arg(m_userPw);
    m_networkManager->sendAuthentication(auth);
    qDebug() << "Sent authentication:" << auth.trimmed();
}

void mainwidget::onServerDisconnected()
{
    ui->textEdit->append("Disconnected from server.");
    m_isReadyForHand = false;
    m_isOpponentStreamVisible = false;
    scene2->clear();

    ui->pushButton_2->setEnabled(true);
    ui->pushButton->setEnabled(false);

    if (m_reconnecting) {
        m_reconnecting = false;
        startConnectionSequence();
    }
}

void mainwidget::onOpponentLeft()
{
    ui->textEdit->append("Opponent left. Auto-reconnect.");
    m_reconnecting = true;
    m_isOpponentStreamVisible = false;
    m_isReadyForHand = false;
    scene2->clear();
    terminateProcessIfRunning();
    m_networkManager->disconnectFromServer();
}

void mainwidget::onOpponentReady(const QString &opponentId)
{
    qDebug() << "Opponent" << opponentId << "is ready.";
    ui->textEdit->append("Opponent is ready.");

    m_isOpponentStreamVisible = true;
}

void mainwidget::onRoundStarted(int roundNumber, int opponentId)
{
    ui->textEdit->append(QString("Round %1 started! Opponent ID: %2").arg(roundNumber).arg(opponentId));
    m_opponentId = opponentId;

    m_isOpponentStreamVisible = false;
    m_isReadyForHand = true;

    cap2.open(QString("http://10.10.16.%1:8081/?action=stream").arg(m_opponentId).toStdString());
    if (cap2.isOpened()) timer2.start(30);

    terminateProcessIfRunning();
    m_lastDetectedGesture.clear();
    m_process->setWorkingDirectory("../../client");
    m_process->setProgram("./main");
    m_process->setArguments({m_detectionUrl});
    m_process->start();

    ui->pushButton->setEnabled(true);
}

void mainwidget::onGameResultReceived(const QString &result)
{
    QStringList parts = result.split(':');
    if (parts.size() >= 7 && parts[0] == "RESULT") {
        int round = parts[1].toInt();
        QString p1_hand = parts[2];
        QString p2_hand = parts[3];
        QString winner = parts[4];
        int score1 = parts[5].toInt();
        int score2 = parts[6].toInt();

        ui->textEdit->append(QString("--- Round %1 Result ---").arg(round));
        ui->textEdit->append(QString("%1 vs %2").arg(p1_hand, p2_hand));
        ui->textEdit->append(QString("Winner: %1").arg(winner));
        ui->textEdit->append(QString("Score: %1 - %2").arg(score1).arg(score2));
        ui->textEdit->append("-----------------------");
    } else {
        ui->textEdit->append(QString("Malformed game result: %1").arg(result));
    }

    // 5초 후 라운드 준비 상태로 복귀
    QTimer::singleShot(5000, [this]() {
        m_isReadyForHand = true;
        ui->pushButton->setEnabled(true);
    });
}

void mainwidget::onServerError(const QString &error)
{
    ui->textEdit->append(QString("Server Error: %1").arg(error));
    if (error.contains("Connection refused")) onServerDisconnected();
}

// --- Camera Update --- //
void mainwidget::updateFrame1()
{
    cv::Mat frame;
    if (cap1.read(frame)) {
        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
        QImage img((uchar*)frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGB888);
        scene1->clear();
        scene1->addPixmap(QPixmap::fromImage(img));
        ui->graphicsView->fitInView(scene1->itemsBoundingRect(), Qt::KeepAspectRatio);
    }
}

void mainwidget::updateFrame2()
{
    scene2->clear();
    QRectF viewRect = ui->graphicsView_2->rect();

    if (!m_isOpponentStreamVisible) {
        // 상대 연결 안됨 → 검은 화면
        scene2->addRect(viewRect, QPen(Qt::NoPen), QBrush(Qt::black));
    }
    else {
        // 영상 공개
        cv::Mat frame;
        if (cap2.isOpened() && cap2.read(frame)) {
            cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
            QImage img((uchar*)frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGB888);
            scene2->addPixmap(QPixmap::fromImage(img));
        }
    }

    // 화면 갱신
    ui->graphicsView_2->fitInView(scene2->itemsBoundingRect(), Qt::KeepAspectRatio);
}

// --- Process Handling --- //
void mainwidget::onReadyReadStandardOutput()
{
    QByteArray data = m_process->readAllStandardOutput();
    QString output = QString::fromUtf8(data).trimmed();
    qDebug() << "Client Stdout:" << output;

    if (output.contains("Detected Gesture: [")) {
        QString gesture = output.section('[', 1, 1).section(']', 0, 0);
        m_lastDetectedGesture = gesture;
        qDebug() << "Parsed gesture:" << m_lastDetectedGesture;
        if (gesture != "Unknown" && gesture != "Nothing" && gesture != "") {
            ui->textEdit->append(QString("Detected hand preview: %1").arg(gesture));
        }
    }
}

void mainwidget::onReadyReadStandardError()
{
    QByteArray err = m_process->readAllStandardError();
    qDebug() << "Client Stderr:" << err.trimmed();
}

void mainwidget::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "Client finished with exit code:" << exitCode;
    bool valid = !m_lastDetectedGesture.isEmpty() &&
                 m_lastDetectedGesture != "Nothing" &&
                 m_lastDetectedGesture != "Unknown";

    if (m_isReadyForHand && !valid) {
        qDebug() << "Invalid gesture, restarting process.";
        terminateProcessIfRunning();
        m_lastDetectedGesture.clear();
        m_process->setWorkingDirectory("../../client");
        m_process->setProgram("./main");
        m_process->setArguments({m_detectionUrl});
        m_process->start();
    }
}

// --- Helper --- //
void mainwidget::startConnectionSequence()
{
    ui->textEdit->append(QString("Connecting to server at %1:%2").arg(m_serverIp).arg(m_serverPort));
    m_networkManager->connectToServer(m_serverIp, m_serverPort, m_userId, m_userPw);

    ui->pushButton_2->setEnabled(false);

    if (!cameraOpened) {
        cap1.open("http://127.0.0.1:8081/?action=stream");

        if (!cap1.isOpened()) {
            QMessageBox::critical(this, "Camera Error",
                                  "Cannot open camera stream.\nPlease check if mjpg-streamer is running on port 8081.");
            exit(1);
        }

        timer1.start(30);
        cameraOpened = true;
    }
}

void mainwidget::terminateProcessIfRunning()
{
    if (m_process && m_process->state() == QProcess::Running) {
        m_process->kill();
        m_process->waitForFinished();
    }
}
