#include "SimpleVoIP.h"
#include <QDebug>
#include <QThread>
#include <QDateTime>
#include <algorithm>

SimpleVoIP::SimpleVoIP(QObject *parent) : QObject(parent),
    m_running(false),
    m_detailedLoggingEnabled(false),
    m_inputStream(nullptr),
    m_outputStream(nullptr),
    m_encoder(nullptr),
    m_decoder(nullptr),
    m_lastPlayedSequence(0),
    m_outputVolume(80),
    m_micGain(2.0f),
    m_speakerGain(2.0f),
    m_sequenceCounter(0),
    m_totalPacketsSent(0),
    m_totalPacketsReceived(0),
    m_totalPacketsLost(0)
{
    m_performanceTimer.start();
    Pa_Initialize();
}

SimpleVoIP::~SimpleVoIP()
{
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }
    stop();
    Pa_Terminate();
}

void SimpleVoIP::enableDetailedLogging(bool enable)
{
    m_detailedLoggingEnabled = enable;
}

bool SimpleVoIP::start(const QString &multicastAddress, int port)
{
    qDebug() << "VoIP başlatılıyor...";

    if (m_running) {
        qDebug() << "VoIP zaten çalışıyor. Önce durduruluyor...";
        stop();
        // Soketlerin kapanması için kısa bir bekleme
        QThread::msleep(200);
    }
    qDebug() << "VoIP başlatılıyor...";

    // Opus encoder/decoder kur
    int error;
    qDebug() << "Opus encoder oluşturuluyor...";
    m_encoder = opus_encoder_create(SAMPLE_RATE, CHANNELS, OPUS_APPLICATION_VOIP, &error);
    if (error != OPUS_OK) {
        qDebug() << "HATA: Opus encoder oluşturulamadı:" << opus_strerror(error);
        return false;
    }

    qDebug() << "Opus decoder oluşturuluyor...";
    m_decoder = opus_decoder_create(SAMPLE_RATE, CHANNELS, &error);
    if (error != OPUS_OK) {
        qDebug() << "HATA: Opus decoder oluşturulamadı:" << opus_strerror(error);
        opus_encoder_destroy(m_encoder);
        m_encoder = nullptr;
        return false;
    }

    // Opus ayarlarını yapılandır
    qDebug() << "Opus ayarları yapılandırılıyor...";
    opus_encoder_ctl(m_encoder, OPUS_SET_BITRATE(64000));
    opus_encoder_ctl(m_encoder, OPUS_SET_SIGNAL(OPUS_SIGNAL_VOICE));
    opus_encoder_ctl(m_encoder, OPUS_SET_COMPLEXITY(8));
    opus_encoder_ctl(m_encoder, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_FULLBAND));
    opus_decoder_ctl(m_decoder, OPUS_SET_GAIN(4000));
    opus_encoder_ctl(m_encoder, OPUS_SET_VBR(1));

    // UDP Socket kur
    qDebug() << "UDP Socket kuruluyor...";
    m_multicastAddress = QHostAddress(multicastAddress);
    m_port = port;

    // Socket'i yeniden oluştur (kapanmış olduğundan emin ol)
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        m_socket.close();
        qDebug() << "Önceki socket kapatıldı";
    }

    // Farklı bir port deneme opsiyonu ekle
    bool bindSuccess = false;
    int attemptCount = 0;
    int currentPort = port;

    try {
        while (!bindSuccess && attemptCount < 5) {
            qDebug() << "Socket bind ediliyor... Port:" << currentPort;
            bindSuccess = m_socket.bind(QHostAddress::AnyIPv4, currentPort,
                                        QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);

            if (!bindSuccess) {
                qDebug() << "UYARI: Socket bind başarısız:" << m_socket.errorString();
                // Port numarasını artırarak dene
                currentPort++;
                attemptCount++;
            }
        }

        if (!bindSuccess) {
            qDebug() << "HATA: Socket bind başarısız:" << m_socket.errorString();
            opus_encoder_destroy(m_encoder);
            opus_decoder_destroy(m_decoder);
            m_encoder = nullptr;
            m_decoder = nullptr;
            return false;
        }

        qDebug() << "Socket başarıyla bind edildi, port:" << currentPort;
        m_port = currentPort; // Gerçek kullanılan portu güncelle

        qDebug() << "Multicast gruba katılınıyor...";
        bool joinResult = m_socket.joinMulticastGroup(m_multicastAddress);
        if (!joinResult) {
            qDebug() << "HATA: Multicast gruba katılma başarısız:" << m_socket.errorString();
            opus_encoder_destroy(m_encoder);
            opus_decoder_destroy(m_decoder);
            m_encoder = nullptr;
            m_decoder = nullptr;
            return false;
        }
    } catch (const std::exception& e) {
        qDebug() << "HATA: UDP Socket kurulumu sırasında exception:" << e.what();
        opus_encoder_destroy(m_encoder);
        opus_decoder_destroy(m_decoder);
        m_encoder = nullptr;
        m_decoder = nullptr;
        return false;
    }

    // Okuma sinyalini bağla
    connect(&m_socket, &QUdpSocket::readyRead, this, &SimpleVoIP::processPendingDatagrams);

    qDebug() << "PortAudio başlatılıyor...";
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        qDebug() << "HATA: PortAudio başlatılamadı:" << Pa_GetErrorText(err);
        return false;
    }

    int numDevices = Pa_GetDeviceCount();
    qDebug() << "Bulunan ses cihazları:" << numDevices;
    for (int i = 0; i < numDevices; i++) {
        const PaDeviceInfo* info = Pa_GetDeviceInfo(i);
        if (info) {
            qDebug() << "Cihaz" << i << ":" << info->name;
            qDebug() << "  Varsayılan örnekleme hızı:" << info->defaultSampleRate << "Hz";
            qDebug() << "  Max giriş kanalları:" << info->maxInputChannels;
            qDebug() << "  Max çıkış kanalları:" << info->maxOutputChannels;
        }
    }

    // Varsayılan giriş cihazını kullan
    int inputDeviceIndex = Pa_GetDefaultInputDevice();
    if (inputDeviceIndex == paNoDevice) {
        qDebug() << "HATA: Varsayılan giriş cihazı bulunamadı!";
        return false;
    }
    const PaDeviceInfo* inputDeviceInfo = Pa_GetDeviceInfo(inputDeviceIndex);
    qDebug() << "Seçilen giriş cihazı:" << inputDeviceInfo->name;

    // Varsayılan çıkış cihazını kullan
    int outputDeviceIndex = Pa_GetDefaultOutputDevice();
    if (outputDeviceIndex == paNoDevice) {
        qDebug() << "HATA: Varsayılan çıkış cihazı bulunamadı!";
        return false;
    }
    const PaDeviceInfo* outputDeviceInfo = Pa_GetDeviceInfo(outputDeviceIndex);
    qDebug() << "Seçilen çıkış cihazı:" << outputDeviceInfo->name;

    // İki cihaz için kullanılacak örnekleme hızını belirle
    double actualSampleRate = SAMPLE_RATE; // Hedef 48000 Hz

    // Giriş stream parametreleri
    qDebug() << "Input Stream açılıyor...";
    PaStreamParameters inputParams = {};
    inputParams.device = inputDeviceIndex;
    inputParams.channelCount = CHANNELS;
    inputParams.sampleFormat = paInt16;
    inputParams.suggestedLatency = inputDeviceInfo->defaultLowInputLatency;
    inputParams.hostApiSpecificStreamInfo = nullptr;

    // Önce giriş cihazı için format kontrolü
    PaError inputTestResult = Pa_IsFormatSupported(&inputParams, nullptr, SAMPLE_RATE);
    if (inputTestResult != paFormatIsSupported) {
        qDebug() << "UYARI: Giriş cihazı 48000 Hz'yi desteklemiyor! Hata:" << Pa_GetErrorText(inputTestResult);
        qDebug() << "48000 Hz yerine varsayılan giriş oranı kullanılacak:" << inputDeviceInfo->defaultSampleRate;
        actualSampleRate = inputDeviceInfo->defaultSampleRate;
    }

    // Çıkış stream parametreleri
    qDebug() << "Output Stream açılıyor...";
    PaStreamParameters outputParams = {};
    outputParams.device = outputDeviceIndex;
    outputParams.channelCount = CHANNELS;
    outputParams.sampleFormat = paInt16;
    outputParams.suggestedLatency = outputDeviceInfo->defaultLowOutputLatency;
    outputParams.hostApiSpecificStreamInfo = nullptr;

    // Çıkış cihazı için format kontrolü
    PaError outputTestResult = Pa_IsFormatSupported(nullptr, &outputParams, actualSampleRate);
    if (outputTestResult != paFormatIsSupported) {
        qDebug() << "UYARI: Çıkış cihazı" << actualSampleRate << "Hz'yi desteklemiyor! Hata:" << Pa_GetErrorText(outputTestResult);
        qDebug() << "Varsayılan çıkış oranı kullanılacak:" << outputDeviceInfo->defaultSampleRate;
        actualSampleRate = outputDeviceInfo->defaultSampleRate;
    }

    qDebug() << "Kullanılan örnekleme hızı:" << actualSampleRate << "Hz";

    // Giriş stream'i aç
    err = Pa_OpenStream(&m_inputStream,
                        &inputParams,
                        nullptr,
                        actualSampleRate,
                        FRAME_SIZE,
                        paClipOff,
                        audioCallback,
                        this);

    if (err != paNoError) {
        qDebug() << "HATA: PortAudio input stream açılamadı:" << Pa_GetErrorText(err);
        return false;
    }

    // Çıkış stream'i aç
    err = Pa_OpenStream(&m_outputStream,
                        nullptr,
                        &outputParams,
                        actualSampleRate,
                        FRAME_SIZE,
                        paClipOff,
                        nullptr,
                        nullptr);

    if (err != paNoError) {
        qDebug() << "HATA: PortAudio output stream açılamadı:" << Pa_GetErrorText(err);
        Pa_CloseStream(m_inputStream);
        m_inputStream = nullptr;
        return false;
    }

    // Streamleri başlat
    qDebug() << "Streamler başlatılıyor...";
    err = Pa_StartStream(m_inputStream);
    if (err != paNoError) {
        qDebug() << "HATA: PortAudio input stream başlatılamadı:" << Pa_GetErrorText(err);
        Pa_CloseStream(m_inputStream);
        Pa_CloseStream(m_outputStream);
        m_inputStream = nullptr;
        m_outputStream = nullptr;
        return false;
    }

    err = Pa_StartStream(m_outputStream);
    if (err != paNoError) {
        qDebug() << "HATA: PortAudio output stream başlatılamadı:" << Pa_GetErrorText(err);
        Pa_StopStream(m_inputStream);
        Pa_CloseStream(m_inputStream);
        Pa_CloseStream(m_outputStream);
        m_inputStream = nullptr;
        m_outputStream = nullptr;
        return false;
    }

    if (m_detailedLoggingEnabled) {
        QTimer *metricsTimer = new QTimer(this);
        connect(metricsTimer, &QTimer::timeout, this, &SimpleVoIP::logPerformanceMetrics);
        metricsTimer->start(5000);
    }

    if (m_detailedLoggingEnabled) {
        openLogFile(); // Log dosyasını aç
    }

    if (m_detailedLoggingEnabled) {
        QString logFileName = QString("voip_log_%1.txt")
                                  .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));
        m_logFile.setFileName(logFileName);
        if (m_logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream stream(&m_logFile);
            stream << "VoIP Log Başlangıç: "
                   << QDateTime::currentDateTime().toString() << "\n";
        }
    }

    qDebug() << "VoIP başarıyla başlatıldı!";
    m_running = true;
    m_lastPlayedSequence = 0;
    m_sequenceCounter = 0;
    return true;
}

void SimpleVoIP::writeLogToFile(const QString &message) {
    if (m_detailedLoggingEnabled && m_logFile.isOpen()) {
        QTextStream stream(&m_logFile);
        stream << QDateTime::currentDateTime().toString()
               << ": " << message << "\n";
    }
}

void SimpleVoIP::insertPacketToBuffer(uint32_t seqNum, const QByteArray &audioData)
{
    QMutexLocker locker(&m_bufferMutex);

    if (m_jitterBuffer.size() > 100) {
        while (m_jitterBuffer.size() > 50) {
            m_jitterBuffer.erase(m_jitterBuffer.begin());
        }
    }

    JitterBufferPacket packet;
    packet.data = audioData;
    packet.receiveTime = QDateTime::currentMSecsSinceEpoch();
    packet.isLost = false;

    m_jitterBuffer[seqNum] = packet;
}

void SimpleVoIP::stop()
{
    if (!m_running)
        return;

    qDebug() << "VoIP durduruluyor...";
    m_running = false;

    if (m_detailedLoggingEnabled && m_logFile.isOpen()) {
        QTextStream stream(&m_logFile);
        stream << "VoIP Durduruldu: "
               << QDateTime::currentDateTime().toString() << "\n"
               << "Toplam Performans Raporu:\n"
               << "Gönderilen Paket: " << m_totalPacketsSent << "\n"
               << "Alınan Paket: " << m_totalPacketsReceived << "\n"
               << "Kayıp Paket: " << m_totalPacketsLost << "\n";
    }

    // Socket bağlantısını temizle
    if (m_socket.state() != QAbstractSocket::UnconnectedState) {
        // Multicast gruptan ayrıl
        m_socket.leaveMulticastGroup(m_multicastAddress);
        // Socket'i kapat
        m_socket.close();
    }

    // Signal bağlantısını kaldır
    disconnect(&m_socket, &QUdpSocket::readyRead, this, &SimpleVoIP::processPendingDatagrams);

    // PortAudio akışlarını durdur
    if (m_inputStream) {
        Pa_StopStream(m_inputStream);
        Pa_CloseStream(m_inputStream);
        m_inputStream = nullptr;
    }

    if (m_outputStream) {
        Pa_StopStream(m_outputStream);
        Pa_CloseStream(m_outputStream);
        m_outputStream = nullptr;
    }

    // Opus codec'leri temizle
    if (m_encoder) {
        opus_encoder_destroy(m_encoder);
        m_encoder = nullptr;
    }

    if (m_decoder) {
        opus_decoder_destroy(m_decoder);
        m_decoder = nullptr;
    }

    // Jitter buffer temizle
    m_jitterBuffer.clear();

    qDebug() << "VoIP başarıyla durduruldu";
}

void SimpleVoIP::setInputDevice(int deviceIndex)
{
    if (m_running) {
        stop();
        // Input device değiştir
        start(m_multicastAddress.toString(), m_port);
    }
}

void SimpleVoIP::setOutputDevice(int deviceIndex)
{
    if (m_running) {
        stop();
        // Output device değiştir
        start(m_multicastAddress.toString(), m_port);
    }
}

void SimpleVoIP::setVolume(int volume)
{
    m_outputVolume = qBound(0, volume, 100);
    m_speakerGain = m_outputVolume / 25.0f; // 0-4 arası gain
}

int SimpleVoIP::audioCallback(const void *input, void *output,
                              unsigned long frameCount,
                              const PaStreamCallbackTimeInfo *timeInfo,
                              PaStreamCallbackFlags statusFlags,
                              void *userData)
{
    SimpleVoIP *self = static_cast<SimpleVoIP*>(userData);

    if (!self->m_running || !input)
        return paContinue;

    self->m_totalPacketsSent++;
    // Gelen ses verisini güçlendir
    const opus_int16 *inSamples = static_cast<const opus_int16*>(input);
    opus_int16 amplifiedBuffer[FRAME_SIZE];

    for (unsigned long i = 0; i < frameCount; i++) {
        float sample = inSamples[i] * self->m_micGain;
        if (sample > 32767.0f) sample = 32767.0f;
        if (sample < -32768.0f) sample = -32768.0f;
        amplifiedBuffer[i] = static_cast<opus_int16>(sample);
    }

    // Ses verisi örneklerini Opus ile kodla
    unsigned char encodedData[1024];
    opus_int32 encodedBytes = opus_encode(self->m_encoder,
                                          amplifiedBuffer,
                                          frameCount,
                                          encodedData,
                                          sizeof(encodedData));

    if (encodedBytes > 0) {
        // Paket oluştur
        QByteArray packet;
        QDataStream stream(&packet, QIODevice::WriteOnly);
        stream.setByteOrder(QDataStream::LittleEndian);

        // Sıra numarası
        uint32_t seqNum = self->m_sequenceCounter++;
        stream << seqNum;

        // Boyut
        uint16_t size = encodedBytes;
        stream << size;

        // Veri
        stream.writeRawData(reinterpret_cast<const char*>(encodedData), encodedBytes);

        // Paketi gönder
        self->m_socket.writeDatagram(packet, self->m_multicastAddress, self->m_port);
    }

    return paContinue;
}

QByteArray SimpleVoIP::retrievePacketFromBuffer(uint32_t &expectedSeq)
{
    QMutexLocker locker(&m_bufferMutex);

    if (!m_jitterBuffer.contains(expectedSeq)) {
        logPacketLoss(m_lastPlayedSequence, expectedSeq);

        auto it = m_jitterBuffer.find(expectedSeq);
        if (it == m_jitterBuffer.end()) {
            expectedSeq++;
            return QByteArray(); // Paketi atla
        }
    }

    QByteArray data = m_jitterBuffer[expectedSeq].data;
    m_jitterBuffer.remove(expectedSeq);

    return data;
}

void SimpleVoIP::logPacketLoss(uint32_t expectedSeq, uint32_t receivedSeq)
{
    if (!m_detailedLoggingEnabled) return;

    m_totalPacketsLost++;
    qDebug() << "Paket Kayıp Raporu:"
             << "Beklenen Sıra:" << expectedSeq
             << "Alınan Sıra:" << receivedSeq
             << "Toplam Kayıp:" << m_totalPacketsLost;
}

void SimpleVoIP::logPerformanceMetrics()
{
    if (!m_detailedLoggingEnabled) return;

    // Gerçek zamanlı hesaplama
    qint64 elapsedSeconds = m_performanceTimer.elapsed() / 1000;

    QString metricMessage = QString("Performans Metrikleri: "
                                    "Gönderilen Paket: %1, "
                                    "Alınan Paket: %2, "
                                    "Kayıp Paket: %3, "
                                    "Çalışma Süresi: %4 sn, "
                                    "Paket Kayıp Oranı: %5%")
                                .arg(m_totalPacketsSent)
                                .arg(m_totalPacketsReceived)
                                .arg(m_totalPacketsLost)
                                .arg(elapsedSeconds)
                                .arg(m_totalPacketsLost * 100.0 / (m_totalPacketsReceived + 1), 0, 'f', 6);

    qDebug() << metricMessage;
    writeLogToFile(metricMessage);
}

void SimpleVoIP::processPendingDatagrams()
{
    while (m_socket.hasPendingDatagrams()) {
        QByteArray datagram;
        datagram.resize(m_socket.pendingDatagramSize());
        QHostAddress sender;
        quint16 senderPort;

        m_socket.readDatagram(datagram.data(), datagram.size(), &sender, &senderPort);

        QDataStream stream(datagram);
        stream.setByteOrder(QDataStream::LittleEndian);

        uint32_t seqNum;
        uint16_t size;
        stream >> seqNum >> size;

        if (size > 0 && size <= 1024) {
            QByteArray audioData = datagram.mid(sizeof(uint32_t) + sizeof(uint16_t), size);

            // Paketi buffer'a ekle
            insertPacketToBuffer(seqNum, audioData);
            m_totalPacketsReceived++;

            // Gelişmiş jitter buffer mantığı
            uint32_t nextSeq = m_lastPlayedSequence + 1;
            QByteArray dataToPlay = retrievePacketFromBuffer(nextSeq);

            if (!dataToPlay.isEmpty()) {
                // Opus decode et ve oynat
                opus_int16 decodedBuffer[FRAME_SIZE];
                int samplesDecoded = opus_decode(m_decoder,
                                                 reinterpret_cast<const unsigned char*>(dataToPlay.constData()),
                                                 dataToPlay.size(),
                                                 decodedBuffer,
                                                 FRAME_SIZE,
                                                 0);

                if (samplesDecoded > 0) {
                    // Ses seviyesi ayarlama
                    opus_int16 volumeAdjustedBuffer[FRAME_SIZE];
                    for (int i = 0; i < samplesDecoded; i++) {
                        float sample = decodedBuffer[i] * m_speakerGain;
                        volumeAdjustedBuffer[i] = std::clamp<opus_int16>(
                            static_cast<opus_int16>(sample),
                            -32768,
                            32767
                            );
                    }

                    Pa_WriteStream(m_outputStream, volumeAdjustedBuffer, samplesDecoded);
                    m_lastPlayedSequence = nextSeq;
                }
            }
        }
        m_totalPacketsReceived++;
    }
}

void SimpleVoIP::openLogFile()
{
    // Log dosyası varsa önce kapat
    if (m_logFile.isOpen()) {
        m_logFile.close();
    }

    // Yeni log dosyası adı oluştur
    QString logFileName = QString("voip_log_%1.txt")
                              .arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"));

    // Dosya adını ayarla
    m_logFile.setFileName(logFileName);

    // Dosyayı aç
    if (!m_logFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        qDebug() << "Log dosyası açılamadı:" << m_logFile.errorString();
    }
}
