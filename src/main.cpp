#include "core/mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.setWindowTitle("Profile Orchestrator (Qt 5)");
    w.show();
    return a.exec();
}