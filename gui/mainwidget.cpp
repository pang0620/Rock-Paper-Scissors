#include "mainwidget.h"
#include "ui_mainwidget.h"
#include <QImage>
#include <QPixmap>
#include <QProcess>
#include <QDebug>

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

void mainwidget::on_pushButton_clicked()
{
    qDebug() << "Starting client process with URL:" << m_detectionUrl;
    if (m_process->state() == QProcess::Running) {
        m_process->kill();
        m_process->waitForFinished();
    }
    m_process->setWorkingDirectory("../../client");
    m_process->setProgram("./main");

    // Pass the video stream URL as an argument
    QStringList args;
    args << m_detectionUrl;
    m_process->setArguments(args);

    m_process->start();
}

void mainwidget::on_pushButton_2_clicked()
{
    if (!cameraOpened) {
        // 두 개의 카메라 스트리밍 URL
        cap1.open("http://10.10.16.36:8081/?action=stream");   // 첫 번째 카메라
        //cap1.open(0);
        cap2.open("http://10.10.16.37:8081/?action=stream");    // 두 번째 카메라 (IP 수정)

        if (cap1.isOpened()) timer1.start(30);
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
        cv::cvtColor(frame, frame, cv::COLOR_BGR2RGB);
        QImage img((const uchar*)frame.data, frame.cols, frame.rows, frame.step, QImage::Format_RGB888);
        scene2->clear();
        scene2->addPixmap(QPixmap::fromImage(img));
        ui->graphicsView_2->fitInView(scene2->itemsBoundingRect(), Qt::KeepAspectRatio);
    }
}

void mainwidget::onReadyReadStandardOutput()
{
    QByteArray data = m_process->readAllStandardOutput();
    //TODO: needs parsing
    qDebug() << "Stdout:" << data.trimmed();
}

void mainwidget::onReadyReadStandardError()
{
    QByteArray errorData = m_process->readAllStandardError();
    qDebug() << "Stderr:" << errorData.trimmed();
}

void mainwidget::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
    qDebug() << "Process finished with exit code " << exitCode;
}
