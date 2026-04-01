/**
 * @file write_file_action.cpp
 * @brief Implementation of the write file tool.
 *
 * Physically writes the agent's generated code or text to the local disk,
 * ensuring the operation succeeds and returning the system feedback.
 */

#include "write_file_action.h"

#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>

WriteFileAction::WriteFileAction(QObject* parent) : BaseAgentAction(parent) {}

QString WriteFileAction::getName() const {
    return "write_file";
}

void WriteFileAction::execute(const AgentCommand& command, const QString& workspacePath) {
    // command.target already contains the absolute, verified path from the ui routing
    QString targetPath = command.target;
    QString content = command.payload;

    // ensure the parent directories exist before attempting to write the file
    QFileInfo fileInfo(targetPath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QFile file(targetPath);
    
    // open the file in write-only text mode, truncating any existing content
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit actionFinished("[system error: failed to open file for writing at " + targetPath + "]");
        return;
    }

    // stream the exact payload to the disk
    QTextStream out(&file);
    out << content;
    file.close();

    // notify the core system that the operation completed successfully
    emit actionFinished("[system success: file written successfully to " + fileInfo.fileName() + "]");
}