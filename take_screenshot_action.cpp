/**
 * @file take_screenshot_action.cpp
 * @brief Implementation of the screenshot tool.
 *
 * Uses QScreen and Windows API calls to capture application UI, prompts
 * the user for security clearance, and writes the image to the workspace.
 */

#include "take_screenshot_action.h"
#include "execute_shell_action.h"

#include <QGuiApplication>
#include <QScreen>
#include <QDir>
#include <QMessageBox>

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

TakeScreenshotAction::TakeScreenshotAction(ExecuteShellAction* shellAction, QObject* parent) 
    : BaseAgentAction(parent), linkedShellAction(shellAction) {}

QString TakeScreenshotAction::getName() const {
    return "take_screenshot";
}

QPixmap TakeScreenshotAction::captureProcessWindow(qint64 processId) {
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return QPixmap();

    QPixmap fullScreen = screen->grabWindow(0);

#ifdef Q_OS_WIN
    WindowSearchData searchData = { static_cast<DWORD>(processId), nullptr };
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&searchData));

    if (searchData.hwnd) {
        RECT rect;
        if (GetWindowRect(searchData.hwnd, &rect)) {
            int x = rect.left;
            int y = rect.top;
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;
            
            return fullScreen.copy(x, y, width, height);
        }
    }
#endif

    return fullScreen; 
}

void TakeScreenshotAction::execute(const AgentCommand& command, const QString& workspacePath) {
    // ask the shell action if it is currently running a background app
    qint64 pid = linkedShellAction->getActiveProcessId();
    
    if (pid == 0) {
        emit actionFinished("[system error: no active gui application is currently running to screenshot.]");
        return;
    }

    QPixmap croppedShot = captureProcessWindow(pid);
    
    // privacy intercept: explicitly show the user what the agent is about to see
    QMessageBox imgBox;
    imgBox.setWindowTitle("Security Intercept: Approve Image");
    imgBox.setText("The agent wants to send this image to the API. Do you approve?");
    imgBox.setIconPixmap(croppedShot.scaledToWidth(400, Qt::SmoothTransformation));
    imgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    
    if (imgBox.exec() == QMessageBox::Yes) {
        QString savePath = QDir(workspacePath).absoluteFilePath("agent_vision_capture.png");
        croppedShot.save(savePath, "PNG");
        
        emit actionFinished(QString("[system success: image saved locally as '%1'. please read the file to analyze the ui.]").arg("agent_vision_capture.png"));
    } else {
        emit actionFinished("[system error: the human user DENIED the screenshot request due to privacy concerns.]");
    }
}