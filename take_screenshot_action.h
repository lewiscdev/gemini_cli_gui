/**
 * @file take_screenshot_action.h
 * @brief Defines the local gui screenshot tool.
 *
 * Encapsulates the OS-specific logic for capturing the screen or cropping
 * directly to the bounding box of a running background process.
 */

#ifndef TAKE_SCREENSHOT_ACTION_H
#define TAKE_SCREENSHOT_ACTION_H

#include "base_agent_action.h"
#include <QPixmap>

// forward declare the shell action so we can query it for active PIDs
class ExecuteShellAction; 

class TakeScreenshotAction : public BaseAgentAction {
    Q_OBJECT

public:
    /**
     * @brief Constructs the screenshot action.
     * @param shellAction Pointer to the shell action to retrieve active process IDs.
     * @param parent The parent QObject.
     */
    explicit TakeScreenshotAction(ExecuteShellAction* shellAction, QObject* parent = nullptr);

    /**
     * @brief Returns the strict api identifier for this tool.
     * @return The string "take_screenshot".
     */
    [[nodiscard]] QString getName() const override;

    /**
     * @brief Executes the screenshot and prompts the user for privacy approval.
     * @param command The parsed request from the agent.
     * @param workspacePath The absolute path to save the captured image.
     */
    void execute(const AgentCommand& command, const QString& workspacePath) override;

private:
    ExecuteShellAction* linkedShellAction; ///< reference to the engine running background tasks

    /**
     * @brief Os-native logic to crop a screenshot to the exact boundary of a running window.
     * @param processId The pid of the target application.
     * @return The cropped pixel map of the window.
     */
    QPixmap captureProcessWindow(qint64 processId);
};

#endif // TAKE_SCREENSHOT_ACTION_H