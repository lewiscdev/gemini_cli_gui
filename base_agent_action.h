/**
 * @file base_agent_action.h
 * @brief Defines the abstract base class for all local agent tools.
 *
 * This file establishes the Command Pattern interface for agent capabilities.
 * By inheriting from this class, individual tools (like writing files or 
 * uploading via FTP) can be entirely decoupled from the core UI and networking logic.
 */

#ifndef BASE_AGENT_ACTION_H
#define BASE_AGENT_ACTION_H

#include <QObject>
#include <QString>

/**
 * @brief Data structure representing a tool execution request from the LLM.
 */
struct AgentCommand {
    QString action;   ///< The name of the tool/function to execute (e.g., "write_file")
    QString target;   ///< The file path, URL, or terminal command string
    QString payload;  ///< Optional data payload (e.g., file contents to write, JSON args)
};

class BaseAgentAction : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs the base action.
     * @param parent The parent QObject, necessary for memory management.
     */
    explicit BaseAgentAction(QObject* parent = nullptr) : QObject(parent) {}

    /**
     * @brief Virtual destructor to ensure proper cleanup of derived classes.
     */
    virtual ~BaseAgentAction() = default;

    /**
     * @brief Retrieves the string identifier for this tool.
     * @return The exact function name expected by the LLM (e.g., "write_file").
     */
    [[nodiscard]] virtual QString getName() const = 0;

    /**
     * @brief Executes the specific tool logic.
     * @param command The parsed arguments requested by the agent.
     * @param workspacePath The absolute path to the active sandboxed directory.
     */
    virtual void execute(const AgentCommand& command, const QString& workspacePath) = 0;

signals:
    /**
     * @brief Emitted when the tool finishes executing, whether synchronously or asynchronously.
     * @param systemFeedback The stdout, stderr, or system confirmation string to send back to the LLM.
     */
    void actionFinished(const QString& systemFeedback);
};

#endif // BASE_AGENT_ACTION_H