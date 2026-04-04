/**
 * @file take_screenshot_action.cpp
 * @brief Implementation of the visual verification tool.
 *
 * Interfaces with the host os to capture the primary screen, optionally crops 
 * to the active background process window, prompts the user for security 
 * clearance, and formats the specialized html response for the main window.
 */

#include "take_screenshot_action.h"
#include "execute_shell_action.h"

#include <QGuiApplication>
#include <QScreen>
#include <QPixmap>
#include <QDir>
#include <QFile>
#include <QMessageBox>
#include <QWidget>

// include windows api for precise window cropping during screenshots
#ifdef Q_OS_WIN
#include <windows.h>

struct WindowSearchData {
    DWORD pid;
    HWND hwnd;
};

// callback to locate the specific os window owned by the agent's active process
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    WindowSearchData* data = reinterpret_cast<WindowSearchData*>(lParam);
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    
    // stop searching if we find a visible window belonging to the exact pid
    if (processId == data->pid && IsWindowVisible(hwnd)) {
        data->hwnd = hwnd;
        return FALSE; 
    }
    return TRUE;
}
#endif

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

    // ask the shell action for the pid of the background gui app
    qint64 pid = activeShell->getActiveProcessId();
    if (pid == 0) {
        emit actionFinished("System Error [take_screenshot]: No active GUI application is currently running to screenshot.");
        return;
    }

    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        emit actionFinished("System Error [take_screenshot]: Could not access the primary display.");
        return;
    }

    // capture the entire primary display buffer
    QPixmap fullScreen = screen->grabWindow(0);
    QPixmap targetPixmap = fullScreen;

    // --- winapi cropper restoration ---
#ifdef Q_OS_WIN
    WindowSearchData searchData = { static_cast<DWORD>(pid), nullptr };
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&searchData));

    if (searchData.hwnd) {
        RECT rect;
        if (GetWindowRect(searchData.hwnd, &rect)) {
            int x = rect.left;
            int y = rect.top;
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;
            
            // successfully cropped to the specific window
            targetPixmap = fullScreen.copy(x, y, width, height); 
        }
    }
#endif

    // scale down the resolution to prevent massive base64 token payloads to google's api
    QPixmap scaledPixmap = targetPixmap.scaled(1280, 720, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    // --- privacy intercept restoration ---
    
    // cast the parent object to a qwidget so the messagebox centers perfectly over the main ui
    QWidget* parentWidget = qobject_cast<QWidget*>(parent());
    
    QMessageBox imgBox(parentWidget);
    imgBox.setWindowTitle("Security Intercept: Approve Image");
    imgBox.setText("The agent wants to attach this screenshot. Do you approve?");
    
    // show a small preview in the dialog box
    imgBox.setIconPixmap(scaledPixmap.scaledToWidth(400, Qt::SmoothTransformation));
    imgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    
    if (imgBox.exec() == QMessageBox::Yes) {
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
    } else {
        emit actionFinished("System Error [take_screenshot]: The human user DENIED the screenshot request due to privacy concerns.");
    }
}