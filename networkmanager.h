#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <QObject>
#include <QThread>
#include <QByteArray>
#include <QStringList>
#include <QMap>
#include <memory>
#include <asio.hpp>

// ASIO için header guard ekleyelim
#ifndef ASIO_STANDALONE
#define ASIO_STANDALONE

#endif

class NetworkManager : public QObject
{
    Q_OBJECT
public:
    explicit NetworkManager(QObject *parent = nullptr);
    ~NetworkManager();

    bool initialize();
    void terminate();

    // Multicast ayarları
    bool setBroadcastAddress(const QString &address);
    bool addListenAddress(const QString &address);
    bool removeListenAddress(const QString &address);
    QStringList getListenAddresses() const;
    QString getBroadcastAddress() const;

    // Ağ işlemleri
    bool startNetworking();
    void stopNetworking();

signals:
    void dataReceived(const QByteArray &data, const QString &sourceAddress);
    void networkStarted();
    void networkStopped();
    void networkError(const QString &errorMessage);

public slots:
    void sendData(const QByteArray &data);

private:
    // ASIO bileşenleri
    asio::io_context *ioContext;
    std::unique_ptr<asio::ip::udp::socket> sendSocket;

    // QMap ile unique_ptr yerine, shared_ptr kullanılacak
    QMap<QString, std::shared_ptr<asio::ip::udp::socket>> receiveSocketMap;

    // Veri alma tamponu
    std::shared_ptr<std::vector<char>> receiveBuffer;

    // Multicast adresleri
    QString broadcastAddress;
    QStringList listenAddresses;

    // Sabit port
    static const int MULTICAST_PORT = 5004;

    // ASIO işlemleri için thread
    QThread *networkThread;
    bool isRunning;

    // ASIO async işlemleri için handler fonksiyonlar
    void handleReceive(const asio::error_code &error,
                       std::size_t bytesTransferred,
                       const QString &sourceAddress);

    void startReceive(const QString &address);
};

#endif // NETWORKMANAGER_H
