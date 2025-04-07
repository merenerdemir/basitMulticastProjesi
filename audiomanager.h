#ifndef AUDIOMANAGER_H
#define AUDIOMANAGER_H

#include <QObject>
#include <QByteArray>
#include <QMap>
#include <QList>
#include <QPair>

// PortAudio başlık dosyası
#include <portaudio.h>

class AudioManager : public QObject
{
    Q_OBJECT
public:
    explicit AudioManager(QObject *parent = nullptr);
    ~AudioManager();

    bool initialize();
    void terminate();

    QList<QString> getInputDevices();
    QList<QString> getOutputDevices();

    bool setInputDevice(int deviceIndex);
    bool setOutputDevice(int deviceIndex);

    bool startAudioStream();
    void stopAudioStream();

    void setMicrophoneMuted(bool muted);
    bool isMicrophoneMuted() const;

    // Getter metodları
    int getSampleRate() const { return sampleRate; }
    int getInputChannels() const { return inputChannels; }
    int getOutputChannels() const { return outputChannels; }

signals:
    void inputAudioReady(const QByteArray &audioData);
    void audioStreamStarted();
    void audioStreamStopped();
    void audioError(const QString &errorMessage);

public slots:
    void handleOutputAudio(const QByteArray &audioData, const QString &sourceAddress);

private slots: // Önemli: Bu metodu private slots bölümüne taşıdık
    void processMixingQueuedConnection(float *outputBuffer, unsigned long framesPerBuffer);
    void emitInputAudioReady(const QByteArray &audioData); // Bunu da slots bölümüne taşıdık

private:
    // PortAudio handles
    PaStream *stream;

    // Callback functions for PortAudio
    static int audioCallback(const void *inputBuffer, void *outputBuffer,
                             unsigned long framesPerBuffer,
                             const PaStreamCallbackTimeInfo *timeInfo,
                             PaStreamCallbackFlags statusFlags,
                             void *userData);

    // Refreshes the device lists
    void refreshDeviceLists();

    // Sets default input and output devices
    void setDefaultDevices();

    // Audio settings
    int sampleRate;
    int framesPerBuffer;
    int inputChannels;
    int outputChannels;
    int selectedInputDevice;
    int selectedOutputDevice;
    bool microphoneMuted;

    // Cihaz listelerini ve gerçek indekslerini saklamak için
    struct DeviceInfo {
        const PaDeviceInfo *info;
        int paDeviceIndex;
    };

    QList<DeviceInfo> inputDeviceList;
    QList<DeviceInfo> outputDeviceList;

    // Ses verisi tamponları
    QByteArray inputBuffer;
    QMap<QString, QByteArray> outputBuffers; // Her multicast adres için ayrı buffer
};

#endif // AUDIOMANAGER_H
