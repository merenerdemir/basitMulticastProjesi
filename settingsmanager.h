#ifndef SETTINGSMANAGER_H
#define SETTINGSMANAGER_H

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>

class SettingsManager : public QObject
{
    Q_OBJECT
public:
    explicit SettingsManager(QObject *parent = nullptr);

    // Ayarları yükleme ve kaydetme
    void loadSettings();
    void saveSettings();

    // Multicast ayarları
    QString getBroadcastAddress() const;
    void setBroadcastAddress(const QString &address);

    QStringList getListenAddresses() const;
    void setListenAddresses(const QStringList &addresses);

    // Ses cihazı ayarları
    int getInputDeviceIndex() const;
    void setInputDeviceIndex(int index);

    int getOutputDeviceIndex() const;
    void setOutputDeviceIndex(int index);

    // Codec ayarları
    int getBitrate() const;
    void setBitrate(int bitrate);

    int getFrameSize() const;
    void setFrameSize(int frameSize);

private:
    QSettings settings;

    // Geçici saklama için değişkenler
    QString broadcastAddress;
    QStringList listenAddresses;
    int inputDeviceIndex;
    int outputDeviceIndex;
    int bitrate;
    int frameSize;

    // Varsayılan değerler
    static const QString DEFAULT_BROADCAST_ADDRESS;
    static const QStringList DEFAULT_LISTEN_ADDRESSES;
    static const int DEFAULT_INPUT_DEVICE;
    static const int DEFAULT_OUTPUT_DEVICE;
    static const int DEFAULT_BITRATE;
    static const int DEFAULT_FRAME_SIZE;
};

#endif // SETTINGSMANAGER_H
