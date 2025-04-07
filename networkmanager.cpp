#include "networkmanager.h"
#include <QDebug>
#include <QHostAddress>
#include <QNetworkInterface>

// NetworkManager sınıfı implementasyonu
NetworkManager::NetworkManager(QObject *parent)
    : QObject(parent),
    ioContext(new asio::io_context()),
    networkThread(nullptr),
    isRunning(false)
{
    receiveBuffer = std::make_shared<std::vector<char>>(64 * 1024); // 64KB buffer
}

NetworkManager::~NetworkManager()
{
    if (isRunning) {
        stopNetworking();
    }

    if (networkThread) {
        networkThread->quit();
        networkThread->wait();
        delete networkThread;
    }

    delete ioContext;
}

bool NetworkManager::initialize()
{
    // ASIO bileşenlerini başlat
    try {
        // IO context başlat
        ioContext->restart();

        return true;
    }
    catch (const std::exception &e) {
        emit networkError(QString("Ağ başlatılamadı: %1").arg(e.what()));
        return false;
    }
}

void NetworkManager::terminate()
{
    try {
        // ASIO işlemlerini durdur
        ioContext->stop();
    }
    catch (const std::exception &e) {
        qWarning() << "Ağ sonlandırılırken hata:" << e.what();
    }
}

bool NetworkManager::setBroadcastAddress(const QString &address)
{
    if (isRunning) {
        emit networkError("Ağ çalışırken yayın adresi değiştirilemez");
        return false;
    }

    // IP kontrolü yap
    QHostAddress hostAddress(address);
    if (hostAddress.isNull()) {
        emit networkError("Geçersiz IP adresi: " + address);
        return false;
    }

    // Multicast adresi kontrolü
    if (!hostAddress.isMulticast()) {
        emit networkError("Multicast olmayan adres: " + address);
        return false;
    }

    broadcastAddress = address;
    return true;
}

bool NetworkManager::addListenAddress(const QString &address)
{
    if (isRunning) {
        emit networkError("Ağ çalışırken dinleme adresi eklenemez");
        return false;
    }

    // IP kontrolü yap
    QHostAddress hostAddress(address);
    if (hostAddress.isNull()) {
        emit networkError("Geçersiz IP adresi: " + address);
        return false;
    }

    // Multicast adresi kontrolü
    if (!hostAddress.isMulticast()) {
        emit networkError("Multicast olmayan adres: " + address);
        return false;
    }

    // Adres zaten eklenmiş mi kontrol et
    if (listenAddresses.contains(address)) {
        return true; // Zaten eklenmiş
    }

    listenAddresses.append(address);
    return true;
}

bool NetworkManager::removeListenAddress(const QString &address)
{
    if (isRunning) {
        emit networkError("Ağ çalışırken dinleme adresi kaldırılamaz");
        return false;
    }

    return listenAddresses.removeOne(address);
}

QStringList NetworkManager::getListenAddresses() const
{
    return listenAddresses;
}

QString NetworkManager::getBroadcastAddress() const
{
    return broadcastAddress;
}

bool NetworkManager::startNetworking()
{
    if (isRunning) {
        return true; // Zaten çalışıyor
    }

    if (broadcastAddress.isEmpty()) {
        emit networkError("Yayın adresi ayarlanmamış");
        return false;
    }

    if (listenAddresses.isEmpty()) {
        emit networkError("Dinleme adresi eklenmemiş");
        return false;
    }

    try {
        // IO Context'i başlat
        if (!initialize()) {
            return false;
        }

        // Yayın soketi oluştur
        sendSocket = std::make_unique<asio::ip::udp::socket>(*ioContext);
        sendSocket->open(asio::ip::udp::v4());

        // Broadcast seçeneğini aç
        sendSocket->set_option(asio::ip::udp::socket::reuse_address(true));
        sendSocket->set_option(asio::socket_base::broadcast(true));

        // Dinleme soketleri oluştur
        for (const QString &address : listenAddresses) {
            startReceive(address);
        }

        // ASIO işlemlerini başlat (ayrı thread'de)
        std::thread t([this]() {
            try {
                ioContext->run();
            }
            catch (const std::exception &e) {
                QMetaObject::invokeMethod(this, "networkError", Qt::QueuedConnection,
                                          Q_ARG(QString, QString("ASIO hatası: %1").arg(e.what())));
            }
        });
        t.detach();

        isRunning = true;
        emit networkStarted();
        return true;
    }
    catch (const std::exception &e) {
        emit networkError(QString("Ağ başlatılırken hata: %1").arg(e.what()));
        return false;
    }
}

void NetworkManager::stopNetworking()
{
    if (!isRunning) {
        return;
    }

    try {
        // Tüm soketleri kapat
        if (sendSocket) {
            sendSocket->close();
            sendSocket.reset();
        }

        // QMap ile shared_ptr kullanarak tüm soketleri kapatıyoruz
        for (auto &socket : receiveSocketMap) {
            if (socket) {
                socket->close();
            }
        }

        receiveSocketMap.clear();

        // ASIO işlemlerini durdur
        terminate();

        isRunning = false;
        emit networkStopped();
    }
    catch (const std::exception &e) {
        qWarning() << "Ağ durdurulurken hata:" << e.what();
    }
}

void NetworkManager::sendData(const QByteArray &data)
{
    if (!isRunning || !sendSocket) {
        return;
    }

    try {
        // Hedef multicast adresini oluştur
        asio::ip::udp::endpoint endpoint(
            asio::ip::address::from_string(broadcastAddress.toStdString()),
            MULTICAST_PORT
            );

        // Veriyi gönder
        sendSocket->async_send_to(
            asio::buffer(data.constData(), data.size()),
            endpoint,
            [this](const asio::error_code &error, std::size_t /* bytes_transferred */) {
                if (error) {
                    QMetaObject::invokeMethod(this, "networkError", Qt::QueuedConnection,
                                              Q_ARG(QString, QString("Veri gönderme hatası: %1").arg(error.message().c_str())));
                }
            }
            );
    }
    catch (const std::exception &e) {
        emit networkError(QString("Veri gönderme hatası: %1").arg(e.what()));
    }
}

void NetworkManager::startReceive(const QString &address)
{
    try {
        // Dinleme soketi oluştur (shared_ptr ile)
        auto socket = std::make_shared<asio::ip::udp::socket>(*ioContext);

        // Soket seçeneklerini ayarla
        socket->open(asio::ip::udp::v4());
        socket->set_option(asio::ip::udp::socket::reuse_address(true));

        // Soket'i port'a bağla
        socket->bind(asio::ip::udp::endpoint(asio::ip::address_v4::any(), MULTICAST_PORT));

        // Multicast grubuna katıl
        socket->set_option(asio::ip::multicast::join_group(
            asio::ip::address::from_string(address.toStdString())
            ));

        // Veri alımını başlat
        auto bufPtr = std::make_shared<std::vector<char>>(64 * 1024); // 64KB buffer
        auto endpointPtr = std::make_shared<asio::ip::udp::endpoint>();

        // Raw pointer yerine shared_ptr kullanarak lambda içinde soket referansını sakla
        socket->async_receive_from(
            asio::buffer(*bufPtr),
            *endpointPtr,
            [this, bufPtr, endpointPtr, address, socketPtr = socket](
                const asio::error_code &error,
                std::size_t bytesTransferred) {

                if (!error) {
                    // Veriyi işle
                    QByteArray receivedData(bufPtr->data(), bytesTransferred);
                    QMetaObject::invokeMethod(this, "dataReceived", Qt::QueuedConnection,
                                              Q_ARG(QByteArray, receivedData),
                                              Q_ARG(QString, address));

                    // Yeni veri alımını başlat
                    if (isRunning) {
                        auto newBufPtr = std::make_shared<std::vector<char>>(64 * 1024);
                        auto newEndpointPtr = std::make_shared<asio::ip::udp::endpoint>();

                        socketPtr->async_receive_from(
                            asio::buffer(*newBufPtr),
                            *newEndpointPtr,
                            [this, newBufPtr, newEndpointPtr, address, socketPtr](
                                const asio::error_code &error,
                                std::size_t bytesTransferred) {

                                if (!error) {
                                    // Veriyi işle
                                    QByteArray receivedData(newBufPtr->data(), bytesTransferred);
                                    QMetaObject::invokeMethod(this, "dataReceived", Qt::QueuedConnection,
                                                              Q_ARG(QByteArray, receivedData),
                                                              Q_ARG(QString, address));

                                    // Recursive olarak yeni alım başlat
                                    if (isRunning) {
                                        this->startReceive(address);
                                    }
                                }
                                else if (error != asio::error::operation_aborted) {
                                    QMetaObject::invokeMethod(this, "networkError", Qt::QueuedConnection,
                                                              Q_ARG(QString,
                                                                    QString("Veri alma hatası: %1").arg(error.message().c_str())));
                                }
                            }
                            );
                    }
                }
                else if (error != asio::error::operation_aborted) {
                    QMetaObject::invokeMethod(this, "networkError", Qt::QueuedConnection,
                                              Q_ARG(QString,
                                                    QString("Veri alma hatası: %1").arg(error.message().c_str())));
                }
            }
            );

        // Soketi sakla (QMap içinde shared_ptr)
        receiveSocketMap[address] = socket;
    }
    catch (const std::exception &e) {
        emit networkError(QString("Dinleme başlatılamadı %1: %2").arg(address).arg(e.what()));
    }
}

void NetworkManager::handleReceive(const asio::error_code &error,
                                   std::size_t bytesTransferred,
                                   const QString &sourceAddress)
{
    if (error) {
        if (error != asio::error::operation_aborted) {
            emit networkError(QString("Veri alma hatası: %1").arg(error.message().c_str()));
        }
        return;
    }

    // Alınan veriyi QByteArray'e dönüştür ve sinyal gönder
    QByteArray receivedData(receiveBuffer->data(), bytesTransferred);
    emit dataReceived(receivedData, sourceAddress);
}
