#include "core/mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    // Enable High DPI scaling
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);

    QApplication a(argc, argv);

    MainWindow w;
    w.show(); // Show the main window

    return a.exec(); // Start event loop
}