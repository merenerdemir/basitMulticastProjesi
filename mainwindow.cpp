#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QComboBox>
#include <QPushButton>
#include <QListWidget>
#include <QStatusBar>
#include <QMenuBar>
#include <QAction>
#include <QMessageBox>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent),
    isRunning(false)
{
    // Uygulama bileşenlerini oluştur
    audioManager = new AudioManager(this);
    codecManager = new CodecManager(this);
    networkManager = new NetworkManager(this);
    settingsManager = new SettingsManager(this);

    // UI bileşenlerini programatik olarak oluştur
    setupUi();

    // Bağlantıları kur
    setupConnections();

    // Arayüzü doldur
    populateDeviceLists();
    populateMulticastAddresses();

    // Ayarları yükle
    loadSettings();

    // Durum çubuğunu güncelle
    updateStatusBar("Hazır");

    // Pencere ayarları
    setWindowTitle("Multicast Sesli İletişim");
    resize(600, 450);
}

MainWindow::~MainWindow()
{
    // Ayarları kaydet
    saveSettings();

    // Çalışıyorsa durdur
    if (isRunning) {
        onStopButtonClicked();
    }
}

void MainWindow::setupUi()
{
    // Ana widget ve layout
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    // ===== Ses Cihazları Grubu =====
    QGroupBox *audioGroupBox = new QGroupBox("Ses Cihazları", centralWidget);
    QGridLayout *audioLayout = new QGridLayout(audioGroupBox);

    // Mikrofon seçimi
    QLabel *inputLabel = new QLabel("Mikrofon:", audioGroupBox);
    comboBoxInputDevice = new QComboBox(audioGroupBox);
    pushButtonMute = new QPushButton("Sessiz", audioGroupBox);
    pushButtonMute->setCheckable(true);

    audioLayout->addWidget(inputLabel, 0, 0);
    audioLayout->addWidget(comboBoxInputDevice, 0, 1);
    audioLayout->addWidget(pushButtonMute, 0, 2);

    // Hoparlör seçimi
    QLabel *outputLabel = new QLabel("Hoparlör:", audioGroupBox);
    comboBoxOutputDevice = new QComboBox(audioGroupBox);

    audioLayout->addWidget(outputLabel, 1, 0);
    audioLayout->addWidget(comboBoxOutputDevice, 1, 1, 1, 2);

    mainLayout->addWidget(audioGroupBox);

    // ===== Multicast Adresler Grubu =====
    QGroupBox *multicastGroupBox = new QGroupBox("Multicast Adresler", centralWidget);
    QGridLayout *multicastLayout = new QGridLayout(multicastGroupBox);

    // Yayın adresi
    QLabel *broadcastLabel = new QLabel("Yayın Adresi:", multicastGroupBox);
    comboBoxBroadcast = new QComboBox(multicastGroupBox);
    comboBoxBroadcast->setEditable(true);

    multicastLayout->addWidget(broadcastLabel, 0, 0);
    multicastLayout->addWidget(comboBoxBroadcast, 0, 1);

    // Dinleme adresleri
    QLabel *listenLabel = new QLabel("Dinlenecek Adresler:", multicastGroupBox);
    listWidgetListen = new QListWidget(multicastGroupBox);
    listWidgetListen->setSelectionMode(QAbstractItemView::MultiSelection);

    multicastLayout->addWidget(listenLabel, 1, 0);
    multicastLayout->addWidget(listWidgetListen, 1, 1);

    mainLayout->addWidget(multicastGroupBox);

    // ===== Başlat/Durdur düğmeleri =====
    QHBoxLayout *buttonLayout = new QHBoxLayout();

    pushButtonStart = new QPushButton("Başlat", centralWidget);
    pushButtonStop = new QPushButton("Durdur", centralWidget);
    pushButtonStop->setEnabled(false);

    buttonLayout->addWidget(pushButtonStart);
    buttonLayout->addWidget(pushButtonStop);

    mainLayout->addLayout(buttonLayout);

    // ===== Durum çubuğu =====
    statusBar()->showMessage("Hazır");

    // ===== Menü çubuğu =====
    QMenu *fileMenu = menuBar()->addMenu("Dosya");
    QAction *exitAction = new QAction("Çıkış", this);
    fileMenu->addAction(exitAction);

    connect(exitAction, &QAction::triggered, this, &QMainWindow::close);
}

void MainWindow::setupConnections()
{
    // UI bağlantıları
    connect(pushButtonStart, &QPushButton::clicked, this, &MainWindow::onStartButtonClicked);
    connect(pushButtonStop, &QPushButton::clicked, this, &MainWindow::onStopButtonClicked);
    connect(pushButtonMute, &QPushButton::toggled, this, &MainWindow::onMuteButtonToggled);
    connect(comboBoxBroadcast, &QComboBox::currentTextChanged, this, &MainWindow::onBroadcastAddressChanged);
    connect(listWidgetListen, &QListWidget::itemSelectionChanged, this, &MainWindow::onListenAddressesChanged);
    connect(comboBoxInputDevice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onInputDeviceChanged);
    connect(comboBoxOutputDevice, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onOutputDeviceChanged);

    // Component bağlantıları
    connect(audioManager, &AudioManager::inputAudioReady, codecManager, &CodecManager::encodeAudioData);
    connect(codecManager, &CodecManager::encodedDataReady, networkManager, &NetworkManager::sendData);
    connect(networkManager, &NetworkManager::dataReceived, codecManager, &CodecManager::decodeAudioData);
    connect(codecManager, &CodecManager::decodedDataReady, audioManager, &AudioManager::handleOutputAudio);

    // Hata bağlantıları
    connect(audioManager, &AudioManager::audioError, this, &MainWindow::handleAudioError);
    connect(codecManager, &CodecManager::codecError, this, &MainWindow::handleCodecError);
    connect(networkManager, &NetworkManager::networkError, this, &MainWindow::handleNetworkError);

    // Durum bağlantıları
    connect(audioManager, &AudioManager::audioStreamStarted, this, [this]() { updateStatusBar("Ses akışı başladı"); });
    connect(audioManager, &AudioManager::audioStreamStopped, this, [this]() { updateStatusBar("Ses akışı durdu"); });
    connect(networkManager, &NetworkManager::networkStarted, this, [this]() { updateStatusBar("Ağ bağlantısı başladı"); });
    connect(networkManager, &NetworkManager::networkStopped, this, [this]() { updateStatusBar("Ağ bağlantısı durdu"); });
}

void MainWindow::populateDeviceLists()
{
    // Mikrofon listesini doldur
    comboBoxInputDevice->clear();
    QList<QString> inputDevices = audioManager->getInputDevices();
    for (const QString &device : inputDevices) {
        comboBoxInputDevice->addItem(device);
    }

    // Hoparlör listesini doldur
    comboBoxOutputDevice->clear();
    QList<QString> outputDevices = audioManager->getOutputDevices();
    for (const QString &device : outputDevices) {
        comboBoxOutputDevice->addItem(device);
    }
}

void MainWindow::populateMulticastAddresses()
{
    // Standart multicast adreslerini ekle
    QStringList defaultAddresses = {
        "239.255.0.1", "239.255.0.2", "239.255.0.3", "239.255.0.4", "239.255.0.5",
        "239.255.0.6", "239.255.0.7", "239.255.0.8", "239.255.0.9", "239.255.0.10"
    };

    // Yayın adresi combobox'ını doldur
    comboBoxBroadcast->clear();
    comboBoxBroadcast->addItems(defaultAddresses);

    // Dinleme listesini doldur
    listWidgetListen->clear();
    for (const QString &address : defaultAddresses) {
        QListWidgetItem *item = new QListWidgetItem(address);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Unchecked);
        listWidgetListen->addItem(item);
    }
}

void MainWindow::loadSettings()
{
    // Ayarları yükle
    settingsManager->loadSettings();

    // Multicast ayarları
    QString broadcastAddress = settingsManager->getBroadcastAddress();
    QStringList listenAddresses = settingsManager->getListenAddresses();

    // Broadcast adresini ayarla
    int broadcastIndex = comboBoxBroadcast->findText(broadcastAddress);
    if (broadcastIndex >= 0) {
        comboBoxBroadcast->setCurrentIndex(broadcastIndex);
    } else if (!broadcastAddress.isEmpty()) {
        comboBoxBroadcast->addItem(broadcastAddress);
        comboBoxBroadcast->setCurrentText(broadcastAddress);
    }

    // Dinleme adreslerini seç
    for (int i = 0; i < listWidgetListen->count(); ++i) {
        QListWidgetItem *item = listWidgetListen->item(i);
        if (listenAddresses.contains(item->text())) {
            item->setCheckState(Qt::Checked);
        }
    }

    // Ses cihazı ayarları
    int inputDeviceIndex = settingsManager->getInputDeviceIndex();
    int outputDeviceIndex = settingsManager->getOutputDeviceIndex();

    if (inputDeviceIndex >= 0 && inputDeviceIndex < comboBoxInputDevice->count()) {
        comboBoxInputDevice->setCurrentIndex(inputDeviceIndex);
    }

    if (outputDeviceIndex >= 0 && outputDeviceIndex < comboBoxOutputDevice->count()) {
        comboBoxOutputDevice->setCurrentIndex(outputDeviceIndex);
    }
}

void MainWindow::saveSettings()
{
    // Multicast ayarları
    settingsManager->setBroadcastAddress(comboBoxBroadcast->currentText());

    QStringList listenAddresses;
    for (int i = 0; i < listWidgetListen->count(); ++i) {
        QListWidgetItem *item = listWidgetListen->item(i);
        if (item->checkState() == Qt::Checked) {
            listenAddresses.append(item->text());
        }
    }
    settingsManager->setListenAddresses(listenAddresses);

    // Ses cihazı ayarları
    settingsManager->setInputDeviceIndex(comboBoxInputDevice->currentIndex());
    settingsManager->setOutputDeviceIndex(comboBoxOutputDevice->currentIndex());

    // Ayarları kaydet
    settingsManager->saveSettings();
}

// UI olay işleyicileri
// MainWindow.cpp dosyasına eklenecek değişiklikler - onStartButtonClicked metodu

void MainWindow::onStartButtonClicked()
{
    if (isRunning) {
        return;
    }

    // Multicast ayarlarını al
    QString broadcastAddress = comboBoxBroadcast->currentText();

    QStringList listenAddresses;
    for (int i = 0; i < listWidgetListen->count(); ++i) {
        QListWidgetItem *item = listWidgetListen->item(i);
        if (item->checkState() == Qt::Checked) {
            listenAddresses.append(item->text());
        }
    }

    // Ayarları kontrol et
    if (broadcastAddress.isEmpty()) {
        QMessageBox::warning(this, "Hata", "Lütfen bir yayın adresi seçin.");
        return;
    }

    if (listenAddresses.isEmpty()) {
        QMessageBox::warning(this, "Hata", "Lütfen en az bir dinleme adresi seçin.");
        return;
    }

    // Network Manager'ı ayarla
    if (!networkManager->setBroadcastAddress(broadcastAddress)) {
        QMessageBox::critical(this, "Ağ Hatası", "Yayın adresi ayarlanamadı.");
        return;
    }

    for (const QString &address : listenAddresses) {
        if (!networkManager->addListenAddress(address)) {
            QMessageBox::warning(this, "Ağ Hatası",
                                 "Dinleme adresi eklenemedi: " + address);
        }
    }

    // Ses cihazlarını ayarla
    int inputDeviceIndex = comboBoxInputDevice->currentIndex();
    int outputDeviceIndex = comboBoxOutputDevice->currentIndex();

    if (!audioManager->setInputDevice(inputDeviceIndex)) {
        QMessageBox::critical(this, "Ses Hatası", "Mikrofon seçilemedi.");
        return;
    }

    if (!audioManager->setOutputDevice(outputDeviceIndex)) {
        QMessageBox::critical(this, "Ses Hatası", "Hoparlör seçilemedi.");
        return;
    }

    // ÖNEMLİ: Ses akışını başlat ve ÖNCE formatı belirle
    if (!audioManager->startAudioStream()) {
        QMessageBox::critical(this, "Ses Hatası", "Ses akışı başlatılamadı.");
        return;
    }

    // Şimdi codec manager'ı ayarla - ses akışı başladıktan SONRA yapılmalı
    codecManager->setSampleRate(audioManager->getSampleRate());
    codecManager->setChannels(audioManager->getInputChannels());

    if (!codecManager->initialize()) {
        audioManager->stopAudioStream();
        QMessageBox::critical(this, "Codec Hatası", "Ses kodlayıcı başlatılamadı.");
        return;
    }

    // En son ağ bağlantısını başlat
    if (!networkManager->startNetworking()) {
        audioManager->stopAudioStream();
        codecManager->terminate();
        QMessageBox::critical(this, "Ağ Hatası", "Ağ başlatılamadı.");
        return;
    }

    // UI durumunu güncelle
    isRunning = true;
    pushButtonStart->setEnabled(false);
    pushButtonStop->setEnabled(true);
    comboBoxBroadcast->setEnabled(false);
    listWidgetListen->setEnabled(false);
    comboBoxInputDevice->setEnabled(false);
    comboBoxOutputDevice->setEnabled(false);

    updateStatusBar("Çalışıyor - Yayın: " + broadcastAddress + ", Dinleme: " + listenAddresses.join(", "));
}

void MainWindow::onStopButtonClicked()
{
    if (!isRunning) {
        return;
    }

    // Ses akışını durdur
    audioManager->stopAudioStream();

    // Ağ bağlantısını durdur
    networkManager->stopNetworking();

    // UI durumunu güncelle
    isRunning = false;
    pushButtonStart->setEnabled(true);
    pushButtonStop->setEnabled(false);
    comboBoxBroadcast->setEnabled(true);
    listWidgetListen->setEnabled(true);
    comboBoxInputDevice->setEnabled(true);
    comboBoxOutputDevice->setEnabled(true);

    updateStatusBar("Durduruldu");
}

void MainWindow::onMuteButtonToggled(bool checked)
{
    audioManager->setMicrophoneMuted(checked);
    pushButtonMute->setText(checked ? "Sessiz (Aktif)" : "Sessiz");
    updateStatusBar(checked ? "Mikrofon sessizleştirildi" : "Mikrofon aktif");
}

// Kullanılmayan parametre uyarılarını kaldıralım
void MainWindow::onBroadcastAddressChanged(const QString & /* address */)
{
    // Broadcast adresi değiştiğinde yapılacak işlemler
    // Şu anda herhangi bir özel işlem yapmıyoruz
}

void MainWindow::onListenAddressesChanged()
{
    // Dinleme adresleri değiştiğinde yapılacak işlemler
    // Şu anda herhangi bir özel işlem yapmıyoruz
}

void MainWindow::onInputDeviceChanged(int index)
{
    // Giriş cihazı seçimine tepki olarak setInputDevice'ı çağır
    // Bu, ses kanallarını otomatik olarak ayarlayacak
    if (index >= 0 && !isRunning) {
        audioManager->setInputDevice(index);
    }
}

void MainWindow::onOutputDeviceChanged(int index)
{
    // Çıkış cihazı seçimine tepki olarak setOutputDevice'ı çağır
    // Bu, ses kanallarını otomatik olarak ayarlayacak
    if (index >= 0 && !isRunning) {
        audioManager->setOutputDevice(index);
    }
}

// Hata işleyicileri
void MainWindow::handleAudioError(const QString &error)
{
    QMessageBox::critical(this, "Ses Hatası", error);
    if (isRunning) {
        onStopButtonClicked();
    }
}

void MainWindow::handleCodecError(const QString &error)
{
    QMessageBox::critical(this, "Codec Hatası", error);
    if (isRunning) {
        onStopButtonClicked();
    }
}

void MainWindow::handleNetworkError(const QString &error)
{
    QMessageBox::critical(this, "Ağ Hatası", error);
    if (isRunning) {
        onStopButtonClicked();
    }
}

// Durum çubuğu güncellemesi
void MainWindow::updateStatusBar(const QString &message)
{
    statusBar()->showMessage(message);
}
