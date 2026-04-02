/**
 * @file agent_manager.h
 * @brief Manages the execution of local machine actions requested by the agent.
 *
 * Utilizes the Command Pattern to route agent requests to isolated tool classes.
 * Maintains a registry of available capabilities, parses json arguments, 
 * enforces security whitelists, and manages human-in-the-loop approvals.
 */

#ifndef AGENT_MANAGER_H
#define AGENT_MANAGER_H

#include <QObject>
#include <QString>
#include <QSet>
#include <QMap>
#include <QJsonObject>

// forward declarations to reduce header bloat
struct AgentCommand;
class BaseAgentAction;

class AgentActionManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs the agent action manager.
     * @param parent The parent QObject, typically the main window.
     */
    explicit AgentActionManager(QObject* parent = nullptr);

    /**
     * @brief Safely cleans up the command registry.
     */
    ~AgentActionManager() override;

    /**
     * @brief Adds an action to the safe-list of non-destructive capabilities.
     * @param action The function name to whitelist (e.g., "write_file").
     */
    void addWhitelistedAction(const QString& action);

    /**
     * @brief Checks if a requested action is in the approved whitelist.
     * @param action The function name to verify.
     * @return True if the action is safely whitelisted, false otherwise.
     */
    bool isActionWhitelisted(const QString& action) const;

    /**
     * @brief Routes the validated command to its isolated tool class for execution.
     * @param command The fully populated agent command struct.
     * @param workspacePath The absolute path to the active sandboxed directory.
     */
    void executeApprovedAction(const AgentCommand& command, const QString& workspacePath);

public slots:
    /**
     * @brief The main gateway for all api tool requests. Parses args and triggers security.
     * @param functionName The exact string name of the tool requested by the llm.
     * @param arguments The json payload containing the tool parameters.
     * @param workspacePath The absolute path to the active sandboxed directory.
     */
    void processFunctionCall(const QString& functionName, const QJsonObject& arguments, const QString& workspacePath);

signals:
    /**
     * @brief Emitted when a system message or tool output is ready for the ui.
     * @param text The formatted string to display.
     */
    void cleanTextReady(const QString& text);

private:
    // ============================================================================
    // internal state
    // ============================================================================
    QSet<QString> whitelistedActions;         ///< collection of pre-approved safe actions
    QMap<QString, BaseAgentAction*> registry; ///< dynamic map of registered tool classes

    // ============================================================================
    // private helper methods
    // ============================================================================
    
    /**
     * @brief Internal helper to register a new tool class to the routing engine.
     * @param action Pointer to the instantiated tool class.
     */
    void registerAction(BaseAgentAction* action);

    /**
     * @brief Security helper to ensure file operations never escape the project workspace.
     * @param relativeTarget The path provided by the agent.
     * @param workspacePath The absolute path to the active sandboxed directory.
     * @return The absolute, verified path, or an empty string if a traversal attack is detected.
     */
    [[nodiscard]] QString resolveAndVerifyPath(const QString& relativeTarget, const QString& workspacePath) const;

    /**
     * @brief Triggers the human-in-the-loop gui modal for destructive actions.
     * @param command The fully parsed agent command.
     * @param workspacePath The absolute path to the active sandboxed directory.
     */
    void handleSecurityIntercept(const AgentCommand& command, const QString& workspacePath);
};

#endif // AGENT_MANAGER_H