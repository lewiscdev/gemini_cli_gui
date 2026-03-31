// update your main.cpp
#include <QApplication>
#include "main_window.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    // set global application identifiers for the QSettings registry
    QCoreApplication::setOrganizationName("LewisDevelopment");
    QCoreApplication::setApplicationName("GeminiNativeClient");

    MainWindow window;
    window.show();

    return app.exec();
}