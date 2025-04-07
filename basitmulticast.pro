QT += core gui widgets network
LIBS += -lws2_32
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    audiomanager.cpp \
    codecmanager.cpp \
    main.cpp \
    mainwindow.cpp \
    networkmanager.cpp \
    settingsmanager.cpp

HEADERS += \
    audiomanager.h \
    codecmanager.h \
    mainwindow.h \
    networkmanager.h \
    settingsmanager.h

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

LIBS += -L"C:/Users/M.Eren/Desktop/portaudio/build" -lportaudio
INCLUDEPATH += C:/Users/M.Eren/Desktop/portaudio/include
DEPENDPATH += C:/Users/M.Eren/Desktop/portaudio/include

INCLUDEPATH += C:/Users/M.Eren/Desktop/opus/include
LIBS += -L"C:/Users/M.Eren/Desktop/opus/.libs" -lopus

INCLUDEPATH += C:/Users/M.Eren/Desktop/asio-1.30.2/include

DISTFILES +=

# Add this for ASIO
DEFINES += ASIO_STANDALONE
