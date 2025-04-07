#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCoreApplication::setOrganizationName("MulticastVoiceApp");
    QCoreApplication::setApplicationName("MulticastVoiceApp");

    MainWindow w;
    w.show();

    return a.exec();
}
