Multicast Sesli İletişim Uygulaması
Bu uygulama, ağ üzerinden UDP multicast protokolü kullanarak gerçek zamanlı sesli iletişim sağlar.
Yüksek kaliteli ses iletimi için Opus codec kullanır ve ağ gecikmelerini yönetmek için jitter buffer sistemi içerir.

Gerekli Kütüphaneler

Qt Framework (5.15.0+): https://www.qt.io/download

PortAudio (v19+): http://www.portaudio.com/download.html

Opus Codec (1.3+): https://opus-codec.org/downloads/

ASIO (1.12.0+): https://think-async.com/Asio/Download.html

Donanım Gereksinimleri

Minimum Kulaklık/Mikrofon Özellikleri:

Örnekleme Hızı: 48kHz (Opus, 44.1kHz desteklemez)
Bit Derinliği: 16-bit
Frekans Yanıtı: 80Hz-15kHz (en az)
Kanal Sayısı: Mono veya Stereo (uygulamada genellikle mono kullanılır)
Optimum performans için:
Düşük gecikme özellikli kulaklık
Kardiyoid mikrofonlu kulaklık seti
Gürültü engelleme özelliği olan mikrofon

Proje Özellikleri

Ses Formatı: 48kHz, 16-bit (uygulama içinde int16 PCM format kullanılır)
Codec: Opus VOIP
Bit Hızı: 32kbps (mono), 64kbps (stereo)
Ağ Protokolü: UDP Multicast (IPv4)
Jitter Buffer: 50ms (varsayılan)

Kullanım

Ses Cihazlarını Seçin:

İstediğiniz mikrofon ve hoparlör/kulaklık cihazını seçin
Multicast Adreslerini Ayarlayın:
Yayın yapacağınız adresi girin (ör. 239.255.0.1)
Dinlemek istediğiniz adresleri işaretleyin
Bağlantıyı Başlatın:
"Başlat" düğmesine tıklayın
Durum çubuğunda bağlantı durumu görüntülenecektir

İletişim:

Mikrofonunuza konuşun
Diğer kullanıcıların sesi kulaklığınıza/hoparlörünüze iletilir
"Sessiz" düğmesi ile mikrofonunuzu kapatabilirsiniz
Bağlantıyı Sonlandırın:
"Durdur" düğmesine tıklayın

Proje Yapısı

AudioManager: Ses donanımı yönetimi (PortAudio)
CodecManager: Opus codec ile ses kodlama/çözme
NetworkManager: ASIO ile multicast ağ iletişimi
JitterBuffer: Ağ gecikmelerini telafi eden arabellek sistemi
SettingsManager: XML tabanlı ayar yönetimi
MainWindow: Qt kullanıcı arayüzü

Ayarlar

Tüm ayarlar settings.xml dosyasında saklanır:
Ses cihazı seçimleri
Multicast adresleri
Ses kalitesi parametreleri
Mikrofon durumu

Proje Dosyası (.pro) Yapılandırması
Proje aşağıdaki QMake yapılandırmasını kullanır:

QT += core gui widgets network
QT += xml
LIBS += -lws2_32
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG += c++17

Kaynak Dosyaları:

SOURCES += \
    audiomanager.cpp \
    codecmanager.cpp \
    debuglogger.cpp \
    main.cpp \
    mainwindow.cpp \
    networkmanager.cpp \
    settingsmanager.cpp

HEADERS += \
    JitterBuffer.h \
    audiomanager.h \
    codecmanager.h \
    debuglogger.h \
    mainwindow.h \
    networkmanager.h \
    settingsmanager.h

Harici Kütüphane Entegrasyonu:

# PortAudio kütüphanesi

LIBS += -L"C:/Users/M.Eren/Desktop/portaudio/build" -lportaudio
INCLUDEPATH += C:/Users/M.Eren/Desktop/portaudio/include
DEPENDPATH += C:/Users/M.Eren/Desktop/portaudio/include

# Opus codec kütüphanesi

INCLUDEPATH += C:/Users/M.Eren/Desktop/opus/include
LIBS += -L"C:/Users/M.Eren/Desktop/opus/.libs" -lopus

# ASIO kütüphanesi (header-only)

INCLUDEPATH += C:/Users/M.Eren/Desktop/asio-1.30.2/include
DEFINES += ASIO_STANDALONE

Kütüphanelerin Projeye Eklenmesi

1. Qt Modülleri:
   
   - 'QT += core gui widgets network xml' ile Qt'nin temel modülleri eklenir
   - core: Çekirdek QT bileşenleri
   - gui: Grafik kullanıcı arayüzü bileşenleri 
   - widgets: UI widget'ları
   - network: Ağ haberleşme bileşenleri
   - xml: XML dosya işleme bileşenleri

2. Windows Soket Kütüphanesi:
   
   - 'LIBS += -lws2_32' ile Windows ağ programlama için gerekli soket kütüphanesi eklenir

3. C++ Standardı:
   - 'CONFIG += c++17' ile C++17 standardı kullanılır

4. PortAudio Kütüphanesi:
   
   - LIBS parametresiyle kütüphane dosyasının konumu ve ismi belirtilir
   - INCLUDEPATH ile başlık dosyalarının konumu belirtilir
   - DEPENDPATH ile bağımlılık yolu belirtilir

5. Opus Codec Kütüphanesi:
   
   - INCLUDEPATH ile başlık dosyalarının konumu belirtilir
   - LIBS ile kütüphane dosyasının konumu ve ismi belirtilir

6. ASIO Kütüphanesi:
    
   - Sadece başlık dosyalarından oluşan (header-only) bir kütüphane olduğu için
     sadece INCLUDEPATH ile başlık dosyalarının konumu belirtilir
   - DEFINES += ASIO_STANDALONE ile ASIO'nun bağımsız modda çalışması sağlanır
     (Boost kütüphanesine bağımlılık olmadan)

Sorun Giderme

Ses Cihazları Görünmüyor: Windows Ses Denetim Masasında cihazlarınızı kontrol edin
Ses İletilmiyor: Güvenlik Duvarı ayarlarınızı kontrol edin
Ses Kalitesi Düşük: Daha iyi bir kulaklık kullanın, ağ bağlantınızı kontrol edin
Yüksek Gecikme: Kablolu kulaklık kullanın, ağ bağlantınızı iyileştirin
Mikrofon Sesi Sorunları: Windows ses ayarlarında mikrofon seviyesini düzenleyin
Derleme Hataları: Kütüphane yollarının doğru olduğundan emin olun

Bu uygulama, düşük gecikmeli multicast ses iletişim sisteminin temel yapıtaşlarını göstererek gerçek zamanlı sesli iletişim uygulamaları için iyi bir başlangıç noktası sunar.
