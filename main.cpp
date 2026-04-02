/**
 * @file main.cpp
 * @brief Application entry point for the Gemini Native Agent.
 *
 * This file initializes the core Qt application framework, instantiates
 * the primary GUI window, and hands over control to the Qt event loop.
 */

#include <QApplication>
#include "main_window.h"

int main(int argc, char *argv[]) {
    // initialize the qt application object required for gui and event handling
    QApplication app(argc, argv);
    
    // persistent os registry setup for saving api keys and settings
    QApplication::setOrganizationName("GeminiNativeAgent");
    QApplication::setOrganizationDomain("gemininativeagent.local");
    QApplication::setApplicationName("GeminiNativeAgent");

    // create and display the primary application window
    MainWindow mainWindow;
    mainWindow.show();
    
    // execute the application's main event loop and block until exit
    return app.exec();
}