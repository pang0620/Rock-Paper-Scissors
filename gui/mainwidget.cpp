#include "mainwidget.h"
#include "ui_mainwidget.h"
#include <QImage>
#include <QPixmap>
#include <QProcess>
#include <QTcpSocket>
#include <QHostAddress>
#include <QDebug>

mainwidget::mainwidget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::mainwidget)
{
    ui->setupUi(this);

    // 기본 스트리밍 URL
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
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &mainwidget::onProcessFinished);

    // --- 서버 통신 초기화 --- //
    m_socket = new QTcpSocket(this);
    m_serverIp = "10.10.16.37";  // 서버 IP
    m_serverPort = 5000;
    m_userId = "2";
    m_userPw = "PASSWD";

    m_currentRound = 0;
    m_isReadyForHand = false;

    connect(m_socket, &QTcpSocket::connected, this, &mainwidget::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &mainwidget::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &mainwidget::onReadyReadSocket);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &mainwidget::onErrorOccurred);
}

mainwidget::~mainwidget()
{
    if (cap1.isOpened()) cap1.release();
    if (cap2.isOpened()) cap2.release();
    delete ui;

    if (m_process->state() == QProcess::Running) {
        m_process->kill();
        m_process->waitForFinished();
    }

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

// --- 서버 연결 --- //
void mainwidget::connectToServer()
{
    qDebug() << "Attempting to connect to server at" << m_serverIp << ":" << m_serverPort;
    m_socket->connectToHost(m_serverIp, m_serverPort);
}

// --- 서버에 손패 전송 --- //
void mainwidget::sendHandToServer(const QString& hand)
{
    if (m_socket->state() == QAbstractSocket::ConnectedState && m_isReadyForHand && m_currentRound > 0) {
        QString message = QString("HAND:%1:%2\n").arg(hand.toLower()).arg(m_currentRound);
        qDebug() << "Sending to server:" << message.trimmed();
        m_socket->write(message.toUtf8());
        m_isReadyForHand = false;
    } else {
        qDebug() << "Not ready to send hand. State=" << m_socket->state()
                 << " Ready=" << m_isReadyForHand
                 << " Round=" << m_currentRound;
    }
}

// --- QTcpSocket Slots --- //
void mainwidget::onConnected()
{
    // ✅ UI에만 표시
    ui->textEdit->append("Connected to server!");

    QString auth = QString("%1:%2\n").arg(m_userId).arg(m_userPw);
    m_socket->write(auth.toUtf8());
    qDebug() << "Sent authentication:" << auth.trimmed();
}

void mainwidget::onDisconnected()
{
    // ✅ UI에만 표시
    ui->textEdit->append("Disconnected from server.");

    m_currentRound = 0;
    m_isReadyForHand = false;
}

void mainwidget::onReadyReadSocket()
{
    QByteArray data = m_socket->readAll();
    QString response = QString::fromUtf8(data).trimmed();
    qDebug() << "Received from server:" << response;

    if (response.startsWith("WELCOME:")) {
        qDebug() << "Server Welcome:" << response;
    } else if (response == "WAIT") {
        qDebug() << "Server Status: Waiting for opponent...";
    } else if (response.startsWith("START_ROUND:")) {
        m_currentRound = response.section(':', 1, 1).toInt();
        m_isReadyForHand = true;
        qDebug() << "Round" << m_currentRound << "started! Ready for hand detection.";

        if (m_process->state() == QProcess::Running) {
            m_process->kill();
            m_process->waitForFinished();
        }
        m_process->setWorkingDirectory("../client");
        m_process->setProgram("./main");
        QStringList args;
        args << m_detectionUrl;
        m_process->setArguments(args);
        m_process->start();
        qDebug() << "Client process started for detection.";
    } else if (response.startsWith("RESULT:")) {
        qDebug() << "Game Result:" << response;
    } else if (response.startsWith("ERROR:")) {
        qDebug() << "Server Error:" << response;
    } else if (response == "OPPONENT_LEFT") {
        qDebug() << "Opponent left the game.";
    }
}

void mainwidget::onErrorOccurred(QAbstractSocket::SocketError socketError)
{
    qDebug() << "Socket Error:" << socketError << m_socket->errorString();
}

// --- 버튼 슬롯 --- //
void mainwidget::on_pushButton_clicked()
{
    qDebug() << "'Ready' button clicked.";
    connectToServer();
}

void mainwidget::on_pushButton_2_clicked()
{
    qDebug() << "'Camera Open' button clicked.";
    connectToServer();

    if (!cameraOpened) {
        cap1.open("http://10.10.16.36:8081/?action=stream");
        cap2.open("http://10.10.16.37:8081/?action=stream");

        if (cap1.isOpened()) timer1.start(30);
        if (cap2.isOpened()) timer2.start(30);

        cameraOpened = true;
        qDebug() << "Cameras opened.";
        ui->pushButton_2->setText("Camera Close");
    }
    else {
        timer1.stop();
        timer2.stop();
        if (cap1.isOpened()) cap1.release();
        if (cap2.isOpened()) cap2.release();
        scene1->clear();
        scene2->clear();

        if (m_socket->state() == QAbstractSocket::ConnectedState) {
            m_socket->disconnectFromHost();
        }

        cameraOpened = false;
        ui->pushButton_2->setText("Camera Open");
    }
}

// --- 카메라 업데이트 --- //
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
        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
        QImage img((const uchar*)frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGB888);
        scene2->clear();
        scene2->addPixmap(QPixmap::fromImage(img));
        ui->graphicsView_2->fitInView(scene2->itemsBoundingRect(), Qt::KeepAspectRatio);
    }
}

// --- Client 프로세스 출력 처리 --- //
void mainwidget::onReadyReadStandardOutput()
{
    QByteArray data = m_process->readAllStandardOutput();
    QString detectedGesture = QString::fromUtf8(data).trimmed();
    qDebug() << "Detected Gesture:" << detectedGesture;
    sendHandToServer(detectedGesture);
}

void mainwidget::onReadyReadStandardError()
{
    QByteArray errorData = m_process->readAllStandardError();
    qDebug() << "Client Stderr:" << QString::fromUtf8(errorData).trimmed();
}

void mainwidget::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "Client process finished. Exit code=" << exitCode;
}
