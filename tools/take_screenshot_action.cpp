/**
 * @file take_screenshot_action.cpp
 * @brief Implementation of the visual verification tool.
 *
 * Interfaces with the host os to capture the primary screen, resize 
 * the image buffer, and format the specialized html response required 
 * by the main window interceptor.
 */

#include "take_screenshot_action.h"
#include "execute_shell_action.h"

#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QDir>
#include <QFile>

// ============================================================================
// constructor
// ============================================================================

TakeScreenshotAction::TakeScreenshotAction(ExecuteShellAction* shellAction, QObject* parent) 
    : BaseAgentAction(parent), activeShell(shellAction) {}

// ============================================================================
// public interface
// ============================================================================

QString TakeScreenshotAction::getName() const {
    return "take_screenshot";
}

// ============================================================================
// execution logic
// ============================================================================

void TakeScreenshotAction::execute(const AgentCommand& command, const QString& workspacePath) {
    Q_UNUSED(command);

    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        emit actionFinished("System Error [take_screenshot]: Could not access the primary display.");
        return;
    }

    // capture the entire primary display buffer
    QPixmap originalPixmap = screen->grabWindow(0);
    
    // scale down the resolution to prevent massive base64 token payloads to google's api
    QPixmap scaledPixmap = originalPixmap.scaled(1280, 720, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // write to the expected hardcoded path in the sandbox
    QString fileName = "latest_agent_screenshot.png";
    QString filePath = QDir(workspacePath).absoluteFilePath(fileName);

    if (scaledPixmap.save(filePath, "PNG")) {
        // format the specific string the main window looks for to intercept and attach the file
        QString uiMessage = QString("System [take_screenshot]: Visual verification captured.<br><br>"
                                    "<img src=\"file:///%1\" width=\"400\" style=\"border-radius: 4px;\">")
                                    .arg(filePath);
        emit actionFinished(uiMessage);
    } else {
        emit actionFinished("System Error [take_screenshot]: Failed to save the image buffer to disk.");
    }
}