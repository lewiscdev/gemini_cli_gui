/**
 * @file main_window.cpp
 * @brief Implementation of the primary GUI and agent orchestration logic.
 *
 * This file contains the implementation for UI rendering, SQLite chat history 
 * management, multi-modal drag-and-drop, and the execution engine for all 
 * native capabilities (shell commands, FTP, web requests, and screenshots).
 */

#include "main_window.h"
#include "gemini_api_client.h"
#include "settings_dialog.h"
#include "session_dialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QFileDialog>
#include <QSettings>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QUuid>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QEventLoop>
#include <QThread>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <QStandardPaths>

// Include Windows API for precise window cropping during screenshots
#ifdef Q_OS_WIN
#include <windows.h>

struct WindowSearchData {
    DWORD pid;
    HWND hwnd;
};

// Callback to locate the specific OS window owned by the agent's active process
BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM lParam) {
    WindowSearchData* data = reinterpret_cast<WindowSearchData*>(lParam);
    DWORD processId = 0;
    GetWindowThreadProcessId(hwnd, &processId);
    
    // Stop searching if we find a visible window belonging to the exact PID
    if (processId == data->pid && IsWindowVisible(hwnd)) {
        data->hwnd = hwnd;
        return FALSE; 
    }
    return TRUE;
}
#endif

/**
 * @brief Constructs the Main Window and initializes the application state.
 */
MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUi();
    initDatabase();
    
    // Track the running state of background code executed by the agent
    agentProcess = new QProcess(this);
    
    apiClient = new GeminiApiClient(this);
    agentController = new AgentActionManager(this);
    
    // White-list the non-destructive actions the agent can take without a prompt
    agentController->addWhitelistedAction("write_file"); 
    
    initializeConnections();

    // Draw the UI immediately so modal dialogs float correctly over the application
    this->show(); 

    // --- GLOBAL SETTINGS CHECK ---
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

    // --- SESSION MANAGER LAUNCH ---
    SessionDialog sessionDlg(this);
    if (sessionDlg.exec() != QDialog::Accepted) {
        chatDisplay->append("<span style=\"color: red;\">Error: No session selected. Please restart.</span>");
        inputField->setEnabled(false);
        sendButton->setEnabled(false);
        return;
    }

    // Apply the user's selected session configuration
    SessionData activeSession = sessionDlg.getSelectedSession();
    currentSessionId = activeSession.id;
    currentWorkspacePath = activeSession.workspace;
    
    setWindowTitle(QString("Gemini Agent - %1").arg(activeSession.name));

    // Load history; returns false if this is a brand new session
    bool isExistingSession = loadHistoryFromDb(); 

    // Only inject the heavy system prompt if Google doesn't already have the history loaded
    if (!isExistingSession) {
        QString systemInstructions = buildSystemPrompt();
        saveInteractionToDb("system", "System Sandbox Initialized: " + currentWorkspacePath);
        apiClient->sendPrompt(systemInstructions);
    } else {
        chatDisplay->append("<span style=\"color: green;\">[System: Session Restored Successfully]</span>");
    }
}

/**
 * @brief Destructor ensures no orphan processes are left running in the background.
 */
MainWindow::~MainWindow() {
    if (agentProcess && agentProcess->state() == QProcess::Running) {
        agentProcess->terminate();
        agentProcess->waitForFinished(2000);
    }
}

/**
 * @brief Dynamically constructs the comprehensive system instructions for the LLM.
 */
QString MainWindow::buildSystemPrompt() {
    return QString(
        "System Configuration: You are an autonomous local coding agent running inside a Qt C++ wrapper.\n"
        "CRITICAL: You are sandboxed to the working directory: %1\n"
        "All file paths you provide MUST be relative to this directory. Do not attempt to access files outside this workspace.\n\n"
        "GIT INTEGRATION: If you need to execute 'git push' or 'git pull', a GitHub Personal Access Token is available in the environment variable $GITHUB_PAT. "
        "To authenticate silently, format your remote URLs like this: https://$GITHUB_PAT@github.com/Username/Repo.git\n\n"
        "WEB DEPLOYMENT: You have a native 'upload_ftp' tool. You can use this to deploy HTML/CSS/JS files directly to remote servers.\n\n"
        "WEB BROWSING: You have a 'fetch_webpage' tool. Use it to verify your web deployments by reading the live DOM, or to fetch external documentation.\n\n"
        "CODE EXECUTION & TESTING: You can test the code you write! Use `execute_shell_command` to compile and run C++/Java, or execute Python/Node scripts. You will receive the terminal STDOUT/STDERR exactly as an end-user would see it.\n\n"
        "Acknowledge these instructions, confirm your working directory, and introduce yourself."
    ).arg(currentWorkspacePath);
}

/**
 * @brief Security helper to ensure file operations never escape the project workspace.
 */
QString MainWindow::resolveAndVerifyPath(const QString& relativeTarget) {
    if (currentWorkspacePath.isEmpty()) return "";

    QDir workspaceDir(currentWorkspacePath);
    QString absolutePath = QDir::cleanPath(workspaceDir.absoluteFilePath(relativeTarget));

    // Block directory traversal attacks (e.g., ../../etc/passwd)
    if (!absolutePath.startsWith(workspaceDir.absolutePath())) {
        return ""; 
    }
    return absolutePath;
}

/**
 * @brief Constructs the visual layout of the application.
 */
void MainWindow::setupUi() {
    centralWidget = new QWidget(this);
    mainLayout = new QVBoxLayout(centralWidget);

    // --- Top Toolbar ---
    QHBoxLayout* topBarLayout = new QHBoxLayout();
    btnManageSessions = new QPushButton("📁 Switch Session", this);
    btnSettings = new QPushButton("⚙️ Settings", this);
    
    topBarLayout->addWidget(btnManageSessions);
    topBarLayout->addStretch(); 
    topBarLayout->addWidget(btnSettings);
    
    mainLayout->addLayout(topBarLayout); 

    // --- Chat Display ---
    chatDisplay = new QTextEdit(this);
    chatDisplay->setReadOnly(true);
    
    tokenDisplayLabel = new QLabel("Tokens: 0 In | 0 Out", this);
    tokenDisplayLabel->setAlignment(Qt::AlignRight);
    tokenDisplayLabel->setStyleSheet("color: gray; font-size: 10px;");

    // --- Multi-modal Attachment UI ---
    lblAttachments = new QLabel("", this);
    lblAttachments->setStyleSheet("color: #0078D7; font-size: 11px; font-weight: bold;");
    lblAttachments->hide(); 

    btnClearFiles = new QPushButton("❌", this);
    btnClearFiles->setFixedSize(24, 24);
    btnClearFiles->setToolTip("Clear Attachments");
    btnClearFiles->hide();

    QHBoxLayout* attachmentLayout = new QHBoxLayout();
    attachmentLayout->addWidget(lblAttachments);
    attachmentLayout->addWidget(btnClearFiles);
    attachmentLayout->addStretch();

    // --- Input Area ---
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

    mainLayout->addWidget(chatDisplay);
    mainLayout->addWidget(tokenDisplayLabel);
    mainLayout->addLayout(attachmentLayout); 
    mainLayout->addLayout(inputLayout);      

    setCentralWidget(centralWidget);
    setWindowTitle("Gemini Native Agent");
    resize(800, 600);
    
    // Enable system drag-and-drop support
    setAcceptDrops(true); 
}

/**
 * @brief Wires UI actions and API signals to their respective logic handlers.
 */
void MainWindow::initializeConnections() {
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::handleSendClicked);
    connect(inputField, &QLineEdit::returnPressed, this, &MainWindow::handleSendClicked);
    connect(btnAttach, &QPushButton::clicked, this, &MainWindow::attachFiles);
    connect(btnClearFiles, &QPushButton::clicked, this, &MainWindow::clearAttachments);
    connect(btnSettings, &QPushButton::clicked, this, &MainWindow::openSettings);
    connect(btnManageSessions, &QPushButton::clicked, this, &MainWindow::switchSession);

    connect(apiClient, &GeminiApiClient::responseReceived, this, &MainWindow::onResponseReceived);
    connect(apiClient, &GeminiApiClient::networkError, this, &MainWindow::onNetworkError);
    connect(apiClient, &GeminiApiClient::usageMetricsReceived, this, &MainWindow::onUsageMetricsReceived);
    connect(apiClient, &GeminiApiClient::functionCallRequested, this, &MainWindow::handleNativeFunctionCall);
}

// ============================================================================
// DATABASE & SESSION MANAGEMENT
// ============================================================================

/**
 * @brief Initializes the SQLite database and schemas.
 */
void MainWindow::initDatabase() {
    // --- CRITICAL FIX: Persistent AppData Path ---
    // Forces the database into a permanent OS folder (e.g., AppData/Roaming on Windows)
    // so it doesn't get wiped when CMake cleans the local build directory.
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir); 
    QString dbPath = QDir(dataDir).absoluteFilePath("agent_history.db");

    // Safely check if the connection exists to prevent warnings on hot-reloads
    if (QSqlDatabase::contains(QSqlDatabase::defaultConnection)) {
        db = QSqlDatabase::database(QSqlDatabase::defaultConnection);
    } else {
        db = QSqlDatabase::addDatabase("QSQLITE");
    }

    db.setDatabaseName(dbPath);

    if (!db.open()) {
        QMessageBox::critical(this, "Database Error", "Could not open local database at:\n" + dbPath);
        return;
    }

    QSqlQuery query;
    query.exec("CREATE TABLE IF NOT EXISTS interactions ("
               "id INTEGER PRIMARY KEY AUTOINCREMENT, "
               "session_id TEXT, "
               "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, "
               "role TEXT, "
               "content TEXT, "
               "api_interaction_id TEXT)");
}

/**
 * @brief Saves a single chat interaction to local storage.
 */
void MainWindow::saveInteractionToDb(const QString& role, const QString& content, const QString& apiInteractionId) {
    if (!db.isOpen()) return;

    QSqlQuery query;
    query.prepare("INSERT INTO interactions (session_id, role, content, api_interaction_id) VALUES (:sid, :role, :content, :api_id)");
    query.bindValue(":sid", currentSessionId);
    query.bindValue(":role", role);
    query.bindValue(":content", content);
    query.bindValue(":api_id", apiInteractionId);
    query.exec();
}

/**
 * @brief Restores UI state and API memory thread for the active session.
 */
bool MainWindow::loadHistoryFromDb() {
    if (!db.isOpen()) return false;

    QSqlQuery query;
    query.prepare("SELECT role, content, api_interaction_id FROM interactions WHERE session_id = :session_id ORDER BY timestamp ASC");
    query.bindValue(":session_id", currentSessionId);
    query.exec();

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

    // Pass the state token back to the API client so the LLM remembers the context
    if (!lastInteractionId.isEmpty()) {
        apiClient->restoreSession(lastInteractionId);
    }
    
    return hasMessages; 
}

/**
 * @brief Hot-swaps the application to a different local project.
 */
void MainWindow::switchSession() {
    SessionDialog sessionDlg(this);
    if (sessionDlg.exec() == QDialog::Accepted) {
        SessionData activeSession = sessionDlg.getSelectedSession();

        if (activeSession.id == currentSessionId) return;

        currentSessionId = activeSession.id;
        currentWorkspacePath = activeSession.workspace;
        setWindowTitle(QString("Gemini Agent - %1").arg(activeSession.name));

        // Wipe UI and sever the Google API thread
        chatDisplay->clear();
        apiClient->resetSession(); 

        bool isExistingSession = loadHistoryFromDb();

        if (!isExistingSession) {
            QString systemInstructions = buildSystemPrompt();
            saveInteractionToDb("system", "System Sandbox Initialized: " + currentWorkspacePath);
            apiClient->sendPrompt(systemInstructions);
        } else {
            chatDisplay->append("<span style=\"color: green;\">[System: Switched to existing session]</span>");
        }
    }
}

/**
 * @brief Opens the global settings dialogue (API Keys, PATs).
 */
void MainWindow::openSettings() {
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QSettings settings;
        apiClient->setApiKey(settings.value("api_key").toString());
        chatDisplay->append("<span style=\"color: green;\">[System: Settings updated successfully]</span>");
    }
}

// ============================================================================
// MULTI-MODAL & USER INPUT
// ============================================================================

/**
 * @brief Packages text and attached files to send to the LLM.
 */
void MainWindow::handleSendClicked() {
    QString userInput = inputField->text();
    
    if (!userInput.isEmpty() || !pendingAttachments.isEmpty()) {
        QString displayMsg = userInput;
        if (!pendingAttachments.isEmpty()) {
            displayMsg += QString(" <i>[Attached %1 file(s)]</i>").arg(pendingAttachments.size());
        }

        chatDisplay->append("<b>You:</b> " + displayMsg);
        saveInteractionToDb("user", displayMsg);
        
        apiClient->sendPrompt(userInput, pendingAttachments); 
        
        inputField->clear();
        clearAttachments(); 
    }
}

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

void MainWindow::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

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

// ============================================================================
// API CALLBACKS
// ============================================================================

void MainWindow::onResponseReceived(const QString& responseText, const QString& interactionId) {
    chatDisplay->append("<b>Agent:</b> " + responseText);
    saveInteractionToDb("model", responseText, interactionId);
}

void MainWindow::onNetworkError(const QString& errorDetails) {
    chatDisplay->append("<span style=\"color: red;\"><b>Error:</b> " + errorDetails + "</span>");
}

void MainWindow::onUsageMetricsReceived(int inputTokens, int outputTokens, int totalTokens) {
    tokenDisplayLabel->setText(QString("Tokens: %1 In | %2 Out | %3 Total").arg(inputTokens).arg(outputTokens).arg(totalTokens));
}

// ============================================================================
// THE AGENT TOOL ROUTER
// ============================================================================

/**
 * @brief Intercepts raw tool calls from Google and routes them to local execution.
 */
void MainWindow::handleNativeFunctionCall(const QString& functionName, const QJsonObject& arguments) {
    chatDisplay->append(QString("<span style=\"color: orange;\"><i>[Agent requested tool execution: %1]</i></span>").arg(functionName));

    // 1. Write File
    if (functionName == "write_file") {
        QString absoluteTarget = resolveAndVerifyPath(arguments["target"].toString());
        if (absoluteTarget.isEmpty()) {
            apiClient->sendPrompt("System Error: Security violation. Path is outside the workspace.");
            return;
        }

        AgentCommand command;
        command.action = functionName;
        command.target = absoluteTarget;
        command.payload = arguments["payload"].toString();
        
        handleAgentActionRequest(command);
        return;
    }
    
    // 2. Read File (Silent action)
    if (functionName == "read_file") {
        QString absoluteTarget = resolveAndVerifyPath(arguments["target"].toString());
        QFile file(absoluteTarget);
        QString result;

        if (absoluteTarget.isEmpty() || !file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            result = "System Error: File not found or access denied.";
        } else {
            result = QString("System [read_file]:\n%1").arg(QString(file.readAll()));
            file.close();
        }
        saveInteractionToDb("system", result);
        apiClient->sendPrompt(result);
        return;
    }
    
    // 3. List Directory (Silent action)
    if (functionName == "list_directory") {
        QString absoluteTarget = resolveAndVerifyPath(arguments["target"].toString());
        QString result;

        if (absoluteTarget.isEmpty()) {
            result = "System Error: Invalid directory.";
        } else {
            QDir dir(absoluteTarget);
            result = QString("System [list_directory]:\n%1").arg(dir.entryList(QDir::AllEntries | QDir::NoDotAndDotDot).join("\n"));
        }
        saveInteractionToDb("system", result);
        apiClient->sendPrompt(result);
        return;
    }

    // 4. Execute Shell Command (Destructive, Modal required)
    if (functionName == "execute_shell_command") {
        AgentCommand command;
        command.action = functionName;
        command.target = arguments["command"].toString(); 
        handleAgentActionRequest(command);
        return;
    }

    // 5. FTP Upload (Destructive, Modal required)
    if (functionName == "upload_ftp") {
        QString localPath = resolveAndVerifyPath(arguments["local_path"].toString());
        if (localPath.isEmpty()) {
            apiClient->sendPrompt("System Error: Local file path is outside the workspace.");
            return;
        }

        AgentCommand command;
        command.action = functionName;
        command.target = localPath; 
        
        QJsonObject ftpArgs;
        ftpArgs["remote_path"] = arguments["remote_path"].toString();
        command.payload = QJsonDocument(ftpArgs).toJson(QJsonDocument::Compact);

        handleAgentActionRequest(command);
        return;
    }

    // 6. Fetch Webpage (Silent action)
    if (functionName == "fetch_webpage") {
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
        saveInteractionToDb("system", result);
        apiClient->sendPrompt(result); 
        return;
    }

    // 7. Take Screenshot (Privacy implications, Modal required)
    if (functionName == "take_screenshot") {
        AgentCommand command;
        command.action = functionName;
        command.target = "GUI Application";
        handleAgentActionRequest(command);
        return;
    }
}

// ============================================================================
// THE DESTRUCTIVE ACTION EXECUTOR
// ============================================================================

/**
 * @brief OS-native logic to crop a screenshot to the exact boundary of a running process window.
 */
QPixmap MainWindow::captureProcessWindow(qint64 processId) {
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return QPixmap();

    QPixmap fullScreen = screen->grabWindow(0);

#ifdef Q_OS_WIN
    WindowSearchData searchData = { static_cast<DWORD>(processId), nullptr };
    EnumWindows(EnumWindowsProc, reinterpret_cast<LPARAM>(&searchData));

    if (searchData.hwnd) {
        RECT rect;
        if (GetWindowRect(searchData.hwnd, &rect)) {
            int x = rect.left;
            int y = rect.top;
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;
            
            return fullScreen.copy(x, y, width, height);
        }
    }
#endif

    return fullScreen; 
}

/**
 * @brief The Security Modal and Execution Engine for destructive or privacy-impacting actions.
 */
void MainWindow::handleAgentActionRequest(const AgentCommand& command) {
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("Agent Action Approval");
    msgBox.setText(QString("The agent wants to execute: <b>%1</b>\nTarget: %2\n\nDo you allow this?").arg(command.action, command.target));
    msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    msgBox.setDefaultButton(QMessageBox::No);

    if (msgBox.exec() == QMessageBox::Yes) {
        chatDisplay->append(QString("<span style=\"color: green;\">[System: Executed %1 on %2]</span>").arg(command.action, command.target));
        QString systemFeedbackMsg;

        // Route 1: Write File
        if (command.action == "write_file") {
            agentController->executeApprovedAction(command);
            systemFeedbackMsg = "System: Action approved. File written successfully.";
        } 
        
        // Route 2: Shell / Terminal Execution (Hybrid Wait)
        else if (command.action == "execute_shell_command") {
            if (agentProcess->state() == QProcess::Running) {
                agentProcess->terminate();
                agentProcess->waitForFinished(2000); 
            }

            agentProcess->setWorkingDirectory(currentWorkspacePath); 

            QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
            QSettings settings;
            QString githubPat = settings.value("github_pat", "").toString();
            
            if (!githubPat.isEmpty()) {
                env.insert("GITHUB_PAT", githubPat);
                env.insert("GITHUB_TOKEN", githubPat); 
            }
            agentProcess->setProcessEnvironment(env);

            #ifdef Q_OS_WIN
                agentProcess->start("cmd.exe", QStringList() << "/c" << command.target);
            #else
                agentProcess->start("/bin/sh", QStringList() << "-c" << command.target);
            #endif

            // Wait up to 5 seconds. If it finishes, it's a CLI tool. If not, it's a persistent GUI/Server.
            bool finished = agentProcess->waitForFinished(5000); 

            if (finished) {
                QByteArray stdOut = agentProcess->readAllStandardOutput();
                QByteArray stdErr = agentProcess->readAllStandardError();

                systemFeedbackMsg = QString("System [execute_shell_command]:\nExit Code: %1\nSTDOUT:\n%2\nSTDERR:\n%3")
                                     .arg(agentProcess->exitCode())
                                     .arg(QString::fromLocal8Bit(stdOut).trimmed())
                                     .arg(QString::fromLocal8Bit(stdErr).trimmed());
            } else {
                systemFeedbackMsg = QString("System [execute_shell_command]: Process started and is running in the background. Process ID: %1\n"
                                            "(Note: If this is a GUI application, you may now use the take_screenshot tool).")
                                     .arg(agentProcess->processId());
            }
        }
        
        // Route 3: Native FTP Upload
        else if (command.action == "upload_ftp") {
            QJsonObject ftpArgs = QJsonDocument::fromJson(command.payload.toUtf8()).object();
            QString remotePath = ftpArgs["remote_path"].toString();

            // Pull credentials globally from the OS Registry
            QSettings settings;
            QString host = settings.value("ftp_host", "").toString();
            int port = settings.value("ftp_port", 21).toInt();
            QString username = settings.value("ftp_username", "").toString();
            QString password = settings.value("ftp_password", "").toString();

            if (host.isEmpty()) {
                systemFeedbackMsg = "System Error: FTP Host is not configured. Please set it up in Global Settings.";
            } else {
                // Ensure proper path formatting
                if (!remotePath.startsWith("/")) {
                    remotePath.prepend("/");
                }

                // Construct the secure URL
                QUrl url;
                url.setScheme("ftp");
                url.setHost(host);
                url.setPort(port);
                url.setPath(remotePath);
                url.setUserName(username);
                url.setPassword(password);

                QFile* fileToUpload = new QFile(command.target);
                if (!fileToUpload->open(QIODevice::ReadOnly)) {
                    systemFeedbackMsg = "System Error: Could not open local file for upload.";
                    delete fileToUpload;
                } else {
                    QNetworkAccessManager ftpManager;
                    QNetworkRequest request(url);
                    QNetworkReply* reply = ftpManager.put(request, fileToUpload);
                    
                    QEventLoop loop;
                    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
                    loop.exec();

                    if (reply->error() == QNetworkReply::NoError) {
                        // Use toString(QUrl::RemovePassword) to keep credentials out of the chat history!
                        systemFeedbackMsg = "System [upload_ftp]: Upload successful to " + url.toString(QUrl::RemovePassword);
                    } else {
                        systemFeedbackMsg = "System Error [upload_ftp]: " + reply->errorString();
                    }
                    
                    reply->deleteLater();
                    fileToUpload->setParent(reply); 
                }
            }
        }
        
        // Route 4: GUI Screenshot
        else if (command.action == "take_screenshot") {
            qint64 pid = agentProcess->processId();
            
            if (pid == 0 || agentProcess->state() != QProcess::Running) {
                systemFeedbackMsg = "System Error: No active GUI application is currently running to screenshot.";
            } else {
                QPixmap croppedShot = captureProcessWindow(pid);
                
                QMessageBox imgBox(this);
                imgBox.setWindowTitle("Security Intercept: Approve Image");
                imgBox.setText("The agent wants to send this image to the API. Do you approve?");
                imgBox.setIconPixmap(croppedShot.scaledToWidth(400, Qt::SmoothTransformation));
                imgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
                
                if (imgBox.exec() == QMessageBox::Yes) {
                    QString savePath = QDir(currentWorkspacePath).absoluteFilePath("agent_vision_capture.png");
                    croppedShot.save(savePath, "PNG");
                    
                    systemFeedbackMsg = QString("System [take_screenshot]: Success. Image saved locally as '%1'. Please read the file to analyze the UI.").arg("agent_vision_capture.png");
                } else {
                    systemFeedbackMsg = "System Error: The human user DENIED the screenshot request due to privacy concerns.";
                }
            }
        }

        saveInteractionToDb("system", systemFeedbackMsg);
        apiClient->sendPrompt(systemFeedbackMsg);
        
    } else {
        chatDisplay->append(QString("<span style=\"color: red;\">[System: Denied %1 on %2]</span>").arg(command.action, command.target));
        apiClient->sendPrompt(QString("System: User denied permission to execute %1.").arg(command.action));
    }
}