#include "main_window.h"
#include "gemini_api_client.h" 
#include "settings_dialog.h"

#include <QSettings>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QWidget>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QUuid>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QStringList>
#include <QProcess>

// --- THE SANDBOX HELPER ---
// This safely resolves relative paths to absolute paths, ensuring the LLM 
// cannot escape the designated workspace directory (e.g., trying to read ../../System32)
static QString resolveAndVerifyPath(const QString& relativeTarget) {
    QSettings settings;
    QString workspacePath = settings.value("workspace_dir", QDir::homePath()).toString();
    QDir workspaceDir(workspacePath);

    QString absolutePath = QDir::cleanPath(workspaceDir.absoluteFilePath(relativeTarget));

    if (!absolutePath.startsWith(workspaceDir.absolutePath())) {
        return ""; // Path escaping detected!
    }
    return absolutePath;
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUi();
    initDatabase();
    
    currentSessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    apiClient = new GeminiApiClient(this);
    agentController = new AgentActionManager(this);
    agentController->addWhitelistedAction("write_file"); 
    
    initializeConnections();

    loadHistoryFromDb();

    // Check settings for API Key AND Workspace Directory
    QSettings settings;
    QString storedKey = settings.value("api_key", "").toString();
    QString storedWorkspace = settings.value("workspace_dir", "").toString();

    // Force settings modal if either is missing
    if (storedKey.isEmpty() || storedWorkspace.isEmpty()) {
        SettingsDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            storedKey = dialog.getApiKey();
            storedWorkspace = dialog.getWorkspaceDirectory();
        } else {
            chatDisplay->append("<span style=\"color: red;\">Error: Application requires an API Key and Workspace. Please restart.</span>");
            inputField->setEnabled(false);
            sendButton->setEnabled(false);
            return;
        }
    }

    apiClient->setApiKey(storedKey);
    
    // Inject the dynamically generated system prompt with the strict sandbox path
    QString systemInstructions = QString(
        "System Configuration: You are an autonomous local coding agent running inside a Qt C++ wrapper.\n"
        "CRITICAL: You are sandboxed to the following working directory: %1\n"
        "All file paths you provide to your tools MUST be relative to this directory. Do not attempt to access files outside this workspace.\n"
        "Acknowledge these instructions, confirm your working directory, and introduce yourself."
    ).arg(storedWorkspace);
        
    apiClient->sendPrompt(systemInstructions);
}

MainWindow::~MainWindow() {
    if (db.isOpen()) {
        db.close();
    }
}

void MainWindow::initDatabase() {
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName("interactions.db");

    if (!db.open()) {
        QMessageBox::critical(this, "Database Error", "Failed to open local SQLite database.");
        return;
    }

    QSqlQuery query;
    query.exec("CREATE TABLE IF NOT EXISTS interactions ("
               "local_id TEXT PRIMARY KEY, "
               "session_id TEXT, "
               "api_interaction_id TEXT, "
               "role TEXT, "
               "content TEXT, "
               "timestamp INTEGER)");
}

void MainWindow::loadHistoryFromDb() {
    if (!db.isOpen()) return;

    // Fetch all interactions in chronological order
    QSqlQuery query("SELECT role, content, api_interaction_id FROM interactions ORDER BY timestamp ASC");
    QString lastInteractionId;

    while (query.next()) {
        QString role = query.value(0).toString();
        QString content = query.value(1).toString();
        QString apiId = query.value(2).toString();

        // Keep track of the most recent Google Interaction ID
        if (!apiId.isEmpty()) {
            lastInteractionId = apiId;
        }

        // Repopulate the UI exactly how the user left it
        if (role == "user") {
            chatDisplay->append("<b>You:</b> " + content);
        } else if (role == "model") {
            chatDisplay->append("<b>Agent:</b> " + content);
        } else if (role == "system") {
            chatDisplay->append("<span style=\"color: gray;\">" + content + "</span>");
        }
    }

    // CRITICAL: Hand the state back to the Gemini Client!
    if (!lastInteractionId.isEmpty()) {
        apiClient->restoreSession(lastInteractionId);
    }
}

void MainWindow::saveInteractionToDb(const QString& role, const QString& content, const QString& apiInteractionId) {
    if (!db.isOpen()) return;

    QSqlQuery query;
    query.prepare("INSERT INTO interactions (local_id, session_id, api_interaction_id, role, content, timestamp) "
                  "VALUES (:local_id, :session_id, :api_interaction_id, :role, :content, :timestamp)");
    
    query.bindValue(":local_id", QUuid::createUuid().toString(QUuid::WithoutBraces));
    query.bindValue(":session_id", currentSessionId);
    query.bindValue(":api_interaction_id", apiInteractionId);
    query.bindValue(":role", role);
    query.bindValue(":content", content);
    query.bindValue(":timestamp", QDateTime::currentSecsSinceEpoch());

    if (!query.exec()) {
        appendStandardError("Failed to save interaction to database: " + query.lastError().text());
    }
}

void MainWindow::setupUi() {
    centralWidget = new QWidget(this);
    mainLayout = new QVBoxLayout(centralWidget);

    chatDisplay = new QTextEdit(this);
    chatDisplay->setReadOnly(true);

    tokenDisplayLabel = new QLabel("Context Load: 0 tokens | Last Output: 0 tokens", this);
    tokenDisplayLabel->setAlignment(Qt::AlignRight);
    tokenDisplayLabel->setStyleSheet("color: #888888; font-size: 11px;");

    inputField = new QLineEdit(this);
    inputField->setPlaceholderText("Enter command or prompt...");

    sendButton = new QPushButton("Send", this);

    mainLayout->addWidget(chatDisplay);
    mainLayout->addWidget(tokenDisplayLabel);
    mainLayout->addWidget(inputField);
    mainLayout->addWidget(sendButton);

    setCentralWidget(centralWidget);
    setWindowTitle("Gemini Native Agent");
    resize(800, 600);
}

void MainWindow::initializeConnections() {
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::handleSendClicked);
    connect(inputField, &QLineEdit::returnPressed, sendButton, &QPushButton::click);

    connect(apiClient, &GeminiApiClient::responseReceived, this, &MainWindow::appendStandardOutput);
    connect(apiClient, &GeminiApiClient::networkError, this, &MainWindow::appendStandardError);
    connect(apiClient, &GeminiApiClient::functionCallRequested, this, &MainWindow::handleNativeFunctionCall);
    connect(apiClient, &GeminiApiClient::usageMetricsReceived, this, &MainWindow::updateTokenDisplay);
}

void MainWindow::updateTokenDisplay(int inputTokens, int outputTokens, int totalTokens) {
    QString displayText = QString("Session Context Load: %1 | Last Reply Cost: %2 | Turn Total: %3")
                          .arg(inputTokens).arg(outputTokens).arg(totalTokens);
                          
    tokenDisplayLabel->setText(displayText);
    
    if (inputTokens > 950000) {
        tokenDisplayLabel->setStyleSheet("color: red; font-size: 11px; font-weight: bold;");
        chatDisplay->append("<span style=\"color: orange;\"><b>[System Warning]:</b> Approaching maximum 1M token context window! The API will reject further requests soon. Please start a new session.</span>");
    } else if (inputTokens > 800000) {
        tokenDisplayLabel->setStyleSheet("color: orange; font-size: 11px;");
    } else {
        tokenDisplayLabel->setStyleSheet("color: #888888; font-size: 11px;");
    }
}

void MainWindow::handleSendClicked() {
    QString userInput = inputField->text();
    
    if (!userInput.isEmpty()) {
        chatDisplay->append("<b>You:</b> " + userInput);
        saveInteractionToDb("user", userInput);
        apiClient->sendPrompt(userInput);
        inputField->clear();
    }
}

void MainWindow::appendStandardOutput(const QString& text, const QString& interactionId) {
    chatDisplay->append("<b>Agent:</b> " + text);
    saveInteractionToDb("model", text, interactionId);
}

void MainWindow::appendStandardError(const QString& text) {
    chatDisplay->append("<span style=\"color: red;\">Network Error: " + text + "</span>");
}

void MainWindow::handleAgentActionRequest(const AgentCommand& command) {
    QString warningText = QString("The agent is requesting permission to execute an action.\n\n"
                                  "Action: %1\n"
                                  "Target: %2\n\n"
                                  "Press [Enter] to Allow, or [Esc] to Deny.")
                                  .arg(command.action, command.target);

    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Security Intercept");
    msgBox.setText(warningText);
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::Yes); 

    if (msgBox.exec() == QMessageBox::Yes) {
        chatDisplay->append(QString("<span style=\"color: green;\">[System: Executed %1 on %2]</span>").arg(command.action, command.target));
        
        QString systemFeedbackMsg;

        // Route 1: Standard File Writing
        if (command.action == "write_file") {
            agentController->executeApprovedAction(command);
            systemFeedbackMsg = "System: Action approved. File written successfully.";
        } 
        // Route 2: Shell/Terminal Execution
        else if (command.action == "execute_shell_command") {
            QSettings settings;
            QString workspacePath = settings.value("workspace_dir", QDir::homePath()).toString();

            QProcess process;
            process.setWorkingDirectory(workspacePath); // Lock execution to the sandbox

            // Cross-platform shell wrapper
            #ifdef Q_OS_WIN
                process.start("cmd.exe", QStringList() << "/c" << command.target);
            #else
                process.start("/bin/sh", QStringList() << "-c" << command.target);
            #endif

            // Wait up to 15 seconds for the command to finish compiling/running
            process.waitForFinished(15000); 

            // Capture the raw terminal output
            QByteArray stdOut = process.readAllStandardOutput();
            QByteArray stdErr = process.readAllStandardError();

            // Format the feedback exactly like a terminal window for the LLM
            systemFeedbackMsg = QString("System [execute_shell_command]:\nExit Code: %1\nSTDOUT:\n%2\nSTDERR:\n%3")
                                 .arg(process.exitCode())
                                 .arg(QString::fromLocal8Bit(stdOut).trimmed())
                                 .arg(QString::fromLocal8Bit(stdErr).trimmed());
        }

        // Save and send the resulting terminal output back to the LLM
        saveInteractionToDb("system", systemFeedbackMsg);
        apiClient->sendPrompt(systemFeedbackMsg);
    } else {
        chatDisplay->append("<span style=\"color: red;\">[System: Action denied by user.]</span>");
        
        QString failureMsg = "System: The user DENIED permission for that action. It was not executed.";
        saveInteractionToDb("system", failureMsg);
        apiClient->sendPrompt(failureMsg);
    }
}

// --- THE FUNCTION ROUTER ---
void MainWindow::handleNativeFunctionCall(const QString& functionName, const QJsonObject& arguments) {
    QString target = arguments["target"].toString();
    QString safePath = resolveAndVerifyPath(target);

    // 1. Sandbox Verification Check
    if (safePath.isEmpty()) {
        QString errorMsg = "System Error: Path traversal detected. Access to outside directories is denied.";
        saveInteractionToDb("system", errorMsg);
        apiClient->sendPrompt(errorMsg);
        return;
    }

    // 2. SILENT ACTION: Read File
    if (functionName == "read_file") {
        QFile file(safePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = file.readAll();
            file.close();
            
            QString result = QString("System [read_file %1]:\n%2").arg(target, content);
            saveInteractionToDb("system", result);
            apiClient->sendPrompt(result); 
        } else {
            apiClient->sendPrompt(QString("System Error: Could not read file %1").arg(target));
        }
        return;
    }

    // 3. SILENT ACTION: List Directory
    if (functionName == "list_directory") {
        QDir dir(safePath);
        if (dir.exists()) {
            QStringList entries = dir.entryList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
            QString result = QString("System [list_directory %1]:\n%2").arg(target, entries.join("\n"));
            
            saveInteractionToDb("system", result);
            apiClient->sendPrompt(result);
        } else {
            apiClient->sendPrompt(QString("System Error: Directory %1 does not exist.").arg(target));
        }
        return;
    }

    // 4. DESTRUCTIVE ACTION: Write File (Must go through the UI Modal)
    if (functionName == "write_file") {
        AgentCommand command;
        command.action = functionName;
        command.target = safePath; // We pass the safe absolute path to the execution controller
        command.payload = arguments["payload"].toString();

        handleAgentActionRequest(command);
    }

    // 5. DESTRUCTIVE ACTION: Execute Shell Command (Must go through the UI Modal)
    if (functionName == "execute_shell_command") {
        AgentCommand command;
        command.action = functionName;
        // We store the shell command in the 'target' variable so the UI modal can display it
        command.target = arguments["command"].toString(); 
        command.payload = "";

        handleAgentActionRequest(command);
        return;
    }
}