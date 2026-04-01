/**
 * @file ftp_upload_action.h
 * @brief Defines the native FTP deployment tool for the agent.
 *
 * Encapsulates the network logic for connecting to an external server
 * and pushing local workspace files to live web environments.
 */

#ifndef FTP_UPLOAD_ACTION_H
#define FTP_UPLOAD_ACTION_H

#include "base_agent_action.h"

class FtpUploadAction : public BaseAgentAction {
    Q_OBJECT

public:
    /**
     * @brief Constructs the ftp upload action.
     * @param parent The parent QObject.
     */
    explicit FtpUploadAction(QObject* parent = nullptr);

    /**
     * @brief Returns the strict api identifier for this tool.
     * @return The string "upload_ftp".
     */
    [[nodiscard]] QString getName() const override;

    /**
     * @brief Executes the file upload over the network.
     * @param command The parsed request containing the local file path and remote url/credentials.
     * @param workspacePath The absolute path to the active sandboxed directory.
     */
    void execute(const AgentCommand& command, const QString& workspacePath) override;
};

#endif // FTP_UPLOAD_ACTION_H