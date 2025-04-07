#include "settingsmanager.h"

// Varsayılan değerler
const QString SettingsManager::DEFAULT_BROADCAST_ADDRESS = "239.255.0.1";
const QStringList SettingsManager::DEFAULT_LISTEN_ADDRESSES = {"239.255.0.2", "239.255.0.3"};
const int SettingsManager::DEFAULT_INPUT_DEVICE = 0;
const int SettingsManager::DEFAULT_OUTPUT_DEVICE = 0;
const int SettingsManager::DEFAULT_BITRATE = 64000;
const int SettingsManager::DEFAULT_FRAME_SIZE = 480;

SettingsManager::SettingsManager(QObject *parent)
    : QObject(parent),
    broadcastAddress(DEFAULT_BROADCAST_ADDRESS),
    listenAddresses(DEFAULT_LISTEN_ADDRESSES),
    inputDeviceIndex(DEFAULT_INPUT_DEVICE),
    outputDeviceIndex(DEFAULT_OUTPUT_DEVICE),
    bitrate(DEFAULT_BITRATE),
    frameSize(DEFAULT_FRAME_SIZE)
{
}

void SettingsManager::loadSettings()
{
    // Multicast ayarları
    settings.beginGroup("Multicast");
    broadcastAddress = settings.value("broadcastAddress", DEFAULT_BROADCAST_ADDRESS).toString();
    listenAddresses = settings.value("listenAddresses", DEFAULT_LISTEN_ADDRESSES).toStringList();
    settings.endGroup();

    // Ses cihazı ayarları
    settings.beginGroup("Audio");
    inputDeviceIndex = settings.value("inputDevice", DEFAULT_INPUT_DEVICE).toInt();
    outputDeviceIndex = settings.value("outputDevice", DEFAULT_OUTPUT_DEVICE).toInt();
    settings.endGroup();

    // Codec ayarları
    settings.beginGroup("Codec");
    bitrate = settings.value("bitrate", DEFAULT_BITRATE).toInt();
    frameSize = settings.value("frameSize", DEFAULT_FRAME_SIZE).toInt();
    settings.endGroup();
}

void SettingsManager::saveSettings()
{
    // Multicast ayarları
    settings.beginGroup("Multicast");
    settings.setValue("broadcastAddress", broadcastAddress);
    settings.setValue("listenAddresses", listenAddresses);
    settings.endGroup();

    // Ses cihazı ayarları
    settings.beginGroup("Audio");
    settings.setValue("inputDevice", inputDeviceIndex);
    settings.setValue("outputDevice", outputDeviceIndex);
    settings.endGroup();

    // Codec ayarları
    settings.beginGroup("Codec");
    settings.setValue("bitrate", bitrate);
    settings.setValue("frameSize", frameSize);
    settings.endGroup();

    settings.sync();
}

QString SettingsManager::getBroadcastAddress() const
{
    return broadcastAddress;
}

void SettingsManager::setBroadcastAddress(const QString &address)
{
    broadcastAddress = address;
}

QStringList SettingsManager::getListenAddresses() const
{
    return listenAddresses;
}

void SettingsManager::setListenAddresses(const QStringList &addresses)
{
    listenAddresses = addresses;
}

int SettingsManager::getInputDeviceIndex() const
{
    return inputDeviceIndex;
}

void SettingsManager::setInputDeviceIndex(int index)
{
    inputDeviceIndex = index;
}

int SettingsManager::getOutputDeviceIndex() const
{
    return outputDeviceIndex;
}

void SettingsManager::setOutputDeviceIndex(int index)
{
    outputDeviceIndex = index;
}

int SettingsManager::getBitrate() const
{
    return bitrate;
}

void SettingsManager::setBitrate(int newBitrate)
{
    bitrate = newBitrate;
}

int SettingsManager::getFrameSize() const
{
    return frameSize;
}

void SettingsManager::setFrameSize(int newFrameSize)
{
    frameSize = newFrameSize;
}
