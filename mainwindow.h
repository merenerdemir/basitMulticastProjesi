#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QListWidget>
#include <QStatusBar>
#include <QComboBox>
#include <QPushButton>

#include "audiomanager.h"
#include "codecmanager.h"
#include "networkmanager.h"
#include "settingsmanager.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // UI event handlers
    void onStartButtonClicked();
    void onStopButtonClicked();
    void onMuteButtonToggled(bool checked);
    void onBroadcastAddressChanged(const QString &address);
    void onListenAddressesChanged();
    void onInputDeviceChanged(int index);
    void onOutputDeviceChanged(int index);

    // Error handlers
    void handleAudioError(const QString &error);
    void handleCodecError(const QString &error);
    void handleNetworkError(const QString &error);

private:
    // UI setup methods
    void setupUi();
    void setupConnections();
    void populateDeviceLists();
    void populateMulticastAddresses();
    void loadSettings();
    void saveSettings();

    // Status updates
    void updateStatusBar(const QString &message);

    // UI components
    QComboBox *comboBoxInputDevice;
    QComboBox *comboBoxOutputDevice;
    QPushButton *pushButtonMute;
    QComboBox *comboBoxBroadcast;
    QListWidget *listWidgetListen;
    QPushButton *pushButtonStart;
    QPushButton *pushButtonStop;

    // Application components
    AudioManager *audioManager;
    CodecManager *codecManager;
    NetworkManager *networkManager;
    SettingsManager *settingsManager;

    // Application state
    bool isRunning;
};

#endif // MAINWINDOW_H
