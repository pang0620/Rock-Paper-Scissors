#include "mainwidget.h"
#include "ui_mainwidget.h"
#include <QImage>
#include <QPixmap>
#include <QProcess>
#include <QDebug>
#include <QTcpSocket>
#include <QHostAddress>
#include <QDir>
#include <cstdlib>
#include <QMessageBox>
#include <QFile>

mainwidget::mainwidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::mainwidget)
{
    ui->setupUi(this);

    // Set default detection URL
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

    // --- Server Communication Initialization --- //
    m_socket = new QTcpSocket(this);
    // Hardcoded server IP and Port for now - TODO: Make configurable
    m_serverIp = "127.0.0.1"; // TODO: Make configurable
    m_serverPort = 5000;      // TODO: Make configurable
    // Hardcoded user credentials for now - TODO: Make configurable
    m_userId = "1";          // TODO: Make configurable
    m_userPw = "PASSWD";       // TODO: Make configurable

    m_currentRound = 0;
    m_isReadyForHand = false;

    connect(m_socket, &QTcpSocket::connected, this, &mainwidget::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &mainwidget::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &mainwidget::onReadyReadSocket);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &mainwidget::onErrorOccurred);

    // Initially hide opponent's view
    ui->graphicsView_2->setVisible(false);

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

    
        // Ensure socket is properly closed on destruction
        if (m_socket->isOpen()) {
            m_socket->disconnectFromHost();
            m_socket->waitForDisconnected();
        }
        delete m_socket;
    }
void mainwidget::setDetectionUrl(const QString& url)
{
    m_detectionUrl = url;
    qDebug() << "Detection URL set to:" << m_detectionUrl;
}

// --- Server Communication Methods --- //
void mainwidget::connectToServer()
{
    qDebug() << "Attempting to connect to server at" << m_serverIp << ":" << m_serverPort;
    m_socket->connectToHost(m_serverIp, m_serverPort);
}

void mainwidget::sendHandToServer(const QString& hand)
{
    if (m_socket->state() == QAbstractSocket::ConnectedState && m_isReadyForHand && m_currentRound > 0) {
        QString message = QString("HAND:%1:%2\n").arg(hand.toLower()).arg(m_currentRound);
        qDebug() << "Sending to server:" << message.trimmed();
        m_socket->write(message.toUtf8());
        m_isReadyForHand = false; // Hand sent for this round, stop retrying
        ui->pushButton->setEnabled(false); // Disable ready button after sending
    } else {
        qDebug() << "Not ready to send hand to server. State:" << m_socket->state()
                 << ", Ready for hand:" << m_isReadyForHand
                 << ", Current Round:" << m_currentRound;
    }
}

// --- QTcpSocket Slots --- //
void mainwidget::onConnected()
{
    qDebug() << "Connected to server!";
    // Ensure the connect button is disabled now that we are connected.
    ui->pushButton_2->setEnabled(false);

    // Send authentication immediately after connecting
    QString auth = QString("%1:%2\n").arg(m_userId).arg(m_userPw);
    m_socket->write(auth.toUtf8());
    qDebug() << "Sent authentication:" << auth.trimmed();
}

void mainwidget::onDisconnected()
{
    qDebug() << "Disconnected from server.";
    m_currentRound = 0;
    m_isReadyForHand = false;
    ui->graphicsView_2->setVisible(false); // Hide opponent's view

    // Reset button states for a new connection attempt
    ui->pushButton_2->setEnabled(true); // Re-enable connect button
    ui->pushButton->setEnabled(false);  // Disable ready button
}

void mainwidget::onReadyReadSocket()
{
    QByteArray data = m_socket->readAll();
    QString response = QString::fromUtf8(data).trimmed();
    qDebug() << "Received from server:" << response;

    // Simple parsing for server messages
    if (response.startsWith("WELCOME:")) {
        qDebug() << "Server Welcome:" << response;
    } else if (response == "WAIT") {
        qDebug() << "Server Status: Waiting for opponent...";
    } else if (response.startsWith("START_ROUND:")) {
        m_currentRound = response.section(':', 1, 1).toInt();
        m_isReadyForHand = true;
        qDebug() << "Round" << m_currentRound << "started! Starting client process for detection.";
        ui->graphicsView_2->setVisible(true); // Show opponent's view (or placeholder)
        ui->pushButton->setEnabled(true); // Enable the ready button for the user

        // --- Start client/main for detection --- //
        if (m_process->state() == QProcess::Running) {
            m_process->kill();
            m_process->waitForFinished();
        }
        m_lastDetectedGesture.clear(); // Clear previous gesture before new detection
        m_process->setWorkingDirectory("../../client");
        m_process->setProgram("./main");
        QStringList args;
        args << m_detectionUrl;
        m_process->setArguments(args);
        m_process->start();

    } else if (response.startsWith("RESULT:")) {
        qDebug() << "Game Result:" << response;
        // Hide opponent's view until next round starts
        ui->graphicsView_2->setVisible(false);

    } else if (response.startsWith("ERROR:")) {
        qDebug() << "Server Error:" << response;
    } else if (response == "OPPONENT_LEFT") {
        qDebug() << "Opponent left the game.";
        onDisconnected(); // Treat as a disconnect
    }
}

void mainwidget::onErrorOccurred(QAbstractSocket::SocketError socketError)
{
    qDebug() << "Socket Error:" << socketError << m_socket->errorString();
    // Also treat connection refused as a disconnect scenario to re-enable button
    if (socketError == QAbstractSocket::ConnectionRefusedError) {
        onDisconnected();
    }
}

// --- UI Button Slots --- //
void mainwidget::on_pushButton_clicked()
{
    qDebug() << "'Ready' button clicked. Sending gesture:" << m_lastDetectedGesture;

    // Only send if the gesture is valid
    if (m_lastDetectedGesture != "Nothing" && m_lastDetectedGesture != "Unknown" && !m_lastDetectedGesture.isEmpty()) {
        sendHandToServer(m_lastDetectedGesture);
    } else {
        qDebug() << "No valid gesture to send. Show a valid hand to the camera.";
        // Optionally, provide feedback to the user via a status bar or label
    }
}

void mainwidget::on_pushButton_2_clicked()
{
    qDebug() << "'Camera Open' button clicked. Opening camera and connecting to server.";
    // This button now handles both camera and connection.
    connectToServer();
    ui->pushButton_2->setEnabled(false); // Disable button to prevent re-clicks

    if (!cameraOpened) {
        // 두 개의 카메라 스트리밍 URL
        cap1.open("http://127.0.0.1:8081/?action=stream");   // 첫 번째 카메라

        if (!cap1.isOpened()) {
            QMessageBox::critical(this, "카메라 오류", "카메라 스트림을 열 수 없습니다.\n mjpg-streamer가 8081 포트에서 실행 중인지 확인하세요.");
            exit(1);
        }

        //cap1.open(0);
        cap2.open("http://10.10.16.36:8081/?action=stream");    // 두 번째 카메라 (IP 수정)

        timer1.start(30);
        if (cap2.isOpened()) timer2.start(30);

        cameraOpened = true;
    }
}

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
    cv::Mat frame;
    if (cap2.read(frame)) {
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
    qDebug() << "Client stdout (raw):" << output;

    // Expected format: "C++ | Detected Gesture: [GestureName]"
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

    // Only retry if we are in an active round AND the last gesture was not valid.
    if (m_isReadyForHand && !isValidGesture) {
        qDebug() << "Client process finished with an invalid gesture. Retrying...";
        if (m_process->state() == QProcess::Running) {
            m_process->kill();
            m_process->waitForFinished();
        }
        m_lastDetectedGesture.clear(); // Clear previous gesture before retrying
        m_process->setWorkingDirectory("../../client");
        m_process->setProgram("./main");
        QStringList args;
        args << m_detectionUrl;
        m_process->setArguments(args);
        m_process->start();
    } else if (m_isReadyForHand && isValidGesture) {
        qDebug() << "Client process finished with a valid gesture (" << m_lastDetectedGesture << "). Waiting for user to click 'Ready'.";
        // Do nothing, just wait for the user to press the ready button.
    }
}
