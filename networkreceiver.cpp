#include "NetworkReceiver.h"
#include <QDebug>
#include <QRegularExpression>

// NetworkReceiver implementation
NetworkReceiver::NetworkReceiver(asio::io_context& ioContext, const QString& listenAddress, int port)
    : socket(ioContext), running(false)
{
    try {
        // Open the socket
        socket.open(asio::ip::udp::v4());

        // Allow address reuse (multiple applications on same machine)
        socket.set_option(asio::ip::udp::socket::reuse_address(true));

        // Bind to the specified port
        socket.bind(asio::ip::udp::endpoint(asio::ip::udp::v4(), port));

        // Join the multicast group
        asio::ip::address multicastAddress = asio::ip::address::from_string(listenAddress.toStdString());
        socket.set_option(asio::ip::multicast::join_group(multicastAddress));
    }
    catch (const std::exception& e) {
        qWarning() << "NetworkReceiver initialization error:" << e.what();
    }
}

NetworkReceiver::~NetworkReceiver()
{
    stop();
}

void NetworkReceiver::start()
{
    if (!running) {
        running = true;
        startReceive();
    }
}

void NetworkReceiver::stop()
{
    running = false;
    try {
        socket.close();
    }
    catch (const std::exception& e) {
        qWarning() << "Error closing receiver socket:" << e.what();
    }
}

void NetworkReceiver::startReceive()
{
    if (!running) {
        return;
    }

    socket.async_receive_from(
        asio::buffer(recvBuffer, maxPacketSize), senderEndpoint,
        [this](const asio::error_code& error, std::size_t bytesReceived) {
            handleReceive(error, bytesReceived);
        }
        );
}

void NetworkReceiver::handleReceive(const asio::error_code& error, std::size_t bytesReceived)
{
    if (!running) {
        return;
    }

    if (!error) {
        QByteArray data(recvBuffer, static_cast<int>(bytesReceived));
        emit dataReceived(data);
    }
    else {
        qWarning() << "Receive error:" << error.message().c_str();
    }

    // Continue receiving
    startReceive();
}

// NetworkSender implementation
NetworkSender::NetworkSender(asio::io_context& ioContext, const QString& broadcastAddress, int port)
    : socket(ioContext), initialized(false)
{
    try {
        // Create endpoint for broadcasting
        endpoint = asio::ip::udp::endpoint(
            asio::ip::address::from_string(broadcastAddress.toStdString()),
            port
            );

        // Open the socket
        socket.open(asio::ip::udp::v4());

        // Allow address reuse
        socket.set_option(asio::ip::udp::socket::reuse_address(true));

        // Enable sending to multicast
        socket.set_option(asio::ip::multicast::enable_loopback(true));

        // Set TTL for multicast packets
        socket.set_option(asio::ip::multicast::hops(1));

        initialized = true;
    }
    catch (const std::exception& e) {
        qWarning() << "NetworkSender initialization error:" << e.what();
    }
}

NetworkSender::~NetworkSender()
{
    try {
        if (socket.is_open()) {
            socket.close();
        }
    }
    catch (const std::exception& e) {
        qWarning() << "Error closing sender socket:" << e.what();
    }
}

bool NetworkSender::initialize()
{
    return initialized;
}

void NetworkSender::sendData(const QByteArray& data)
{
    if (!initialized || data.isEmpty()) {
        return;
    }

    try {
        socket.send_to(asio::buffer(data.data(), data.size()), endpoint);
    }
    catch (const std::exception& e) {
        qWarning() << "Error sending data:" << e.what();
    }
}

// NetworkManager implementation
NetworkManager::NetworkThread::NetworkThread(asio::io_context& ioContext)
    : ioContext(ioContext)
{
}

void NetworkManager::NetworkThread::run()
{
    try {
        asio::io_context::work work(ioContext);
        ioContext.run();
    }
    catch (const std::exception& e) {
        qWarning() << "Network thread error:" << e.what();
    }
}

NetworkManager::NetworkManager(CodecManager *codecManager, QObject *parent)
    : QObject(parent), codecManager(codecManager),
    networkThread(nullptr), sender(nullptr),
    initialized(false), running(false)
{
}

NetworkManager::~NetworkManager()
{
    if (running) {
        stopNetwork();
    }

    if (networkThread) {
        ioContext.stop();
        networkThread->wait();
        delete networkThread;
    }
}

bool NetworkManager::isValidMulticastAddress(const QString& address)
{
    // Validate multicast address (224.0.0.0 to 239.255.255.255)
    QRegularExpression regex("^(22[4-9]|23[0-9])\\.(\\d{1,3})\\.(\\d{1,3})\\.(\\d{1,3})$");
    QRegularExpressionMatch match = regex.match(address);

    if (!match.hasMatch()) {
        return false;
    }

    // Check each octet is <= 255
    for (int i = 1; i <= 4; i++) {
        int octet = match.captured(i).toInt();
        if (octet > 255) {
            return false;
        }
    }

    return true;
}

bool NetworkManager::initialize(const QString& broadcastAddress, const QStringList& listenAddresses, int port)
{
    if (initialized || running) {
        return false;
    }

    // Validate broadcast address
    if (!isValidMulticastAddress(broadcastAddress)) {
        qWarning() << "Invalid multicast broadcast address:" << broadcastAddress;
        return false;
    }

    // Create sender
    sender = new NetworkSender(ioContext, broadcastAddress, port);
    if (!sender->initialize()) {
        qWarning() << "Failed to initialize network sender";
        delete sender;
        sender = nullptr;
        return false;
    }

    // Create receivers for each listen address
    for (const QString& address : listenAddresses) {
        if (!isValidMulticastAddress(address)) {
            qWarning() << "Invalid multicast listen address:" << address;
            continue;
        }

        NetworkReceiver *receiver = new NetworkReceiver(ioContext, address, port);
        connect(receiver, &NetworkReceiver::dataReceived,
                this, &NetworkManager::handleReceivedData);
        receivers.append(receiver);
    }

    if (receivers.isEmpty()) {
        qWarning() << "No valid listen addresses";
        delete sender;
        sender = nullptr;
        return false;
    }

    // Create network thread
    networkThread = new NetworkThread(ioContext);

    initialized = true;
    return true;
}

bool NetworkManager::startNetwork()
{
    if (!initialized || running) {
        return false;
    }

    // Start all receivers
    for (NetworkReceiver *receiver : receivers) {
        receiver->start();
    }

    // Start network thread
    networkThread->start();

    running = true;
    return true;
}

void NetworkManager::stopNetwork()
{
    if (!running) {
        return;
    }

    // Stop all receivers
    for (NetworkReceiver *receiver : receivers) {
        receiver->stop();
    }

    // Clean up
    ioContext.stop();
    if (networkThread) {
        networkThread->wait();
    }

    qDeleteAll(receivers);
    receivers.clear();

    delete sender;
    sender = nullptr;

    running = false;
    initialized = false;
}

void NetworkManager::sendAudioData(const QByteArray& encodedData)
{
    if (running && sender) {
        sender->sendData(encodedData);
    }
}

void NetworkManager::handleReceivedData(const QByteArray& data)
{
    if (running) {
        emit audioDataReceived(data);
    }
}
