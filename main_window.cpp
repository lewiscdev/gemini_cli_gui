#include "main_window.h"
#include "gemini_api_client.h" 
#include "settings_dialog.h"
#include "session_dialog.h"

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
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QEventLoop>
#include <QUrl>
#include <QFileDialog>

// --- THE SANDBOX HELPER ---
// This safely resolves relative paths to absolute paths, ensuring the LLM 
// cannot escape the designated workspace directory (e.g., trying to read ../../System32)
// The new, session-aware sandbox helper
QString MainWindow::resolveAndVerifyPath(const QString& relativeTarget) {
    if (currentWorkspacePath.isEmpty()) return "";

    QDir workspaceDir(currentWorkspacePath);
    QString absolutePath = QDir::cleanPath(workspaceDir.absoluteFilePath(relativeTarget));

    // Security: Ensure the resolved path hasn't escaped the active session's workspace
    if (!absolutePath.startsWith(workspaceDir.absolutePath())) {
        return ""; 
    }
    return absolutePath;
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUi();
    initDatabase();
    
    apiClient = new GeminiApiClient(this);
    agentController = new AgentActionManager(this);
    agentController->addWhitelistedAction("write_file"); 
    
    initializeConnections();

    // 1. GLOBAL SETTINGS CHECK (API Key Only)
    QSettings settings;
    QString storedKey = settings.value("api_key", "").toString();

    if (storedKey.isEmpty()) {
        SettingsDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            storedKey = dialog.getApiKey();
        } else {
            chatDisplay->append("<span style=\"color: red;\">Error: API Key required. Please restart.</span>");
            inputField->setEnabled(false);
            sendButton->setEnabled(false);
            return;
        }
    }
    apiClient->setApiKey(storedKey);

    // 2. SESSION MANAGER LAUNCH
    SessionDialog sessionDlg(this);
    if (sessionDlg.exec() != QDialog::Accepted) {
        chatDisplay->append("<span style=\"color: red;\">Error: No session selected. Please restart.</span>");
        inputField->setEnabled(false);
        sendButton->setEnabled(false);
        return;
    }

    // 3. APPLY THE SELECTED SESSION
    SessionData activeSession = sessionDlg.getSelectedSession();
    currentSessionId = activeSession.id;
    currentWorkspacePath = activeSession.workspace;
    
    // Update window title to show the active project
    setWindowTitle(QString("Gemini Agent - %1").arg(activeSession.name));

    // 4. LOAD HISTORY & INITIALIZE
    // Load the specific chat history for THIS session
    bool isExistingSession = loadHistoryFromDb(); 

    // ONLY send the system instructions if this is a brand new project!
    // If it's an existing project, Google already remembers the workspace.
    if (!isExistingSession) {
        QString systemInstructions = QString(
            "System Configuration: You are an autonomous local coding agent running inside a Qt C++ wrapper.\n"
            "CRITICAL: You are sandboxed to the working directory: %1\n"
            "All file paths you provide MUST be relative to this directory. Do not attempt to access files outside this workspace.\n\n"
            "GIT INTEGRATION: If you need to execute 'git push' or 'git pull', a GitHub Personal Access Token is available in the environment variable $GITHUB_PAT. "
            "WEB DEPLOYMENT: You have a native 'upload_ftp' tool. You can use this to deploy HTML/CSS/JS files directly to remote servers.\n\n"
            "To authenticate silently, format your remote URLs like this: https://$GITHUB_PAT@github.com/Username/Repo.git\n\n"
            "Acknowledge these instructions, confirm your working directory, and introduce yourself."
        ).arg(currentWorkspacePath);
            
        saveInteractionToDb("system", "System Sandbox Initialized: " + currentWorkspacePath);
        apiClient->sendPrompt(systemInstructions);
    } else {
        chatDisplay->append("<span style=\"color: green;\">[System: Session Restored Successfully]</span>");
    }

    setAcceptDrops(true); // Enable Drag and Drop
} // End of constructor

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

bool MainWindow::loadHistoryFromDb() {
    if (!db.isOpen()) return false;

    // CRITICAL FIX: Only fetch interactions for the currently active session!
    QSqlQuery query;
    query.prepare("SELECT role, content, api_interaction_id FROM interactions WHERE session_id = :session_id ORDER BY timestamp ASC");
    query.bindValue(":session_id", currentSessionId);
    
    if (!query.exec()) {
        appendStandardError("Failed to load history: " + query.lastError().text());
        return false;
    }

    QString lastInteractionId;
    bool hasMessages = false;

    while (query.next()) {
        hasMessages = true;
        QString role = query.value(0).toString();
        QString content = query.value(1).toString();
        QString apiId = query.value(2).toString();

        if (!apiId.isEmpty()) {
            lastInteractionId = apiId;
        }

        if (role == "user") {
            chatDisplay->append("<b>You:</b> " + content);
        } else if (role == "model") {
            chatDisplay->append("<b>Agent:</b> " + content);
        } else if (role == "system") {
            chatDisplay->append("<span style=\"color: gray;\">" + content + "</span>");
        }
    }

    if (!lastInteractionId.isEmpty()) {
        apiClient->restoreSession(lastInteractionId);
    }
    
    return hasMessages; // Tell the constructor if this was an existing chat
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

    QHBoxLayout* topBarLayout = new QHBoxLayout();
    btnManageSessions = new QPushButton("📁 Switch Session", this);
    btnSettings = new QPushButton("⚙️ Settings", this);
    
    topBarLayout->addWidget(btnManageSessions);
    topBarLayout->addStretch(); // Pushes settings to the far right
    topBarLayout->addWidget(btnSettings);
    
    mainLayout->addLayout(topBarLayout);

    chatDisplay = new QTextEdit(this);
    chatDisplay->setReadOnly(true);

    tokenDisplayLabel = new QLabel("Context Load: 0 tokens | Last Output: 0 tokens", this);
    tokenDisplayLabel->setAlignment(Qt::AlignRight);
    tokenDisplayLabel->setStyleSheet("color: #888888; font-size: 11px;");

    // --- NEW: Bottom Input Area ---
    lblAttachments = new QLabel("", this);
    lblAttachments->setStyleSheet("color: #0078D7; font-size: 11px; font-weight: bold;");
    lblAttachments->hide(); // Hidden by default until a file is attached

    btnClearFiles = new QPushButton("❌", this);
    btnClearFiles->setFixedSize(24, 24);
    btnClearFiles->setToolTip("Clear Attachments");
    btnClearFiles->hide();

    QHBoxLayout* attachmentLayout = new QHBoxLayout();
    attachmentLayout->addWidget(lblAttachments);
    attachmentLayout->addWidget(btnClearFiles);
    attachmentLayout->addStretch();

    QHBoxLayout* inputLayout = new QHBoxLayout();
    btnAttach = new QPushButton("📎", this);
    btnAttach->setFixedSize(30, 30);
    btnAttach->setToolTip("Attach Files");

    inputField = new QLineEdit(this);
    inputField->setPlaceholderText("Enter command, or drag and drop files here...");

    sendButton = new QPushButton("Send", this);

    inputLayout->addWidget(btnAttach);
    inputLayout->addWidget(inputField);
    inputLayout->addWidget(sendButton);

    // Assemble the main layout
    mainLayout->addWidget(chatDisplay);
    mainLayout->addWidget(tokenDisplayLabel);
    mainLayout->addLayout(attachmentLayout); // Shows pending files
    mainLayout->addLayout(inputLayout);      // The input bar

    setCentralWidget(centralWidget);
    setWindowTitle("Gemini Native Agent");
    resize(800, 600);
}

void MainWindow::initializeConnections() {
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::handleSendClicked);
    connect(inputField, &QLineEdit::returnPressed, sendButton, &QPushButton::click);
    connect(btnSettings, &QPushButton::clicked, this, &MainWindow::openSettings);
    connect(btnManageSessions, &QPushButton::clicked, this, &MainWindow::switchSession);
    connect(btnAttach, &QPushButton::clicked, this, &MainWindow::attachFiles);
    connect(btnClearFiles, &QPushButton::clicked, this, &MainWindow::clearAttachments);

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
    
    // Allow sending if there is text OR if there are just files attached
    if (!userInput.isEmpty() || !pendingAttachments.isEmpty()) {
        
        QString displayMsg = userInput;
        if (!pendingAttachments.isEmpty()) {
            displayMsg += QString(" <i>[Attached %1 file(s)]</i>").arg(pendingAttachments.size());
        }

        chatDisplay->append("<b>You:</b> " + displayMsg);
        saveInteractionToDb("user", displayMsg);
        
        // Pass the attachments directly to the API client!
        apiClient->sendPrompt(userInput, pendingAttachments); 
        
        inputField->clear();
        clearAttachments(); // Wipe the pending list after sending
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
            QProcess process;
            process.setWorkingDirectory(currentWorkspacePath); 

            // --- NEW: Inject Environment Variables ---
            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            QSettings settings;
            QString githubPat = settings.value("github_pat", "").toString();
            
            if (!githubPat.isEmpty()) {
                env.insert("GITHUB_PAT", githubPat);
                env.insert("GITHUB_TOKEN", githubPat); // Standard fallback for gh cli
            }
            process.setProcessEnvironment(env);
            // -----------------------------------------

            // Cross-platform shell wrapper
            #ifdef Q_OS_WIN
                process.start("cmd.exe", QStringList() << "/c" << command.target);
            #else
                process.start("/bin/sh", QStringList() << "-c" << command.target);
            #endif

            process.waitForFinished(15000); 

            QByteArray stdOut = process.readAllStandardOutput();
            QByteArray stdErr = process.readAllStandardError();

            systemFeedbackMsg = QString("System [execute_shell_command]:\nExit Code: %1\nSTDOUT:\n%2\nSTDERR:\n%3")
                                 .arg(process.exitCode())
                                 .arg(QString::fromLocal8Bit(stdOut).trimmed())
                                 .arg(QString::fromLocal8Bit(stdErr).trimmed());
        }
        // Route 3: Native FTP Upload
        else if (command.action == "upload_ftp") {
            QJsonObject ftpArgs = QJsonDocument::fromJson(command.payload.toUtf8()).object();
            QString remoteUrl = ftpArgs["remote_url"].toString();
            
            QUrl url(remoteUrl);
            url.setUserName(ftpArgs["username"].toString());
            url.setPassword(ftpArgs["password"].toString());

            QFile* fileToUpload = new QFile(command.target);
            if (!fileToUpload->open(QIODevice::ReadOnly)) {
                systemFeedbackMsg = "System Error: Could not open local file for upload.";
                delete fileToUpload;
            } else {
                QNetworkAccessManager ftpManager;
                QNetworkRequest request(url);
                
                // Execute the native PUT request
                QNetworkReply* reply = ftpManager.put(request, fileToUpload);
                
                // Freeze local execution until the upload finishes
                QEventLoop loop;
                connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
                loop.exec();

                if (reply->error() == QNetworkReply::NoError) {
                    systemFeedbackMsg = "System [upload_ftp]: Upload successful to " + remoteUrl;
                } else {
                    systemFeedbackMsg = "System Error [upload_ftp]: " + reply->errorString();
                }
                
                reply->deleteLater();
                fileToUpload->setParent(reply); // Auto-deletes the file object when reply is destroyed
            }
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

    // 6. DESTRUCTIVE ACTION: FTP Upload (Must go through the UI Modal)
    if (functionName == "upload_ftp") {
        QString localPath = resolveAndVerifyPath(arguments["local_path"].toString());
        if (localPath.isEmpty()) {
            apiClient->sendPrompt("System Error: Local file path is outside the workspace.");
            return;
        }

        AgentCommand command;
        command.action = functionName;
        command.target = localPath; 
        
        // Pack the FTP credentials into the payload to pass to the execution engine
        QJsonObject ftpArgs;
        ftpArgs["remote_url"] = arguments["remote_url"].toString();
        ftpArgs["username"] = arguments["username"].toString();
        ftpArgs["password"] = arguments["password"].toString();
        command.payload = QJsonDocument(ftpArgs).toJson(QJsonDocument::Compact);

        handleAgentActionRequest(command);
        return;
    }
}

// --- TOOLBAR LOGIC ---

void MainWindow::openSettings() {
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        // Update the live API client if the user changed their key
        QSettings settings;
        apiClient->setApiKey(settings.value("api_key").toString());
        
        chatDisplay->append("<span style=\"color: green;\">[System: Settings updated successfully]</span>");
    }
}

void MainWindow::switchSession() {
    SessionDialog sessionDlg(this);
    if (sessionDlg.exec() == QDialog::Accepted) {
        SessionData activeSession = sessionDlg.getSelectedSession();

        // Prevent reloading if they picked the session they are already in
        if (activeSession.id == currentSessionId) return;

        // Apply new session data
        currentSessionId = activeSession.id;
        currentWorkspacePath = activeSession.workspace;
        setWindowTitle(QString("Gemini Agent - %1").arg(activeSession.name));

        // Wipe the UI and reset the Google API state to start fresh
        chatDisplay->clear();
        apiClient->resetSession(); 

        // Load the new session's history
        bool isExistingSession = loadHistoryFromDb();

        if (!isExistingSession) {
            QString systemInstructions = QString(
                "System Configuration: You are an autonomous local coding agent running inside a Qt C++ wrapper.\n"
                "CRITICAL: You are sandboxed to the working directory: %1\n"
                "All file paths you provide MUST be relative to this directory. Do not attempt to access files outside this workspace.\n\n"
                "GIT INTEGRATION: If you need to execute 'git push' or 'git pull', a GitHub Personal Access Token is available in the environment variable $GITHUB_PAT. "
                "To authenticate silently, format your remote URLs like this: https://$GITHUB_PAT@github.com/Username/Repo.git\n\n"
                "Acknowledge these instructions, confirm your working directory, and introduce yourself."
            ).arg(currentWorkspacePath);
                
            saveInteractionToDb("system", "System Sandbox Initialized: " + currentWorkspacePath);
            apiClient->sendPrompt(systemInstructions);
        } else {
            chatDisplay->append("<span style=\"color: green;\">[System: Switched to existing session]</span>");
        }
    }
}

// --- MULTI-MODAL & ATTACHMENT LOGIC ---

void MainWindow::attachFiles() {
    QStringList files = QFileDialog::getOpenFileNames(this, "Select Files to Attach");
    if (!files.isEmpty()) {
        pendingAttachments.append(files);
        updateAttachmentUi();
    }
}

void MainWindow::clearAttachments() {
    pendingAttachments.clear();
    updateAttachmentUi();
}

void MainWindow::updateAttachmentUi() {
    if (pendingAttachments.isEmpty()) {
        lblAttachments->hide();
        btnClearFiles->hide();
    } else {
        lblAttachments->setText(QString("📎 %1 file(s) attached").arg(pendingAttachments.size()));
        lblAttachments->show();
        btnClearFiles->show();
    }
}

// Drag & Drop: Accept the event if it contains URLs (files)
void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

// Drag & Drop: Extract the file paths when dropped
void MainWindow::dropEvent(QDropEvent *event) {
    const QMimeData *mimeData = event->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        for (const QUrl &url : urlList) {
            if (url.isLocalFile()) {
                pendingAttachments.append(url.toLocalFile());
            }
        }
        updateAttachmentUi();
        event->acceptProposedAction();
    }
}