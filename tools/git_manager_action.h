/**
 * @file git_manager_action.h
 * @brief Autonomous tool for the llm to interact with git version control.
 *
 * This class handles batched git commands, allowing the agent to perform
 * complex multi-step version control workflows (like add, commit, push)
 * in a single execution cycle to prevent intermediate state failures.
 */

#ifndef GIT_MANAGER_ACTION_H
#define GIT_MANAGER_ACTION_H

#include "base_agent_action.h"

class GitManagerAction : public BaseAgentAction {
    Q_OBJECT

public:
    // ============================================================================
    // constructor
    // ============================================================================

    /**
     * @brief Constructs the git manager action.
     * @param parent The parent QObject.
     */
    explicit GitManagerAction(QObject* parent = nullptr);

    // ============================================================================
    // public interface
    // ============================================================================

    /**
     * @brief Returns the strict api identifier for this tool.
     * @return The string "git_manager".
     */
    [[nodiscard]] QString getName() const override;

    /**
     * @brief Executes a batch of git commands sequentially.
     * @param command The parsed request containing the json array of git arguments.
     * @param workspacePath The absolute path to the active sandboxed directory.
     */
    void execute(const AgentCommand& command, const QString& workspacePath) override;
};

#endif // GIT_MANAGER_ACTION_H