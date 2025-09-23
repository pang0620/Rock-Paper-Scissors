#include "networkmanager.h"
#include <QDebug>

NetworkManager::NetworkManager(QObject *parent) : QObject(parent), m_socket(new QTcpSocket(this)), m_currentRound(0)
{
    connect(m_socket, &QTcpSocket::connected, this, &NetworkManager::onConnected);
    connect(m_socket, &QTcpSocket::disconnected, this, &NetworkManager::onDisconnected);
    connect(m_socket, &QTcpSocket::readyRead, this, &NetworkManager::onReadyReadSocket);
    connect(m_socket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred), this, &NetworkManager::onErrorOccurred);
}

NetworkManager::~NetworkManager()
{
    if (m_socket->isOpen()) {
        m_socket->disconnectFromHost();
    }
}

void NetworkManager::connectToServer(const QString& ip, quint16 port, const QString& userId, const QString& userPw)
{
    qDebug() << "NetworkManager: Attempting to connect to server at" << ip << ":" << port;
    m_socket->connectToHost(ip, port);
    // Authentication will be sent in onConnected slot
}

void NetworkManager::disconnectFromServer()
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        qDebug() << "NetworkManager: Disconnecting from server...";
        m_socket->disconnectFromHost();
    }
}

void NetworkManager::sendAuthentication(const QString& authString)
{
    if (m_socket->state() == QAbstractSocket::ConnectedState) {
        qDebug() << "NetworkManager: Sending authentication:" << authString.trimmed();
        m_socket->write(authString.toUtf8());
    }
}

void NetworkManager::sendHandToServer(const QString& hand)
{
    if (m_socket->state() == QAbstractSocket::ConnectedState && m_currentRound > 0) {
        QString message = QString("HAND:%1:%2\n").arg(hand.toLower()).arg(m_currentRound);
        qDebug() << "NetworkManager: Sending to server:" << message.trimmed();
        m_socket->write(message.toUtf8());
    } else {
        qDebug() << "NetworkManager: Not ready to send hand to server.";
    }
}

void NetworkManager::onConnected()
{
    qDebug() << "NetworkManager: Connected to server!";
    emit connected();
    // Note: Authentication is now handled by MainWidget in response to the connected() signal
}

void NetworkManager::onDisconnected()
{
    qDebug() << "NetworkManager: Disconnected from server.";
    m_currentRound = 0;
    emit disconnected();
}

void NetworkManager::onReadyReadSocket()
{
    QByteArray data = m_socket->readAll();
    QString dataStr = QString::fromUtf8(data);
    // Split the received data by newline, removing any empty parts.
    QStringList responses = dataStr.split('\n', Qt::SkipEmptyParts);

    // Process each command line by line.
    for (const QString &response : responses) {
        QString trimmedResponse = response.trimmed();
        if (trimmedResponse.isEmpty()) {
            continue;
        }

        qDebug() << "NetworkManager: Processing line:" << trimmedResponse;

        if (trimmedResponse.startsWith("WELCOME:")) {
            emit welcomeMessageReceived(trimmedResponse);
        } else if (trimmedResponse == "WAIT") {
            emit waitingForOpponent();
        } else if (trimmedResponse.startsWith("START_ROUND:")) {
            QStringList parts = trimmedResponse.split(':');
            int roundNum = 0;
            int opponentId = 0; // Default to 0 if not provided

            if (parts.size() >= 2) {
                roundNum = parts[1].toInt();
            }
            if (parts.size() >= 3) {
                opponentId = parts[2].toInt();
            }
            m_currentRound = roundNum;
            emit roundStarted(m_currentRound, opponentId);
        } else if (trimmedResponse.startsWith("RESULT:")) {
            emit gameResultReceived(trimmedResponse);
        } else if (trimmedResponse.startsWith("ERROR:")) {
            emit serverError(trimmedResponse);
        } else if (trimmedResponse == "OPPONENT_LEFT") {
            emit opponentLeft();
        }
    }
}

void NetworkManager::onErrorOccurred(QAbstractSocket::SocketError socketError)
{
    qDebug() << "NetworkManager: Socket Error:" << m_socket->errorString();
    emit serverError(m_socket->errorString());
}
