#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QByteArray>
#include <QStringList>
#include <QThread>
#include <asio.hpp>
#include "CodecManager.h"

class NetworkReceiver : public QObject
{
    Q_OBJECT

public:
    NetworkReceiver(asio::io_context& ioContext, const QString& listenAddress, int port);
    ~NetworkReceiver();

    void start();
    void stop();

signals:
    void dataReceived(const QByteArray& data);

private:
    void startReceive();
    void handleReceive(const asio::error_code& error, std::size_t bytesReceived);

    asio::ip::udp::socket socket;
    asio::ip::udp::endpoint senderEndpoint;
    enum { maxPacketSize = 4096 };
    char recvBuffer[maxPacketSize];
    bool running;
};

class NetworkSender : public QObject
{
    Q_OBJECT

public:
    NetworkSender(asio::io_context& ioContext, const QString& broadcastAddress, int port);
    ~NetworkSender();

    bool initialize();

public slots:
    void sendData(const QByteArray& data);

private:
    asio::ip::udp::socket socket;
    asio::ip::udp::endpoint endpoint;
    bool initialized;
};

class NetworkManager : public QObject
{
    Q_OBJECT

public:
    explicit NetworkManager(CodecManager *codecManager, QObject *parent = nullptr);
    ~NetworkManager();

    bool initialize(const QString& broadcastAddress, const QStringList& listenAddresses, int port);
    bool startNetwork();
    void stopNetwork();

    static bool isValidMulticastAddress(const QString& address);

signals:
    void audioDataReceived(const QByteArray& encodedData);

public slots:
    void sendAudioData(const QByteArray& encodedData);
    void handleReceivedData(const QByteArray& data);

private:
    class NetworkThread : public QThread
    {
    public:
        NetworkThread(asio::io_context& ioContext);
        void run() override;

        asio::io_context& ioContext;
    };

    CodecManager *codecManager;
    asio::io_context ioContext;
    NetworkThread *networkThread;
    NetworkSender *sender;
    QList<NetworkReceiver*> receivers;
    bool initialized;
    bool running;
};

#endif // NETWORKMANAGER_H
