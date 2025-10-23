#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    MainWindow w;
    // 이미지와 유사하게 타이틀 변경
    w.setWindowTitle("Profile Orchestrator (Qt 5)");
    w.show();
    
    return a.exec();
}

