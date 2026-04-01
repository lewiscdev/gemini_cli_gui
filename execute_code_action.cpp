/**
 * @file execute_code_action.cpp
 * @brief Implementation of the code execution sandbox.
 *
 * Handles the two-step compilation process for c++, enforces execution
 * timeouts to prevent infinite loops, and securely wipes temporary files.
 */

#include "execute_code_action.h"

#include <QProcess>
#include <QFile>
#include <QDir>

ExecuteCodeAction::ExecuteCodeAction(QObject* parent) : BaseAgentAction(parent) {}

QString ExecuteCodeAction::getName() const {
    return "execute_code";
}

void ExecuteCodeAction::execute(const AgentCommand& command, const QString& workspacePath) {
    QString language = command.target.toLower();
    QString sourceCode = command.payload;
    
    QString tempFileName;
    QString execCommand;
    QStringList execArgs;

    // determine the appropriate environment and setup temporary filenames
    if (language == "python") {
        tempFileName = "temp_agent_script.py";
        execCommand = "python"; 
        execArgs << tempFileName;
    } else if (language == "javascript" || language == "node") {
        tempFileName = "temp_agent_script.js";
        execCommand = "node";   
        execArgs << tempFileName;
    } else if (language == "cpp" || language == "c++") {
        tempFileName = "temp_agent_script.cpp";
        execCommand = "g++";    
        execArgs << tempFileName << "-o" << "temp_agent_exec.exe";
    } else {
        emit actionFinished("[system error: unsupported language '" + language + "'.]");
        return;
    }

    // construct the absolute path for the temporary file within the sandbox
    QDir workspaceDir(workspacePath);
    QString absoluteTempPath = workspaceDir.absoluteFilePath(tempFileName);

    // silently write the raw code to the temporary file
    QFile file(absoluteTempPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit actionFinished("[system error: failed to write temporary code file.]");
        return;
    }
    file.write(sourceCode.toUtf8());
    file.close();

    QProcess process;
    process.setWorkingDirectory(workspacePath);
    process.setProcessChannelMode(QProcess::MergedChannels); // merge stdout and stderr

    // handle execution based on compiled vs interpreted languages
    if (language == "cpp" || language == "c++") {
        // step 1: compile the c++ code
        process.start(execCommand, execArgs);
        process.waitForFinished(10000); 

        if (process.exitCode() != 0) {
            QString compileError = process.readAll();
            QFile::remove(absoluteTempPath); // clean up source
            emit actionFinished("[system error: compilation failed]\n" + compileError);
            return;
        }

        // step 2: run the compiled executable
        QString execName = workspaceDir.absoluteFilePath("temp_agent_exec.exe");
        process.start(execName, QStringList());
        process.waitForFinished(10000); // 10 second timeout protection
    } else {
        // step 1: run the interpreted code directly
        process.start(execCommand, execArgs);
        process.waitForFinished(10000); // 10 second timeout protection
    }

    // capture output and handle timeouts
    QString output = process.readAll();
    if (process.error() == QProcess::Timedout) {
        output = "[system error: execution timed out after 10 seconds. infinite loop detected?]";
        process.kill();
    } else if (output.isEmpty()) {
        output = "[system: execution completed successfully with no console output.]";
    } else {
        output = "[system output]:\n" + output;
    }

    // meticulously clean up temporary files to preserve the workspace
    QFile::remove(absoluteTempPath);
    if (language == "cpp" || language == "c++") {
        QFile::remove(workspaceDir.absoluteFilePath("temp_agent_exec.exe"));
    }

    // pipe the results back to the core system
    emit actionFinished(output.trimmed());
}