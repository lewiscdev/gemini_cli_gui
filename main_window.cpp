/**
 * @file main_window.cpp
 * @brief Implementation of the primary GUI and agent orchestration logic.
 *
 * This file handles UI rendering, multi-modal drag-and-drop, and API routing.
 * All complex agent actions (like shell execution, FTP uploads, and screenshots) 
 * are entirely offloaded to the isolated Command Pattern action classes.
 */

#include "main_window.h"
#include "gemini_api_client.h"
#include "settings_dialog.h"
#include "session_dialog.h"
#include "agent_prompt_builder.h"
#include "chat_formatter.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QFileDialog>
#include <QSettings>
#include <QFileInfo>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUi();
    
    // initialize the central database manager
    dbManager = new DatabaseManager(this);
    if (!dbManager->initializeDatabase()) {
        QMessageBox::critical(this, "Database Error", "Could not initialize local database.");
        return;
    }
    
    apiClient = new GeminiApiClient(this);
    agentController = new AgentActionManager(this);
    
    // white-list the non-destructive actions the agent can take without a prompt
    agentController->addWhitelistedAction("write_file"); 
    
    initializeConnections();

    // draw the ui immediately so modal dialogs float correctly over the application
    this->show(); 

    // --- global settings check ---
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

    // --- session manager launch ---
    SessionDialog sessionDlg(dbManager, this);
    if (sessionDlg.exec() != QDialog::Accepted) {
        chatDisplay->append("<span style=\"color: red;\">Error: No session selected. Please restart.</span>");
        inputField->setEnabled(false);
        sendButton->setEnabled(false);
        return;
    }

    SessionData activeSession = sessionDlg.getSelectedSession();
    currentSessionId = activeSession.id;
    currentWorkspacePath = activeSession.workspace;
    
    setWindowTitle(QString("Gemini Agent - %1").arg(activeSession.name));

    bool isExistingSession = loadHistoryFromDb(); 

    if (!isExistingSession) {
            QString systemInstructions = AgentPromptBuilder::buildSystemInstruction(currentWorkspacePath);
            saveInteractionToDb("system", "System Sandbox Initialized: " + currentWorkspacePath);
            QList<InteractionData> history = dbManager->getInteractions(currentSessionId);
            apiClient->sendPrompt(history, systemInstructions);
    } else {
        QTextCursor cursor = chatDisplay->textCursor();
        cursor.movePosition(QTextCursor::End);
        chatDisplay->setTextCursor(cursor);
        chatDisplay->insertHtml("<span style=\"color: green;\">[System: Session Restored Successfully]</span><br><br>");
        chatDisplay->ensureCursorVisible();
    }
}

MainWindow::~MainWindow() {
    // all memory and background process cleanup is now securely handled by the qobject tree
}

void MainWindow::setupUi() {
    centralWidget = new QWidget(this);
    mainLayout = new QVBoxLayout(centralWidget);

    QHBoxLayout* topBarLayout = new QHBoxLayout();
    btnManageSessions = new QPushButton("📁 Switch Session", this);
    btnSettings = new QPushButton("⚙️ Settings", this);
    
    topBarLayout->addWidget(btnManageSessions);
    topBarLayout->addStretch(); 
    topBarLayout->addWidget(btnSettings);
    
    mainLayout->addLayout(topBarLayout); 

    chatDisplay = new QTextBrowser(this);
    chatDisplay->setReadOnly(true);
    chatDisplay->setOpenLinks(false); // We must disable auto-open so we can intercept the copy click!

    tokenDisplayLabel = new QLabel("Tokens: 0 In | 0 Out", this);
    tokenDisplayLabel->setAlignment(Qt::AlignRight);
    tokenDisplayLabel->setStyleSheet("color: gray; font-size: 10px;");

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

    QHBoxLayout* inputLayout = new QHBoxLayout();
    btnAttach = new QPushButton("📎", this);
    btnAttach->setFixedSize(30, 30);
    btnAttach->setToolTip("Attach Files");

    inputField = new QTextEdit(this);
    inputField->setPlaceholderText("Enter command (Shift+Enter for newline), or drag and drop files here...");
    inputField->setMaximumHeight(80); // Prevent the box from taking up the whole screen
    inputField->installEventFilter(this); // Tell Qt to funnel keypresses to our interceptor

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
    
    setAcceptDrops(true); 
}

void MainWindow::initializeConnections() {
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::handleSendClicked);
    connect(btnAttach, &QPushButton::clicked, this, &MainWindow::attachFiles);
    connect(btnClearFiles, &QPushButton::clicked, this, &MainWindow::clearAttachments);
    connect(btnSettings, &QPushButton::clicked, this, &MainWindow::openSettings);
    connect(btnManageSessions, &QPushButton::clicked, this, &MainWindow::switchSession);
    connect(chatDisplay, &QTextBrowser::anchorClicked, this, &MainWindow::handleAnchorClicked);

    // api routes
    connect(apiClient, &GeminiApiClient::responseReceived, this, &MainWindow::onResponseReceived);
    connect(apiClient, &GeminiApiClient::networkError, this, &MainWindow::onNetworkError);
    connect(apiClient, &GeminiApiClient::usageMetricsReceived, this, &MainWindow::onUsageMetricsReceived);
    connect(apiClient, &GeminiApiClient::functionCallsRequested, this, &MainWindow::handleNativeFunctionCalls);
    
    // dynamically bind the command pattern registry back to the main ui loop
    connect(agentController, &AgentActionManager::cleanTextReady, this, &MainWindow::onAgentSystemFeedback);
}

// ============================================================================
// DATABASE HELPER PROXIES
// ============================================================================

void MainWindow::saveInteractionToDb(const QString& role, const QString& content, const QString& apiInteractionId) {
    dbManager->saveInteraction(currentSessionId, role, content, apiInteractionId);
}

bool MainWindow::loadHistoryFromDb() {
    QList<InteractionData> history = dbManager->getInteractions(currentSessionId);
    if (history.isEmpty()) return false;

    QString lastInteractionId;

    // Grab the cursor so we can inject raw text to preserve \n for the syntax engine
    QTextCursor cursor = chatDisplay->textCursor();

    for (const InteractionData& data : history) {
        if (!data.apiId.isEmpty()) {
            lastInteractionId = data.apiId;
        }

        cursor.movePosition(QTextCursor::End);
        chatDisplay->setTextCursor(cursor);

        // Native Qt Block Alignment
        QTextBlockFormat blockFormat;

        if (data.role == "user") {
            blockFormat.setAlignment(Qt::AlignRight);
            cursor.insertBlock(blockFormat); // Push the block right
            chatDisplay->insertHtml("<b>You:</b><br>" + ChatFormatter::formatMarkdownToHtml(data.content) + "<br>");
        } else if (data.role == "model") {
            blockFormat.setAlignment(Qt::AlignLeft);
            cursor.insertBlock(blockFormat); // Reset the block left
            chatDisplay->insertHtml("<b>Agent:</b><br>" + ChatFormatter::formatMarkdownToHtml(data.content) + "<br>");
        } else if (data.role == "system") {
            blockFormat.setAlignment(Qt::AlignLeft);
            cursor.insertBlock(blockFormat); // Reset the block left
            chatDisplay->insertHtml("<span style=\"color: gray;\">" + data.content + "</span><br>");
        }
    }

    chatDisplay->ensureCursorVisible();

    // Pass the state token back to the API client so the LLM remembers the context
    if (!lastInteractionId.isEmpty()) {
        apiClient->restoreSession(lastInteractionId);
    }
    
    return true; 
}

void MainWindow::switchSession() {
    SessionDialog sessionDlg(dbManager, this);
    if (sessionDlg.exec() == QDialog::Accepted) {
        SessionData activeSession = sessionDlg.getSelectedSession();

        if (activeSession.id == currentSessionId) return;

        currentSessionId = activeSession.id;
        currentWorkspacePath = activeSession.workspace;
        setWindowTitle(QString("Gemini Agent - %1").arg(activeSession.name));

        chatDisplay->clear();
        apiClient->resetSession(); 

        bool isExistingSession = loadHistoryFromDb();

        if (!isExistingSession) {
            QString systemInstructions = AgentPromptBuilder::buildSystemInstruction(currentWorkspacePath);
            saveInteractionToDb("system", "System Sandbox Initialized: " + currentWorkspacePath);
            QList<InteractionData> history = dbManager->getInteractions(currentSessionId);
            apiClient->sendPrompt(history, systemInstructions);
        } else {
            QTextCursor cursor = chatDisplay->textCursor();
            cursor.movePosition(QTextCursor::End);
            chatDisplay->setTextCursor(cursor);
            chatDisplay->insertHtml("<span style=\"color: green;\">[System: Switched to existing session]</span><br><br>");
            chatDisplay->ensureCursorVisible();
        }
    }
}

void MainWindow::openSettings() {
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QSettings settings;
        apiClient->setApiKey(settings.value("api_key").toString());
        
        // Use the new cursor method for perfect spacing
        QTextCursor cursor = chatDisplay->textCursor();
        cursor.movePosition(QTextCursor::End);
        chatDisplay->setTextCursor(cursor);
        chatDisplay->insertHtml("<span style=\"color: green;\">[System: Settings updated successfully]</span><br><br>");
        chatDisplay->ensureCursorVisible();
    }
}

// ============================================================================
// MULTI-MODAL & USER INPUT
// ============================================================================

void MainWindow::handleSendClicked() {
    // Upgrade from text() to toPlainText() for the new QTextEdit
    QString userInput = inputField->toPlainText().trimmed(); 
    
    if (!userInput.isEmpty() || !pendingAttachments.isEmpty()) {
        QString displayMsg = userInput;
        if (!pendingAttachments.isEmpty()) {
            displayMsg += QString("\n<i>[Attached %1 file(s)]</i>").arg(pendingAttachments.size());
        }

        QTextCursor cursor = chatDisplay->textCursor();
        cursor.movePosition(QTextCursor::End);
        chatDisplay->setTextCursor(cursor);

        QTextBlockFormat blockFormat;
        blockFormat.setAlignment(Qt::AlignRight);
        cursor.insertBlock(blockFormat);
        
        chatDisplay->insertHtml("<b>You:</b><br>" + ChatFormatter::formatMarkdownToHtml(displayMsg) + "<br>");
        chatDisplay->ensureCursorVisible();
        
        saveInteractionToDb("user", displayMsg);
        
        QList<InteractionData> history = dbManager->getInteractions(currentSessionId);
        apiClient->sendPrompt(history, userInput, pendingAttachments); 
        
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
    QTextCursor cursor = chatDisplay->textCursor();
    cursor.movePosition(QTextCursor::End);
    chatDisplay->setTextCursor(cursor);
    
    QTextBlockFormat blockFormat;
    blockFormat.setAlignment(Qt::AlignLeft);
    cursor.insertBlock(blockFormat);

    chatDisplay->insertHtml("<b>Agent:</b><br>" + ChatFormatter::formatMarkdownToHtml(responseText) + "<br>");
    chatDisplay->ensureCursorVisible();

    saveInteractionToDb("model", responseText, interactionId);
}

void MainWindow::onNetworkError(const QString& errorDetails) {
    chatDisplay->append("<span style=\"color: red;\"><b>Error:</b> " + errorDetails + "</span>");
}

void MainWindow::onUsageMetricsReceived(int inputTokens, int outputTokens, int totalTokens) {
    tokenDisplayLabel->setText(QString("Tokens: %1 In | %2 Out | %3 Total").arg(inputTokens).arg(outputTokens).arg(totalTokens));
}

void MainWindow::handleNativeFunctionCalls(const QJsonArray& toolCalls) {
    isBatchProcessing = true;
    batchSystemFeedback.clear();

    for (int i = 0; i < toolCalls.size(); ++i) {
        QJsonObject callObj = toolCalls[i].toObject();
        QString functionName = callObj["name"].toString();
        QJsonObject arguments = callObj["arguments"].toObject();

        // The manager parses the call, triggers security modals, and executes isolated classes.
        // Because of the UI Modals/Event Loops, this synchronously halts until approved/denied.
        // Every signal emitted during this loop will be caught by onAgentSystemFeedback.
        agentController->processFunctionCall(functionName, arguments, currentWorkspacePath);
    }

    isBatchProcessing = false;

    // --- CRITICAL FIX: Send ONE network response back to the LLM for the entire batch ---
    QString finalFeedback = batchSystemFeedback.trimmed();
    if (!finalFeedback.isEmpty()) {
        saveInteractionToDb("system", finalFeedback);
        QList<InteractionData> history = dbManager->getInteractions(currentSessionId);
        apiClient->sendPrompt(history, finalFeedback);
    }
}

void MainWindow::onAgentSystemFeedback(const QString& feedback) {
    // 1. Ensure the user actually sees the agent's actions in the UI!
    chatDisplay->append(feedback);

    // 2. Strip HTML tags so the LLM doesn't get confused by our UI styling
    QString cleanFeedback = feedback;
    cleanFeedback.replace(QRegularExpression("<[^>]*>"), "");

    // 3. Intercept the network call if we are processing a batch of tools
    if (isBatchProcessing) {
        batchSystemFeedback += cleanFeedback + "\n\n";
    } else {
        saveInteractionToDb("system", cleanFeedback);
        QList<InteractionData> history = dbManager->getInteractions(currentSessionId);
        apiClient->sendPrompt(history, cleanFeedback);
    }
}

// ============================================================================
// HARDWARE INTERCEPTS (KEYBOARD & CLIPBOARD)
// ============================================================================

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj == inputField && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (keyEvent->modifiers() & Qt::ShiftModifier) {
                return false; // Let the QTextEdit naturally insert a newline
            } else {
                handleSendClicked(); // Fire the prompt
                return true; // Consume the event so it doesn't drop a random newline
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::handleAnchorClicked(const QUrl& url) {
    // Check if it's our hidden copy scheme
    if (url.scheme() == "copy") {
        // Strip the "copy:" prefix
        QString payload = url.toString().mid(5); 
        
        // Decode the URL-safe Base64 string back into raw C++/Python
        QByteArray decodedCode = QByteArray::fromBase64(payload.toUtf8(), QByteArray::Base64UrlEncoding);
        
        // Push straight to OS clipboard
        QApplication::clipboard()->setText(QString::fromUtf8(decodedCode));
        
        // Brief visual feedback
        QMessageBox::information(this, "Copied", "Code block copied to clipboard!");
    }
}