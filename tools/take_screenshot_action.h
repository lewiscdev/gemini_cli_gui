/**
 * @file take_screenshot_action.h
 * @brief Defines the visual verification tool for the agent.
 *
 * Captures a screenshot of the primary display, scales it down to 
 * optimize token usage, and saves it to the sandbox for the main UI 
 * loop to attach to the next API payload.
 */

#ifndef TAKE_SCREENSHOT_ACTION_H
#define TAKE_SCREENSHOT_ACTION_H

#include "base_agent_action.h"

// forward declarations to reduce header bloat
class ExecuteShellAction;

class TakeScreenshotAction : public BaseAgentAction {
    Q_OBJECT

public:
    // ============================================================================
    // constructor
    // ============================================================================

    /**
     * @brief Constructs the screenshot action.
     * @param shellAction Pointer to the active shell action (to query active process state if needed).
     * @param parent The parent QObject.
     */
    explicit TakeScreenshotAction(ExecuteShellAction* shellAction, QObject* parent = nullptr);

    // ============================================================================
    // public interface
    // ============================================================================

    /**
     * @brief Returns the strict api identifier for this tool.
     * @return The string "take_screenshot".
     */
    [[nodiscard]] QString getName() const override;

    /**
     * @brief Executes the screen capture.
     * @param command The parsed execution request.
     * @param workspacePath The absolute path to the active sandboxed directory.
     */
    void execute(const AgentCommand& command, const QString& workspacePath) override;

private:
    // ============================================================================
    // internal state
    // ============================================================================

    ExecuteShellAction* activeShell; ///< reference to the shell manager to monitor running guis
};

#endif // TAKE_SCREENSHOT_ACTION_H