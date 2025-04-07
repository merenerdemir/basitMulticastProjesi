#include "audiomanager.h"
#include <QDebug>
#include <QMutexLocker>
#include <QMutex>
#include <cmath>

// Ses tamponu için global mutex
static QMutex audioBufferMutex;

AudioManager::AudioManager(QObject *parent)
    : QObject(parent),
    stream(nullptr),
    sampleRate(48000),    // Opus codec 48kHz destekliyor
    framesPerBuffer(480), // 10ms @ 48kHz
    inputChannels(1),     // Başlangıç değeri
    outputChannels(1),    // Başlangıç değeri
    selectedInputDevice(-1),
    selectedOutputDevice(-1),
    microphoneMuted(false)
{
    // PortAudio kütüphanesini başlat
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        qCritical() << "PortAudio başlatılamadı:" << Pa_GetErrorText(err);
        emit audioError(QString("PortAudio başlatılamadı: %1").arg(Pa_GetErrorText(err)));
        return;
    }

    // Cihaz listelerini doldur
    refreshDeviceLists();

    // Varsayılan cihazları seç
    setDefaultDevices();
}

void AudioManager::setDefaultDevices()
{
    int defaultInputIndex = Pa_GetDefaultInputDevice();
    int defaultOutputIndex = Pa_GetDefaultOutputDevice();

    qDebug() << "Varsayılan giriş cihazı indeksi:" << defaultInputIndex;
    qDebug() << "Varsayılan çıkış cihazı indeksi:" << defaultOutputIndex;

    // Varsayılan giriş cihazını ara
    bool foundInput = false;
    for (int i = 0; i < inputDeviceList.size(); i++) {
        qDebug() << "Checking input device" << i << "name:" << inputDeviceList[i].info->name
                 << "PortAudio index:" << inputDeviceList[i].paDeviceIndex;

        if (inputDeviceList[i].paDeviceIndex == defaultInputIndex) {
            qDebug() << "Found default input device:" << inputDeviceList[i].info->name;
            setInputDevice(i);
            foundInput = true;
            break;
        }
    }

    // Eğer varsayılan cihaz bulunamadıysa, ilk cihazı seç
    if (!foundInput && inputDeviceList.size() > 0) {
        qDebug() << "Default input device not found, using first available";
        setInputDevice(0);
    }

    // Varsayılan çıkış cihazını ara
    bool foundOutput = false;
    for (int i = 0; i < outputDeviceList.size(); i++) {
        qDebug() << "Checking output device" << i << "name:" << outputDeviceList[i].info->name
                 << "PortAudio index:" << outputDeviceList[i].paDeviceIndex;

        if (outputDeviceList[i].paDeviceIndex == defaultOutputIndex) {
            qDebug() << "Found default output device:" << outputDeviceList[i].info->name;
            setOutputDevice(i);
            foundOutput = true;
            break;
        }
    }

    // Eğer varsayılan cihaz bulunamadıysa, ilk cihazı seç
    if (!foundOutput && outputDeviceList.size() > 0) {
        qDebug() << "Default output device not found, using first available";
        setOutputDevice(0);
    }

    qDebug() << "Varsayılan ses cihazları seçildi.";
}

AudioManager::~AudioManager()
{
    // Ses akışı çalışıyorsa durdur
    if (stream) {
        stopAudioStream();
    }

    // PortAudio kütüphanesini kapat
    PaError err = Pa_Terminate();
    if (err != paNoError) {
        qCritical() << "PortAudio kapatılamadı:" << Pa_GetErrorText(err);
    }
}

bool AudioManager::initialize()
{
    PaError err = Pa_Initialize();
    if (err != paNoError) {
        emit audioError(QString("PortAudio başlatılamadı: %1").arg(Pa_GetErrorText(err)));
        return false;
    }

    refreshDeviceLists();
    return true;
}

void AudioManager::terminate()
{
    if (stream) {
        stopAudioStream();
    }

    PaError err = Pa_Terminate();
    if (err != paNoError) {
        qWarning() << "PortAudio kapatılamadı:" << Pa_GetErrorText(err);
    }
}

void AudioManager::refreshDeviceLists()
{
    inputDeviceList.clear();
    outputDeviceList.clear();

    // Cihazları oku
    int numDevices = Pa_GetDeviceCount();
    if (numDevices < 0) {
        qCritical() << "Ses cihazları listelenemedi:" << Pa_GetErrorText(numDevices);
        emit audioError(QString("Ses cihazları listelenemedi: %1").arg(Pa_GetErrorText(numDevices)));
        return;
    }

    qDebug() << "Toplam" << numDevices << "ses cihazı bulundu.";

    // Tüm cihazları yazdır (debug için)
    for (int i = 0; i < numDevices; i++) {
        const PaDeviceInfo *deviceInfo = Pa_GetDeviceInfo(i);
        qDebug() << "Cihaz" << i << ":" << deviceInfo->name
                 << "- Giriş Kanalları:" << deviceInfo->maxInputChannels
                 << "- Çıkış Kanalları:" << deviceInfo->maxOutputChannels
                 << "- Örnek Hızı:" << deviceInfo->defaultSampleRate;

        // Giriş cihazı
        if (deviceInfo->maxInputChannels > 0) {
            DeviceInfo device;
            device.info = deviceInfo;
            device.paDeviceIndex = i;
            inputDeviceList.append(device);
        }

        // Çıkış cihazı
        if (deviceInfo->maxOutputChannels > 0) {
            DeviceInfo device;
            device.info = deviceInfo;
            device.paDeviceIndex = i;
            outputDeviceList.append(device);
        }
    }
}

QList<QString> AudioManager::getInputDevices()
{
    QList<QString> devices;
    for (const DeviceInfo &device : inputDeviceList) {
        devices.append(QString(device.info->name));
    }
    return devices;
}

QList<QString> AudioManager::getOutputDevices()
{
    QList<QString> devices;
    for (const DeviceInfo &device : outputDeviceList) {
        devices.append(QString(device.info->name));
    }
    return devices;
}

bool AudioManager::setInputDevice(int deviceIndex)
{
    if (deviceIndex < 0 || deviceIndex >= inputDeviceList.size()) {
        emit audioError("Geçersiz giriş cihazı indeksi");
        return false;
    }

    selectedInputDevice = deviceIndex;

    // Cihazın gerçek kanal sayısını kullan (cihaz ne kadar destekliyorsa)
    inputChannels = inputDeviceList[deviceIndex].info->maxInputChannels;

    qDebug() << "Input device set to:" << inputDeviceList[deviceIndex].info->name
             << "with" << inputChannels << "channels"
             << "at" << sampleRate << "Hz";

    return true;
}

bool AudioManager::setOutputDevice(int deviceIndex)
{
    if (deviceIndex < 0 || deviceIndex >= outputDeviceList.size()) {
        emit audioError("Geçersiz çıkış cihazı indeksi");
        return false;
    }

    selectedOutputDevice = deviceIndex;

    // Cihazın gerçek kanal sayısını kullan (cihaz ne kadar destekliyorsa)
    outputChannels = outputDeviceList[deviceIndex].info->maxOutputChannels;

    qDebug() << "Output device set to:" << outputDeviceList[deviceIndex].info->name
             << "with" << outputChannels << "channels"
             << "at" << sampleRate << "Hz";

    return true;
}

bool AudioManager::startAudioStream()
{
    if (stream) {
        emit audioError("Ses akışı zaten çalışıyor");
        return false;
    }

    if (selectedInputDevice < 0 || selectedOutputDevice < 0) {
        emit audioError("Giriş ve çıkış cihazları seçilmelidir");
        return false;
    }

    // Gerçek cihaz indekslerini al
    int inputDeviceIndex = inputDeviceList[selectedInputDevice].paDeviceIndex;
    int outputDeviceIndex = outputDeviceList[selectedOutputDevice].paDeviceIndex;

    qDebug() << "Using input device index:" << inputDeviceIndex;
    qDebug() << "Using output device index:" << outputDeviceIndex;

    // Stream parametrelerini ayarla
    PaStreamParameters inputParameters = {};
    inputParameters.device = inputDeviceIndex;
    inputParameters.channelCount = inputChannels;
    inputParameters.sampleFormat = paFloat32;
    inputParameters.suggestedLatency = inputDeviceList[selectedInputDevice].info->defaultLowInputLatency;
    inputParameters.hostApiSpecificStreamInfo = nullptr;

    PaStreamParameters outputParameters = {};
    outputParameters.device = outputDeviceIndex;
    outputParameters.channelCount = outputChannels;
    outputParameters.sampleFormat = paFloat32;
    outputParameters.suggestedLatency = outputDeviceList[selectedOutputDevice].info->defaultLowOutputLatency;
    outputParameters.hostApiSpecificStreamInfo = nullptr;

    // Debug log bilgisi
    qDebug() << "Starting audio stream with:"
             << "Input device:" << inputDeviceList[selectedInputDevice].info->name
             << "(" << inputChannels << "channels)"
             << "Output device:" << outputDeviceList[selectedOutputDevice].info->name
             << "(" << outputChannels << "channels)"
             << "Sample rate:" << sampleRate
             << "Frames per buffer:" << framesPerBuffer;

    // Stream formatının desteklenip desteklenmediğini kontrol et
    PaError formatErr = Pa_IsFormatSupported(&inputParameters, &outputParameters, sampleRate);
    if (formatErr != paFormatIsSupported) {
        qCritical() << "Ses formatı desteklenmiyor:" << Pa_GetErrorText(formatErr);

        // Çözüm seçenekleri
        bool success = false;

        // 1. Seçenek: Mono kanal ile dene
        if (inputChannels > 1 || outputChannels > 1) {
            PaStreamParameters monoInput = inputParameters;
            PaStreamParameters monoOutput = outputParameters;

            monoInput.channelCount = 1;
            monoOutput.channelCount = 1;

            formatErr = Pa_IsFormatSupported(&monoInput, &monoOutput, sampleRate);
            if (formatErr == paFormatIsSupported) {
                inputChannels = 1;
                outputChannels = 1;
                inputParameters.channelCount = 1;
                outputParameters.channelCount = 1;
                success = true;
                qDebug() << "Switched to mono channels for compatibility";
            } else {
                qDebug() << "Mono channels not supported either";
            }
        }

        // 2. Seçenek: 16kHz örnek hızı ile dene
        if (!success && sampleRate != 16000) {
            formatErr = Pa_IsFormatSupported(&inputParameters, &outputParameters, 16000);
            if (formatErr == paFormatIsSupported) {
                sampleRate = 16000;
                success = true;
                qDebug() << "Switched to 16kHz sample rate for compatibility";
            } else {
                qDebug() << "16kHz sample rate not supported either";
            }
        }

        // 3. Seçenek: 24kHz örnek hızı ile dene
        if (!success && sampleRate != 24000) {
            formatErr = Pa_IsFormatSupported(&inputParameters, &outputParameters, 24000);
            if (formatErr == paFormatIsSupported) {
                sampleRate = 24000;
                success = true;
                qDebug() << "Switched to 24kHz sample rate for compatibility";
            } else {
                qDebug() << "24kHz sample rate not supported either";
            }
        }

        // Hala başarısız ise
        if (!success) {
            emit audioError(QString("Ses formatı desteklenmiyor ve alternatifler denendi: %1").arg(Pa_GetErrorText(formatErr)));
            return false;
        }
    }

    // Stream'i aç
    PaError err = Pa_OpenStream(
        &stream,
        &inputParameters,
        &outputParameters,
        sampleRate,
        framesPerBuffer,
        paNoFlag,
        &AudioManager::audioCallback,
        this
        );

    if (err != paNoError) {
        qCritical() << "Ses akışı açılamadı:" << Pa_GetErrorText(err);
        emit audioError(QString("Ses akışı açılamadı: %1").arg(Pa_GetErrorText(err)));
        return false;
    }

    // Stream'i başlat
    err = Pa_StartStream(stream);
    if (err != paNoError) {
        Pa_CloseStream(stream);
        stream = nullptr;
        qCritical() << "Ses akışı başlatılamadı:" << Pa_GetErrorText(err);
        emit audioError(QString("Ses akışı başlatılamadı: %1").arg(Pa_GetErrorText(err)));
        return false;
    }

    emit audioStreamStarted();
    return true;
}

void AudioManager::stopAudioStream()
{
    if (!stream) {
        return;
    }

    // Stream'i durdur
    PaError err = Pa_StopStream(stream);
    if (err != paNoError) {
        qWarning() << "Ses akışı düzgün durdurulamadı:" << Pa_GetErrorText(err);
    }

    // Stream'i kapat
    err = Pa_CloseStream(stream);
    if (err != paNoError) {
        qWarning() << "Ses akışı düzgün kapatılamadı:" << Pa_GetErrorText(err);
    }

    stream = nullptr;
    emit audioStreamStopped();
}

void AudioManager::setMicrophoneMuted(bool muted)
{
    microphoneMuted = muted;
}

bool AudioManager::isMicrophoneMuted() const
{
    return microphoneMuted;
}

int AudioManager::audioCallback(const void *inputBuffer, void *outputBuffer,
                                unsigned long framesPerBuffer,
                                const PaStreamCallbackTimeInfo * /* timeInfo */,
                                PaStreamCallbackFlags /* statusFlags */,
                                void *userData)
{
    AudioManager *self = static_cast<AudioManager*>(userData);

    // Giriş verisini işle (mikrofon)
    if (inputBuffer && !self->microphoneMuted) {
        const float *input = static_cast<const float*>(inputBuffer);
        int bufferSize = framesPerBuffer * self->inputChannels * sizeof(float);

        // Veriyi QByteArray'e dönüştür
        QByteArray audioData(reinterpret_cast<const char*>(input), bufferSize);

        // Ana thread'de işlenmesi için Qt'ye gönder
        QMetaObject::invokeMethod(self, "emitInputAudioReady", Qt::QueuedConnection,
                                  Q_ARG(QByteArray, audioData));
    }

    // Çıkış verisini işle (hoparlör)
    if (outputBuffer) {
        float *output = static_cast<float*>(outputBuffer);

        // Başlangıçta çıkışı temizle
        memset(output, 0, framesPerBuffer * self->outputChannels * sizeof(float));

        // Karıştırma işlemini doğrudan burada yap (PortAudio callback thread'i üzerinde)
        QMutexLocker locker(&audioBufferMutex);

        // outputBuffers haritasındaki her bir kaynak için
        for (auto it = self->outputBuffers.begin(); it != self->outputBuffers.end(); ++it) {
            const QByteArray &buffer = it.value();

            // Buffer boyutu uyuşuyor mu kontrol et
            int expectedSize = framesPerBuffer * self->outputChannels * sizeof(float);
            if (buffer.size() == expectedSize) {
                // Verileri karıştır
                const float *bufferData = reinterpret_cast<const float*>(buffer.constData());

                // Basit karıştırma: Ses verilerini topla
                for (unsigned long i = 0; i < framesPerBuffer * self->outputChannels; i++) {
                    output[i] += bufferData[i];
                }
            }
        }

        // Clipping önleme (sesi sınırla, -1.0 ile 1.0 arasında)
        for (unsigned long i = 0; i < framesPerBuffer * self->outputChannels; i++) {
            if (output[i] > 1.0f) output[i] = 1.0f;
            if (output[i] < -1.0f) output[i] = -1.0f;
        }
    }

    return paContinue;
}

void AudioManager::emitInputAudioReady(const QByteArray &audioData)
{
    // Bu metod asenkron olarak çağrılır, threadsafe olmalıdır
    emit inputAudioReady(audioData);
}

// Artık doğrudan callback içinde ses karıştırma yaptığımız için bu metodu kullanmıyoruz
void AudioManager::processMixingQueuedConnection(float *outputBuffer, unsigned long framesPerBuffer)
{
    // Bu metodun implementasyonu artık kullanılmıyor, ancak slot tanımı nedeniyle muhafaza edildi
    qDebug() << "processMixingQueuedConnection çağrıldı, ancak artık kullanılmıyor.";
}

void AudioManager::handleOutputAudio(const QByteArray &audioData, const QString &sourceAddress)
{
    // Thread-safe erişim için mutex kullan
    QMutexLocker locker(&audioBufferMutex);

    // Veriyi kaynağa göre buffer'a ekle
    outputBuffers[sourceAddress] = audioData;
}
