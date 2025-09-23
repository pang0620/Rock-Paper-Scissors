#include "mainwidget.h"
#include "ui_mainwidget.h"
#include <QImage>
#include <QPixmap>
#include <QProcess>
#include <QDebug>
#include <QHostAddress>
#include <QDir>
#include <cstdlib>
#include <QMessageBox>
#include <QTextEdit>

mainwidget::mainwidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::mainwidget)
{
    ui->setupUi(this);

    m_detectionUrl = "http://127.0.0.1:8081/?action=stream";

    scene1 = new QGraphicsScene(this);
    scene2 = new QGraphicsScene(this);

    ui->graphicsView->setScene(scene1);
    ui->graphicsView_2->setScene(scene2);

    m_process = new QProcess(this);

    connect(&timer1, &QTimer::timeout, this, &mainwidget::updateFrame1);
    connect(&timer2, &QTimer::timeout, this, &mainwidget::updateFrame2);

    connect(m_process, &QProcess::readyReadStandardOutput, this, &mainwidget::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &mainwidget::onReadyReadStandardError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &mainwidget::onProcessFinished);

    // --- Network & Game State Initialization --- //
    m_networkManager = new NetworkManager(this);
    m_serverIp = "10.10.16.37";
    m_serverPort = 5000;
    m_userId = "37";
    m_userPw = "PASSWD";
    m_isReadyForHand = false;
    m_isOpponentStreamVisible = false;

    connect(m_networkManager, &NetworkManager::connected, this, &mainwidget::onServerConnected);
    connect(m_networkManager, &NetworkManager::disconnected, this, &mainwidget::onServerDisconnected);
    connect(m_networkManager, &NetworkManager::roundStarted, this, &mainwidget::onRoundStarted);
    connect(m_networkManager, &NetworkManager::gameResultReceived, this, &mainwidget::onGameResultReceived);
    connect(m_networkManager, &NetworkManager::serverError, this, &mainwidget::onServerError);
    connect(m_networkManager, &NetworkManager::opponentLeft, this, &mainwidget::onOpponentLeft);
    connect(m_networkManager, &NetworkManager::opponentReady, this, &mainwidget::onOpponentReady);

    // Set initial button states
    ui->pushButton->setEnabled(false); // Disable Ready button initially
    ui->pushButton_2->setEnabled(true);  // Enable Camera Open button initially
}

mainwidget::~mainwidget()
{
    if (cap1.isOpened()) cap1.release();
    if (cap2.isOpened()) cap2.release();
    delete ui;

    if (m_process->state() == QProcess::Running)
    {
        m_process->kill();
        m_process->waitForFinished();
    }
}

void mainwidget::setDetectionUrl(const QString& url)
{
    m_detectionUrl = url;
    qDebug() << "Detection URL set to:" << m_detectionUrl;
}

// --- New Slots for NetworkManager Signals --- //
void mainwidget::onServerConnected()
{
    ui->textEdit->append("Connected to server!");
    ui->pushButton_2->setEnabled(false);

    // Send authentication immediately after connecting
    QString auth = QString("%1:%2\n").arg(m_userId).arg(m_userPw);
    // This is a temporary solution. A proper NetworkManager would handle this.
    m_networkManager->sendAuthentication(auth); // Re-purposing sendHandToServer for auth
    qDebug() << "UI: Sent authentication:" << auth.trimmed();
}

void mainwidget::onServerDisconnected()
{
    ui->textEdit->append("Disconnected from server.");
    m_isReadyForHand = false;
    m_isOpponentStreamVisible = false; // 연결 끊기면 스트림 숨김
    scene2->clear(); // 화면 비우기

    // Reset button states for a new connection attempt
    ui->pushButton_2->setEnabled(true); // Re-enable connect button
    ui->pushButton->setEnabled(false);  // Disable ready button

    if (m_reconnecting) {
        qDebug() << "UI: Auto-reconnecting...";
        m_reconnecting = false; // Reset flag
        startConnectionSequence(); // Re-initiate connection
    }
}

void mainwidget::onOpponentLeft()
{
    ui->textEdit->append("Opponent left the game. Initiating auto-reconnect sequence.");
    m_reconnecting = true; // Set flag for auto-reconnection
    m_isOpponentStreamVisible = false; // 상대방 떠나면 스트림 숨김
    scene2->clear(); // 화면 비우기

    // Kill any running hand detection process
    if (m_process->state() == QProcess::Running) {
        m_process->kill();
        m_process->waitForFinished();
    }

    // Disconnect from the server to trigger onServerDisconnected for reconnection
    m_networkManager->disconnectFromServer();
}

void mainwidget::onOpponentReady(const QString& opponentId)
{
    qDebug() << "UI: Opponent" << opponentId << "is Ready! Showing opponent's camera.";
}

void mainwidget::onRoundStarted(int roundNumber, int opponentId)
{
    ui->textEdit->append(QString("Round %1 started! Opponent ID: %2").arg(roundNumber).arg(opponentId));
    m_opponentId = opponentId;
    qDebug() << "MainWidget: onRoundStarted received opponentId parameter:" << opponentId;
    m_isReadyForHand = true;
    qDebug() << "UI: Round" << roundNumber << "started! Starting client process for detection.";
    ui->pushButton->setEnabled(true);

    // Open cap2 here, as m_opponentId is now known
    qDebug() << "MainWidget: Constructing cap2 URL with m_opponentId:" << m_opponentId;
    //cap2.open(QString("http://10.10.16.%1:8081/?action=stream").arg(m_opponentId).toStdString());
    cap2.open("http://127.0.0.1:8081/?action=stream");
    if (cap2.isOpened()) {
        timer2.start(30);
        m_isOpponentStreamVisible = true;
    } else {
        qDebug() << "UI: Failed to open opponent's camera stream for ID:" << m_opponentId;
        m_isOpponentStreamVisible = false;
        // Optionally, show a QMessageBox or handle this error more gracefully
    }

    if (m_process->state() == QProcess::Running) {
        m_process->kill();
        m_process->waitForFinished();
    }
    m_lastDetectedGesture.clear();
    m_process->setWorkingDirectory("../../client");
    m_process->setProgram("./main");
    QStringList args;
    args << m_detectionUrl;
    m_process->setArguments(args);
    m_process->start();
}

void mainwidget::onGameResultReceived(const QString& result)
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
        ui->textEdit->append(QString("Player 1 Hand: %1, Player 2 Hand: %2").arg(p1_hand).arg(p2_hand));
        ui->textEdit->append(QString("Winner: %1 (Score: %2 - %3)").arg(winner).arg(score1).arg(score2));
        ui->textEdit->append("-----------------------");
    } else {
        ui->textEdit->append(QString("Game Result: %1 (Malformed)").arg(result));
    }
}

void mainwidget::onServerError(const QString& error)
{
    ui->textEdit->append(QString("Server Error: %1").arg(error));
    // For connection refused, also reset the UI
    if (error.contains("Connection refused")) {
        onServerDisconnected();
    }
}

// --- UI Button Slots --- //
void mainwidget::on_pushButton_clicked()
{
    ui->textEdit->append("'Ready' button clicked.");

    if (m_lastDetectedGesture != "Nothing" && m_lastDetectedGesture != "Unknown" && !m_lastDetectedGesture.isEmpty()) {
        m_networkManager->sendHandToServer(m_lastDetectedGesture);
        ui->pushButton->setEnabled(false);
    } else {
        ui->textEdit->append("No valid gesture to send. Show a valid hand to the camera.");
    }
}

void mainwidget::on_pushButton_2_clicked()
{
    ui->textEdit->append("'Camera Open/Close' button clicked.");

    if (!cameraOpened) {
        // Open camera and connect to server
        startConnectionSequence();
        ui->pushButton_2->setText("Camera Close");
    } else {
        // Close camera and disconnect from server
        timer1.stop();
        timer2.stop();
        if (cap1.isOpened()) cap1.release();
        if (cap2.isOpened()) cap2.release();
        scene1->clear();
        scene2->clear();

        m_networkManager->disconnectFromServer(); // Disconnect from server

        cameraOpened = false;
        ui->pushButton_2->setText("Camera Open");
    }
}

void mainwidget::startConnectionSequence()
{
    ui->textEdit->append(QString("Attempting to connect to server at %1:%2").arg(m_serverIp).arg(m_serverPort));
    m_networkManager->connectToServer(m_serverIp, m_serverPort, m_userId, m_userPw);
    ui->pushButton_2->setEnabled(false);

    if (!cameraOpened) {
        cap1.open("http://127.0.0.1:8081/?action=stream");

        if (!cap1.isOpened()) {
            QMessageBox::critical(this, "카메라 오류", "카메라 스트림을 열 수 없습니다.\n mjpg-streamer가 8081 포트에서 실행 중인지 확인하세요.");
            exit(1);
        }

        timer1.start(30);
        // cap2 will be opened in onRoundStarted once opponentId is known

        cameraOpened = true;
    }
}

// --- Other existing slots --- //

void mainwidget::updateFrame1()
{
    cv::Mat frame;
    if (cap1.read(frame)) {
        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
        QImage img((const uchar*)frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGB888);
        scene1->clear();
        scene1->addPixmap(QPixmap::fromImage(img));
        ui->graphicsView->fitInView(scene1->itemsBoundingRect(), Qt::KeepAspectRatio);
    }
}

void mainwidget::updateFrame2()
{
    if (!m_isOpponentStreamVisible) { // 플래그가 false이면 아무것도 표시하지 않음
        scene2->clear(); // 화면 비우기
        return;
    }

    cv::Mat frame;
    if (cap2.isOpened() && cap2.read(frame)) {
        if (ui->graphicsView_2->isVisible()) {
            cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
            QImage img((const uchar*)frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGB888);
            scene2->clear();
            scene2->addPixmap(QPixmap::fromImage(img));
            ui->graphicsView_2->fitInView(scene2->itemsBoundingRect(), Qt::KeepAspectRatio);
        }
    }
}

void mainwidget::onReadyReadStandardOutput()
{
    QByteArray data = m_process->readAllStandardOutput();
    QString output = QString::fromUtf8(data).trimmed();
    qDebug() << "Client Stdout (raw):" << output;

    if (output.contains("Detected Gesture: [")) {
        QString gesture = output.section('[', 1, 1).section(']', 0, 0);
        m_lastDetectedGesture = gesture;
        qDebug() << "Parsed gesture:" << m_lastDetectedGesture;
    }
}


void mainwidget::onReadyReadStandardError()
{
    QByteArray errorData = m_process->readAllStandardError();
    qDebug() << "Client Stderr:" << errorData.trimmed();
}

void mainwidget::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "Client process finished with exit code " << exitCode;

    bool isValidGesture = (m_lastDetectedGesture != "Nothing" && m_lastDetectedGesture != "Unknown" && !m_lastDetectedGesture.isEmpty());

    if (m_isReadyForHand && !isValidGesture) {
        qDebug() << "Client process finished with an invalid gesture. Retrying...";
        if (m_process->state() == QProcess::Running) {
            m_process->kill();
            m_process->waitForFinished();
        }
        m_lastDetectedGesture.clear();
        m_process->setWorkingDirectory("../../client");
        m_process->setProgram("./main");
        QStringList args;
        args << m_detectionUrl;
        m_process->setArguments(args);
        m_process->start();
    } else if (m_isReadyForHand && isValidGesture) {
        qDebug() << "Client process finished with a valid gesture (" << m_lastDetectedGesture << "). Waiting for user to click 'Ready'.";
    }
}
