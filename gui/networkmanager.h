#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QTcpSocket>
#include <QString>

class NetworkManager : public QObject
{
    Q_OBJECT
public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();

    void connectToServer(const QString& ip, quint16 port, const QString& userId, const QString& userPw);
    void disconnectFromServer();
    void sendAuthentication(const QString& authString);
    void sendHandToServer(const QString& hand);

signals:
    void connected();
    void disconnected();
    void welcomeMessageReceived(const QString& message);
    void waitingForOpponent();
    void roundStarted(int roundNumber, int opponentId);
    void gameResultReceived(const QString& result);
    void opponentLeft();
    void serverError(const QString& error);

private slots:
    void onConnected();
    void onDisconnected();
    void onReadyReadSocket();
    void onErrorOccurred(QAbstractSocket::SocketError socketError);

private:
    QTcpSocket *m_socket;
    int m_currentRound;
};

#endif // NETWORKMANAGER_H
