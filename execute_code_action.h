/**
 * @file execute_code_action.h
 * @brief Defines the local code execution sandbox tool.
 *
 * Encapsulates the logic for writing temporary source files, executing
 * them via local interpreters or compilers (python, node, g++), capturing
 * the terminal output, and safely cleaning up the workspace.
 */

#ifndef EXECUTE_CODE_ACTION_H
#define EXECUTE_CODE_ACTION_H

#include "base_agent_action.h"

class ExecuteCodeAction : public BaseAgentAction {
    Q_OBJECT

public:
    /**
     * @brief Constructs the code execution action.
     * @param parent The parent QObject.
     */
    explicit ExecuteCodeAction(QObject* parent = nullptr);

    /**
     * @brief Returns the strict api identifier for this tool.
     * @return The string "execute_code".
     */
    [[nodiscard]] QString getName() const override;

    /**
     * @brief Executes the raw source code in the specified language.
     * @param command The parsed request containing the language and raw code.
     * @param workspacePath The absolute path to the active sandboxed directory.
     */
    void execute(const AgentCommand& command, const QString& workspacePath) override;
};

#endif // EXECUTE_CODE_ACTION_H