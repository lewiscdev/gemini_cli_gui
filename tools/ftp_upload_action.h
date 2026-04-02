/**
 * @file ftp_upload_action.h
 * @brief Defines the ftp file upload tool.
 *
 * Handles deploying local files from the sandboxed workspace 
 * directly to remote servers using the QNetworkAccessManager.
 */

#ifndef FTP_UPLOAD_ACTION_H
#define FTP_UPLOAD_ACTION_H

#include "base_agent_action.h"

// forward declarations to reduce header bloat
class QNetworkAccessManager;

class FtpUploadAction : public BaseAgentAction {
    Q_OBJECT

public:
    // ============================================================================
    // constructor
    // ============================================================================

    /**
     * @brief Constructs the ftp upload action.
     * @param parent The parent QObject.
     */
    explicit FtpUploadAction(QObject* parent = nullptr);

    // ============================================================================
    // public interface
    // ============================================================================

    /**
     * @brief Returns the strict api identifier for this tool.
     * @return The string "upload_ftp".
     */
    [[nodiscard]] QString getName() const override;

    /**
     * @brief Executes the ftp upload.
     * @param command The parsed request containing the local target and remote json credentials.
     * @param workspacePath The absolute path to the active sandboxed directory.
     */
    void execute(const AgentCommand& command, const QString& workspacePath) override;

private slots:
    // ============================================================================
    // network callbacks
    // ============================================================================

    /**
     * @brief Emits formatted progress updates to the ui.
     * @param bytesSent The number of bytes successfully transmitted.
     * @param bytesTotal The total size of the file being uploaded.
     */
    void onUploadProgress(qint64 bytesSent, qint64 bytesTotal);

private:
    // ============================================================================
    // internal state
    // ============================================================================

    QNetworkAccessManager* networkManager; ///< reusable manager for all ftp connections
    QString currentFileName;               ///< tracks the file currently being uploaded
};

#endif // FTP_UPLOAD_ACTION_H