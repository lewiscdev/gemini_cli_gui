/**
 * @file execute_shell_action.h
 * @brief Defines the local terminal/shell execution tool.
 *
 * Encapsulates the logic for spawning background processes, injecting
 * environment variables (like GitHub PATs), and capturing stdout/stderr
 * to feed back to the agent.
 */

#ifndef EXECUTE_SHELL_ACTION_H
#define EXECUTE_SHELL_ACTION_H

#include "base_agent_action.h"
#include <QProcess>

class ExecuteShellAction : public BaseAgentAction {
    Q_OBJECT

public:
    /**
     * @brief Constructs the shell execution action.
     * @param parent The parent QObject.
     */
    explicit ExecuteShellAction(QObject* parent = nullptr);

    /**
     * @brief Safely terminates any persistent shell processes.
     */
    ~ExecuteShellAction() override;

    /**
     * @brief Returns the strict api identifier for this tool.
     * @return The string "execute_shell_command".
     */
    [[nodiscard]] QString getName() const override;

    /**
     * @brief Executes the terminal command.
     * @param command The parsed request containing the shell string.
     * @param workspacePath The absolute path to the active sandboxed directory.
     */
    void execute(const AgentCommand& command, const QString& workspacePath) override;

    /**
     * @brief Retrieves the PID of the currently running background process.
     * @return The process ID, or 0 if no process is running.
     */
    [[nodiscard]] qint64 getActiveProcessId() const;

private:
    QProcess* agentProcess; ///< persistent process to allow background server execution
};

#endif // EXECUTE_SHELL_ACTION_H