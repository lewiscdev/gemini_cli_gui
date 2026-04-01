/**
 * @file write_file_action.h
 * @brief Defines the local file writing tool for the agent.
 *
 * Encapsulates the logic for securely writing text payloads to the
 * local file system within the sandboxed project workspace.
 */

#ifndef WRITE_FILE_ACTION_H
#define WRITE_FILE_ACTION_H

#include "base_agent_action.h"

class WriteFileAction : public BaseAgentAction {
    Q_OBJECT

public:
    /**
     * @brief Constructs the write file action.
     * @param parent The parent QObject.
     */
    explicit WriteFileAction(QObject* parent = nullptr);

    /**
     * @brief Returns the strict api identifier for this tool.
     * @return The string "write_file".
     */
    [[nodiscard]] QString getName() const override;

    /**
     * @brief Executes the file write operation.
     * @param command The parsed execution request containing the target path and text payload.
     * @param workspacePath The absolute path to the active sandboxed directory.
     */
    void execute(const AgentCommand& command, const QString& workspacePath) override;
};

#endif // WRITE_FILE_ACTION_H