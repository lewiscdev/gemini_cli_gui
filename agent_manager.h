#ifndef AGENT_MANAGER_H
#define AGENT_MANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QRegularExpression>
#include <QFile>
#include <QTextStream>

// structure to hold the parsed multi-variable command
struct AgentCommand {
    QString action;
    QString target;
    QString payload;
    
    // helper method to check if an action was actually found
    bool isValid() const {
        return !action.isEmpty();
    }
};

// parses standard output for xml tags and manages the security whitelist
class AgentActionManager : public QObject {
    Q_OBJECT

public:
    explicit AgentActionManager(QObject* parent = nullptr);

    void addWhitelistedAction(const QString& action);
    
    // new method to handle the physical os-level execution
    void executeApprovedAction(const AgentCommand& command);

public slots:
    // receives the raw text stream from the process controller
    void scanOutputForActions(const QString& rawOutput);

signals:
    // emitted when a valid, whitelisted command is detected
    void actionRequiresPermission(const AgentCommand& command);
    
    // emitted to pass standard text to the ui, stripping out the hidden xml tags
    void cleanTextReady(const QString& cleanText);

private:
    QStringList whitelist;
    
    // internal extraction logic
    AgentCommand extractTags(const QString& text);
    
    // removes the xml blocks so the user doesn't see them in the chat window
    QString stripTagsFromOutput(const QString& text);
};

#endif // AGENT_MANAGER_H