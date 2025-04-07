#ifndef SIMPLEVOIP_H
#define SIMPLEVOIP_H

#include <QObject>
#include <QtNetwork/QUdpSocket>
#include <QtNetwork/QHostAddress>
#include <QTimer>
#include <QByteArray>
#include <QDataStream>
#include <QHostAddress>
#include <QMap>
#include <QElapsedTimer>
#include <QMutex>
#include <opus.h>
#include <portaudio.h>
#include <QFile>

class SimpleVoIP : public QObject {
    Q_OBJECT

public:
    explicit SimpleVoIP(QObject *parent = nullptr);
    ~SimpleVoIP();

    bool start(const QString &multicastAddress, int port);
    void stop();
    void setInputDevice(int deviceIndex);
    void setOutputDevice(int deviceIndex);
    void setVolume(int volume); // 0-100
    void enableDetailedLogging(bool enable);

private slots:
    void processPendingDatagrams();
    void logPerformanceMetrics();


private:
    void openLogFile();
    QFile m_logFile;
    void writeLogToFile(const QString &message);
    // Gelişmiş jitter buffer için yapı
    struct JitterBufferPacket {
        QByteArray data;
        qint64 receiveTime;
        bool isLost;
    };

    // PortAudio callback
    static int audioCallback(const void *input, void *output,
                             unsigned long frameCount,
                             const PaStreamCallbackTimeInfo *timeInfo,
                             PaStreamCallbackFlags statusFlags,
                             void *userData);

    // Gelişmiş jitter buffer metodları
    void insertPacketToBuffer(uint32_t seqNum, const QByteArray &audioData);
    QByteArray retrievePacketFromBuffer(uint32_t &expectedSeq);

    // Performans ve hata ayıklama
    void logPacketLoss(uint32_t expectedSeq, uint32_t receivedSeq);
    void logAudioQualityMetrics();

    // Çalışma durumu
    bool m_running;
    bool m_detailedLoggingEnabled;

    // Audio ayarları
    static const int SAMPLE_RATE = 48000;
    static const int FRAME_SIZE = 960; // 20ms at 48kHz
    static const int CHANNELS = 1;

    // PortAudio
    PaStream *m_inputStream;
    PaStream *m_outputStream;

    // Opus
    OpusEncoder *m_encoder;
    OpusDecoder *m_decoder;

    // Ağ
    QUdpSocket m_socket;
    QHostAddress m_multicastAddress;
    quint16 m_port;

    // Gelişmiş Jitter Buffer
    QMap<uint32_t, JitterBufferPacket> m_jitterBuffer;
    uint32_t m_lastPlayedSequence;
    int m_outputVolume;

    // Ses seviyesi kontrol
    float m_micGain;
    float m_speakerGain;

    // Performans izleme
    uint32_t m_sequenceCounter;
    uint32_t m_totalPacketsSent;
    uint32_t m_totalPacketsReceived;
    uint32_t m_totalPacketsLost;

    // Mutex (thread güvenliği için)
    QMutex m_bufferMutex;

    // Performans ölçüm zamanlayıcısı
    QElapsedTimer m_performanceTimer;
};

#endif // SIMPLEVOIP_H
