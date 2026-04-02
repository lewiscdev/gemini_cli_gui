/**
 * @file agent_manager.cpp
 * @brief Implementation of the local agent execution manager.
 *
 * Initializes the command registry, parses incoming api arguments, 
 * enforces the security intercept modal, and routes execution requests 
 * to their respective isolated tool classes.
 */

#include "agent_manager.h"

#include "write_file_action.h"
#include "ftp_upload_action.h"
#include "execute_shell_action.h"
#include "take_screenshot_action.h"
#include "git_manager_action.h"

#include <QMessageBox>
#include <QWidget>
#include <QDir>
#include <QFile>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>

// ============================================================================
// constructor and initialization
// ============================================================================

AgentActionManager::AgentActionManager(QObject* parent) : QObject(parent) {
    registerAction(new WriteFileAction(this));
    registerAction(new FtpUploadAction(this));
    
    ExecuteShellAction* shellAction = new ExecuteShellAction(this);
    registerAction(shellAction);
    registerAction(new TakeScreenshotAction(shellAction, this));
    registerAction(new GitManagerAction(this));
    
    addWhitelistedAction("take_screenshot");
}

AgentActionManager::~AgentActionManager() {}

// ============================================================================
// registry and security lists
// ============================================================================

void AgentActionManager::registerAction(BaseAgentAction* action) {
    if (action) {
        registry.insert(action->getName(), action);
        connect(action, &BaseAgentAction::actionFinished, this, &AgentActionManager::cleanTextReady);
    }
}

void AgentActionManager::addWhitelistedAction(const QString& action) {
    whitelistedActions.insert(action);
}

bool AgentActionManager::isActionWhitelisted(const QString& action) const {
    return whitelistedActions.contains(action);
}

// ============================================================================
// security and path validation
// ============================================================================

QString AgentActionManager::resolveAndVerifyPath(const QString& relativeTarget, const QString& workspacePath) const {
    if (workspacePath.isEmpty()) return "";

    QDir workspaceDir(workspacePath);
    QString absolutePath = QDir::cleanPath(workspaceDir.absoluteFilePath(relativeTarget));

    // block directory traversal attacks
    if (!absolutePath.startsWith(workspaceDir.absolutePath())) {
        return ""; 
    }
    return absolutePath;
}

// ============================================================================
// api routing and argument parsing
// ============================================================================

void AgentActionManager::processFunctionCall(const QString& functionName, const QJsonObject& arguments, const QString& workspacePath) {
    emit cleanTextReady(QString("UI_ONLY:<span style=\"color: orange;\"><i>[Agent requested tool execution: %1]</i></span>").arg(functionName));
    AgentCommand command;
    command.action = functionName;
    command.rationale = arguments["rationale"].toString();

    // map arguments for the command pattern registry
    if (functionName == "write_file") {
        QString absoluteTarget = resolveAndVerifyPath(arguments["target"].toString(), workspacePath);
        if (absoluteTarget.isEmpty()) {
            emit cleanTextReady("System Error: Security violation. Path is outside the workspace.");
            return;
        }
        command.target = absoluteTarget;
        command.payload = arguments["payload"].toString();
    }
    else if (functionName == "git_manager") {
        command.target = "Git Repository (Batch)";
        command.payload = QJsonDocument(arguments["command_blocks"].toArray()).toJson(QJsonDocument::Compact);
    }
    else if (functionName == "execute_shell_command") {
        command.target = arguments["command"].toString(); 
    }
    else if (functionName == "upload_ftp") {
        QString localPath = resolveAndVerifyPath(arguments["local_path"].toString(), workspacePath);
        if (localPath.isEmpty()) {
            emit cleanTextReady("System Error: Local file path is outside the workspace.");
            return;
        }
        command.target = localPath; 
        
        QJsonObject ftpArgs;
        ftpArgs["remote_url"] = arguments["remote_url"].toString();
        ftpArgs["username"] = arguments["username"].toString();
        ftpArgs["password"] = arguments["password"].toString();
        command.payload = QJsonDocument(ftpArgs).toJson(QJsonDocument::Compact);
    }
    else if (functionName == "take_screenshot") {
        command.target = "GUI Application";
    }
    
    // silent actions (no permission modal required, executed directly)
    else if (functionName == "read_file") {
        QString absoluteTarget = resolveAndVerifyPath(arguments["target"].toString(), workspacePath);
        QFile file(absoluteTarget);
        QString result;

        if (absoluteTarget.isEmpty() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            result = "System Error: File not found or access denied.";
        } else {
            result = QString("System [read_file]:\n%1").arg(QString(file.readAll()));
            file.close();
        }
        emit cleanTextReady(result);
        return;
    }
    else if (functionName == "list_directory") {
        QString absoluteTarget = resolveAndVerifyPath(arguments["target"].toString(), workspacePath);
        QString result;

        if (absoluteTarget.isEmpty()) {
            result = "System Error: Invalid directory.";
        } else {
            QDir dir(absoluteTarget);
            result = QString("System [list_directory]:\n%1").arg(dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).join("\n"));
        }
        emit cleanTextReady(result);
        return;
    }
    else if (functionName == "fetch_webpage") {
        QString urlStr = arguments["url"].toString();
        
        QNetworkAccessManager manager;
        QNetworkRequest request((QUrl(urlStr)));
        QNetworkReply* reply = manager.get(request);
        
        QEventLoop loop;
        connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
        loop.exec();
        
        QString result;
        if (reply->error() == QNetworkReply::NoError) {
            result = QString("System [fetch_webpage %1]:\n%2").arg(urlStr, QString::fromUtf8(reply->readAll()));
        } else {
            result = QString("System Error [fetch_webpage %1]: %2").arg(urlStr, reply->errorString());
        }
        
        reply->deleteLater();
        emit cleanTextReady(result);
        return;
    }

    // route to security interceptor or execute immediately if whitelisted
    if (isActionWhitelisted(functionName)) {
        executeApprovedAction(command, workspacePath);
    } else {
        handleSecurityIntercept(command, workspacePath);
    }
}

// ============================================================================
// human-in-the-loop security
// ============================================================================

void AgentActionManager::handleSecurityIntercept(const AgentCommand& command, const QString& workspacePath) {
    // cast the parent object to a qwidget so the messagebox centers perfectly over the main ui
    QWidget* parentWidget = qobject_cast<QWidget*>(parent());
    
    QMessageBox msgBox(parentWidget);
    msgBox.setWindowTitle("Agent Action Approval");

    QString htmlText = QString(
        "The agent wants to execute: <b>%1</b><br>"
        "Target: %2<br><br>"
        "<b>Reasoning:</b><br><i>%3</i><br><br>"
        "Do you allow this?"
    ).arg(command.action, command.target, command.rationale);

    msgBox.setText(htmlText);
    msgBox.setTextFormat(Qt::RichText);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes);

    if (msgBox.exec() == QMessageBox::Yes) {
        emit cleanTextReady(QString("UI_ONLY:<span style=\"color: green;\">[System: Approved %1 on %2...]</span>").arg(command.action, command.target));
        executeApprovedAction(command, workspacePath);
    } else {
        emit cleanTextReady(QString("UI_ONLY:<span style=\"color: red;\">[System: Denied %1 on %2]</span>").arg(command.action, command.target));
        emit cleanTextReady(QString("System Error: User denied permission to execute %1.").arg(command.action));
    }
}

// ============================================================================
// tool execution
// ============================================================================

void AgentActionManager::executeApprovedAction(const AgentCommand& command, const QString& workspacePath) {
    if (registry.contains(command.action)) {
        BaseAgentAction* tool = registry.value(command.action);
        tool->execute(command, workspacePath);
    } else {
        emit cleanTextReady("[system error: tool '" + command.action + "' is not registered in the execution engine.]");
    }
}