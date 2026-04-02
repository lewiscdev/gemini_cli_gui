/**
 * @file write_file_action.cpp
 * @brief Implementation of the local file writing tool.
 *
 * Safely writes or overwrites text payloads to disk, ensuring that 
 * intermediate directory structures are automatically created if they 
 * do not already exist.
 */

#include "write_file_action.h"

#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QFileInfo>

// ============================================================================
// constructor
// ============================================================================

WriteFileAction::WriteFileAction(QObject* parent) : BaseAgentAction(parent) {}

// ============================================================================
// public interface
// ============================================================================

QString WriteFileAction::getName() const {
    return "write_file";
}

// ============================================================================
// execution logic
// ============================================================================

void WriteFileAction::execute(const AgentCommand& command, const QString& workspacePath) {
    Q_UNUSED(workspacePath); 

    QString targetPath = command.target;
    QString fileContent = command.payload;

    if (targetPath.isEmpty()) {
        emit actionFinished("System Error [write_file]: Target path is empty.");
        return;
    }

    QFileInfo fileInfo(targetPath);
    QDir dir = fileInfo.absoluteDir();

    // create parent directories if they do not exist
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            emit actionFinished(QString("System Error [write_file]: Could not create directory structure for %1.").arg(targetPath));
            return;
        }
    }

    QFile file(targetPath);
    
    // open the file in write-only mode, completely overwriting previous contents
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << fileContent;
        file.close();

        emit actionFinished(QString("System [write_file]: Successfully wrote to %1").arg(fileInfo.fileName()));
    } else {
        emit actionFinished(QString("System Error [write_file]: Access denied or unable to open %1 for writing.").arg(targetPath));
    }
}