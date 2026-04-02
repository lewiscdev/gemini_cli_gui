/**
 * @file git_manager_action.cpp
 * @brief Implementation of the git manager action.
 *
 * Parses a 2d json array of command blocks and executes them sequentially
 * using qprocess, halting the batch immediately if any step fails.
 */

#include "git_manager_action.h"

#include <QProcess>
#include <QJsonDocument>
#include <QJsonArray>

// ============================================================================
// constructor
// ============================================================================

GitManagerAction::GitManagerAction(QObject* parent) : BaseAgentAction(parent) {}

// ============================================================================
// public interface
// ============================================================================

QString GitManagerAction::getName() const {
    return "git_manager";
}

// ============================================================================
// execution logic
// ============================================================================

void GitManagerAction::execute(const AgentCommand& command, const QString& workspacePath) {
    // decode the 2d json array payload
    QJsonArray batchArray = QJsonDocument::fromJson(command.payload.toUtf8()).array();
    
    if (batchArray.isEmpty()) {
        emit actionFinished("System Error [git_manager]: No commands provided in batch.");
        return;
    }

    QString finalOutput;

    // loop through each command block in the batch
    for (int i = 0; i < batchArray.size(); ++i) {
        QJsonArray argsArray = batchArray[i].toArray();
        QStringList arguments;
        
        for (const auto& arg : argsArray) {
            arguments << arg.toString();
        }

        if (arguments.isEmpty()) continue;

        finalOutput += QString(">> git %1\n").arg(arguments.join(" "));

        // execute sequentially
        QProcess gitProcess;
        gitProcess.setWorkingDirectory(workspacePath);
        gitProcess.start("git", arguments);

        if (!gitProcess.waitForFinished(10000)) {
            gitProcess.kill();
            finalOutput += "ERROR: Process timed out. Aborting remaining batch.\n";
            break;
        }

        QString stdOut = QString::fromUtf8(gitProcess.readAllStandardOutput()).trimmed();
        QString stdErr = QString::fromUtf8(gitProcess.readAllStandardError()).trimmed();

        if (!stdOut.isEmpty()) finalOutput += stdOut + "\n";
        
        // git puts successful messages in stderr often
        if (!stdErr.isEmpty()) finalOutput += stdErr + "\n"; 

        // if the process failed fundamentally, stop the batch to be safe
        if (gitProcess.exitCode() != 0 && gitProcess.exitStatus() == QProcess::NormalExit) {
             finalOutput += "Command exited with error code. Aborting remaining batch.\n";
             break;
        }
        
        // space between commands
        finalOutput += "\n"; 
    }

    emit actionFinished(QString("System [git_manager batch]:\n%1").arg(finalOutput.trimmed()));
}