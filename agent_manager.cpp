#include "agent_manager.h"

AgentActionManager::AgentActionManager(QObject* parent) : QObject(parent) {
    // initialize with a strict zero-trust environment
    // we will add whitelist configurations via the gui later
}

void AgentActionManager::addWhitelistedAction(const QString& action) {
    if (!whitelist.contains(action)) {
        whitelist.append(action);
    }
}

void AgentActionManager::scanOutputForActions(const QString& rawOutput) {
    // 1. extract the structured data
    AgentCommand requestedCommand = extractTags(rawOutput);

    // 2. verify and route
    if (requestedCommand.isValid()) {
        if (whitelist.contains(requestedCommand.action)) {
            // tag found and permitted: pause the stream and ask user
            emit actionRequiresPermission(requestedCommand);
        } else {
            // tag found but denied: log silently or notify user
            emit cleanTextReady("<span style=\"color: orange;\">[system: agent attempted unauthorized action: " + requestedCommand.action + "]</span>");
        }
    }

    // 3. clean the output and send the conversational text to the gui
    QString userFacingText = stripTagsFromOutput(rawOutput);
    if (!userFacingText.trimmed().isEmpty()) {
        emit cleanTextReady(userFacingText);
    }
}

AgentCommand AgentActionManager::extractTags(const QString& text) {
    AgentCommand command;

    // define regular expressions to capture everything between the tags, including newlines
    QRegularExpression actionRegex("<action>(.*?)</action>", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression targetRegex("<target>(.*?)</target>", QRegularExpression::DotMatchesEverythingOption);
    QRegularExpression payloadRegex("<payload>(.*?)</payload>", QRegularExpression::DotMatchesEverythingOption);

    // execute the searches
    QRegularExpressionMatch actionMatch = actionRegex.match(text);
    if (actionMatch.hasMatch()) {
        command.action = actionMatch.captured(1).trimmed();
    }

    QRegularExpressionMatch targetMatch = targetRegex.match(text);
    if (targetMatch.hasMatch()) {
        command.target = targetMatch.captured(1).trimmed();
    }

    QRegularExpressionMatch payloadMatch = payloadRegex.match(text);
    if (payloadMatch.hasMatch()) {
        command.payload = payloadMatch.captured(1).trimmed(); // preserves internal formatting like code blocks
    }

    return command;
}

QString AgentActionManager::stripTagsFromOutput(const QString& text) {
    QString cleanText = text;
    
    // remove the xml blocks entirely so the chat display remains clean
    QRegularExpression fullBlockRegex("<action>.*?</action>|<target>.*?</target>|<payload>.*?</payload>", QRegularExpression::DotMatchesEverythingOption);
    cleanText.replace(fullBlockRegex, "");
    
    return cleanText;
}

void AgentActionManager::executeApprovedAction(const AgentCommand& command) {
    if (command.action == "write_file") {
        // instantiate a file object targeting the llm's requested path
        QFile file(command.target);
        
        // attempt to open the file in write-only mode, creating it if it doesn't exist
        // QIODevice::Text ensures cross-platform line ending compatibility
        if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&file);
            out << command.payload;
            file.close(); // strictly enforce memory cleanup
            
            // emit a success message back to the ui
            emit cleanTextReady("<span style=\"color: green;\">[system: file written successfully to " + command.target + "]</span>");
        } else {
            // emit an error if the os denies write access (e.g., restricted directory)
            emit cleanTextReady("<span style=\"color: red;\">[system: os denied write access to " + command.target + "]</span>");
        }
    }
    // future actions like 'read_file' or 'execute_script' would be added as else-if blocks here
}