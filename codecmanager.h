#ifndef CODECMANAGER_H
#define CODECMANAGER_H

#include <QObject>
#include <QByteArray>
#include <QMap>

// Forward declaration for Opus types
typedef struct OpusEncoder OpusEncoder;
typedef struct OpusDecoder OpusDecoder;

class CodecManager : public QObject
{
    Q_OBJECT
public:
    explicit CodecManager(QObject *parent = nullptr);
    ~CodecManager();

    bool initialize();
    void terminate();

    // Kodlama ayarlarını yapılandırma
    void configureBitrate(int bitrate);
    void configureFrameSize(int frameSize);
    void setChannels(int channelCount); // Kanal sayısını ayarla
    void setSampleRate(int sampleRate); // Ses örnek hızını ayarla

signals:
    void encodedDataReady(const QByteArray &encodedData);
    void decodedDataReady(const QByteArray &decodedData, const QString &sourceAddress);
    void codecError(const QString &errorMessage);

public slots:
    void encodeAudioData(const QByteArray &rawAudioData);
    void decodeAudioData(const QByteArray &encodedAudioData, const QString &sourceAddress);

private:
    // Opus encoder/decoder handles
    OpusEncoder *encoder;
    QMap<QString, OpusDecoder*> decoders; // Her multicast adres için ayrı decoder

    // Codec ayarları
    int sampleRate;
    int channels;
    int bitrate;
    int frameSize;
    int application; // OPUS_APPLICATION_VOIP veya OPUS_APPLICATION_AUDIO

    // Hata düzeltme ve paket kayıpları için
    bool useFEC; // Forward Error Correction
};

#endif // CODECMANAGER_H
