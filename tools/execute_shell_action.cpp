/**
 * @file execute_shell_action.cpp
 * @brief Implementation of the local shell execution tool.
 *
 * Manages the QProcess lifecycle, dynamically routing commands to cmd.exe 
 * or /bin/sh based on the host OS, and formatting the console output.
 */

#include "execute_shell_action.h"

#include <QSettings>
#include <QProcessEnvironment>
#include <QCoreApplication>
#include <QDir>
#include <QProcess>

// ============================================================================
// constructor and initialization
// ============================================================================

ExecuteShellAction::ExecuteShellAction(QObject* parent) : BaseAgentAction(parent) {
    agentProcess = new QProcess(this);
}

ExecuteShellAction::~ExecuteShellAction() {
    if (agentProcess->state() == QProcess::Running) {
        agentProcess->terminate();
        agentProcess->waitForFinished(2000);
    }
}

// ============================================================================
// public interface
// ============================================================================

QString ExecuteShellAction::getName() const {
    return "execute_shell_command";
}

qint64 ExecuteShellAction::getActiveProcessId() const {
    return agentProcess->processId();
}

// ============================================================================
// execution logic
// ============================================================================

void ExecuteShellAction::execute(const AgentCommand& command, const QString& workspacePath) {
    // terminate previous long-running tasks before starting a new one
    if (!agentProcess) {
        agentProcess = new QProcess(this);
    } else if (agentProcess->state() != QProcess::NotRunning) {
        agentProcess->kill();
        agentProcess->waitForFinished();
    }

    // securely sandbox the execution to the project directory
    agentProcess->setWorkingDirectory(workspacePath); 

    // inject global credentials into the environment for silent authentication
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    
    QString appDir = QCoreApplication::applicationDirPath();
    
    // build native paths to our bundled tools
    QString mingwPath = QDir::toNativeSeparators(QDir(appDir).filePath("runtimes/mingw/bin"));
    QString pythonPath = QDir::toNativeSeparators(QDir(appDir).filePath("runtimes/python"));
    QString nodePath = QDir::toNativeSeparators(QDir(appDir).filePath("runtimes/node"));
    
    QString currentPath = env.value("PATH");
    env.insert("PATH", mingwPath + ";" + pythonPath + ";" + nodePath + ";" + currentPath);

    QSettings settings;
    QString githubPat = settings.value("github_pat", "").toString();
    
    if (!githubPat.isEmpty()) {
        env.insert("GITHUB_PAT", githubPat);
        env.insert("GITHUB_TOKEN", githubPat); 
    }
    agentProcess->setProcessEnvironment(env);

    // execute natively based on the host os
    #ifdef Q_OS_WIN
        agentProcess->start("cmd.exe", QStringList() << "/c" << command.target);
    #else
        agentProcess->start("/bin/sh", QStringList() << "-c" << command.target);
    #endif

    // hybrid wait: if it finishes in 5s, it's a script. if not, it's a persistent server/gui.
    bool finished = agentProcess->waitForFinished(5000); 
    QString systemFeedbackMsg;

    if (finished) {
        QByteArray stdOut = agentProcess->readAllStandardOutput();
        QByteArray stdErr = agentProcess->readAllStandardError();

        systemFeedbackMsg = QString("[system execute_shell_command]:\nExit Code: %1\nSTDOUT:\n%2\nSTDERR:\n%3")
                             .arg(agentProcess->exitCode())
                             .arg(QString::fromLocal8Bit(stdOut).trimmed())
                             .arg(QString::fromLocal8Bit(stdErr).trimmed());
    } else {
        systemFeedbackMsg = QString("[system execute_shell_command]: Process started and is running in the background. Process ID: %1\n"
                                    "(Note: If this is a GUI application, you may now use the take_screenshot tool).")
                             .arg(agentProcess->processId());
    }

    emit actionFinished(systemFeedbackMsg);
}