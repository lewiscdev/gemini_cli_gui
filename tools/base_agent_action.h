/**
 * @file base_agent_action.h
 * @brief Abstract base class for all agent execution tools.
 *
 * This file defines the command data structure and the polymorphic 
 * interface used by the action manager to route native requests safely 
 * to their isolated capability classes.
 */

#ifndef BASE_AGENT_ACTION_H
#define BASE_AGENT_ACTION_H

#include <QObject>
#include <QString>

// ============================================================================
// data structures
// ============================================================================

/**
 * @brief Data structure representing a fully parsed tool execution request.
 */
struct AgentCommand {
    QString action;     ///< the strict string identifier of the requested tool
    QString target;     ///< the primary argument (e.g., file path, shell command)
    QString payload;    ///< the secondary argument (e.g., file contents, json payload)
    QString rationale;  ///< the model's reasoning for executing this tool
};

// ============================================================================
// interface definition
// ============================================================================

class BaseAgentAction : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs the base agent action.
     * @param parent The parent QObject, typically the agent manager.
     */
    explicit BaseAgentAction(QObject* parent = nullptr) : QObject(parent) {}
    
    /**
     * @brief Virtual destructor to ensure safe cleanup of derived action classes.
     */
    virtual ~BaseAgentAction() = default;

    /**
     * @brief Retrieves the strict string identifier of the tool.
     * @return The exact function name expected by the LLM (e.g., "write_file").
     */
    virtual QString getName() const = 0;

    /**
     * @brief Executes the native action using the provided arguments.
     * @param command The fully parsed execution request.
     * @param workspacePath The absolute path to the active sandboxed directory.
     */
    virtual void execute(const AgentCommand& command, const QString& workspacePath) = 0;

signals:
    // ============================================================================
    // execution callbacks
    // ============================================================================

    /**
     * @brief Emitted when the tool finishes execution (success or failure).
     * @param result The formatted standard output, standard error, or system confirmation.
     */
    void actionFinished(const QString& result);
};

#endif // BASE_AGENT_ACTION_H