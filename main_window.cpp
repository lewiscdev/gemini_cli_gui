/**
 * @file main_window.cpp
 * @brief implementation of the primary gui and agent orchestration logic.
 *
 * this file handles ui rendering, multi-modal drag-and-drop, and api routing.
 * all complex agent actions (like shell execution, ftp uploads, and screenshots) 
 * are entirely offloaded to the isolated command pattern action classes.
 */

#include "main_window.h"
#include "gemini_api_client.h"
#include "settings_dialog.h"
#include "session_dialog.h"
#include "agent_prompt_builder.h"
#include "chat_formatter.h"
#include "theme_manager.h"

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

// ============================================================================
// constructor and initialization
// ============================================================================

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUi();
    applyTheme();
    
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

    // global settings check
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

    // session manager launch
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
        chatDisplay->append(formatChatBubble("system", "[System: Session Restored Successfully]", isDarkTheme));
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
    
    // we must disable auto-open so we can intercept the copy click
    chatDisplay->setOpenLinks(false); 

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
    btnAttach->setObjectName("iconButton");

    inputField = new QTextEdit(this);
    inputField->setPlaceholderText("Enter command (Shift+Enter for newline), or drag and drop files here...");
    
    // prevent the box from taking up the whole screen
    inputField->setMaximumHeight(80); 
    
    // tell qt to funnel keypresses to our interceptor
    inputField->installEventFilter(this); 

    sendButton = new QPushButton("Send", this);

    // tell both buttons to stretch vertically
    btnAttach->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);
    sendButton->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Expanding);

    // the hard ceiling: force them to stop growing at the exact same height as the text box
    btnAttach->setMaximumHeight(80); 
    sendButton->setMaximumHeight(80);

    // add them directly side-by-side
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
// database helper proxies
// ============================================================================

void MainWindow::saveInteractionToDb(const QString& role, const QString& content, const QString& apiInteractionId) {
    dbManager->saveInteraction(currentSessionId, role, content, apiInteractionId);
}

bool MainWindow::loadHistoryFromDb() {
    QList<InteractionData> history = dbManager->getInteractions(currentSessionId);
    if (history.isEmpty()) return false;

    QString lastInteractionId;

    // grab the cursor so we can inject raw text to preserve \n for the syntax engine
    QTextCursor cursor = chatDisplay->textCursor();

    for (const InteractionData& data : history) {
        if (!data.apiId.isEmpty()) { lastInteractionId = data.apiId; }
        chatDisplay->append(formatChatBubble(data.role, data.content, isDarkTheme));
    }

    chatDisplay->ensureCursorVisible();

    // pass the state token back to the api client so the llm remembers the context
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
            chatDisplay->append(formatChatBubble("system", "[System: Switched to existing session]", isDarkTheme));
        }
    }
}

void MainWindow::openSettings() {
    SettingsDialog dialog(this);
    if (dialog.exec() == QDialog::Accepted) {
        QSettings settings;
        apiClient->setApiKey(settings.value("api_key").toString());
        
        // apply new colors immediately
        applyTheme(); 
        
        // redraw the chat history with the new html bubbles
        chatDisplay->clear();
        loadHistoryFromDb();
        
        chatDisplay->append(formatChatBubble("system", "[System: Settings & Theme updated successfully]", isDarkTheme));
    }
}

// ============================================================================
// multi-modal & user input
// ============================================================================

void MainWindow::handleSendClicked() {
    // upgrade from text() to toplaintext() for the new qtextedit
    QString userInput = inputField->toPlainText().trimmed(); 
    
    if (!userInput.isEmpty() || !pendingAttachments.isEmpty()) {
        QString displayMsg = userInput;
        if (!pendingAttachments.isEmpty()) {
            displayMsg += QString("\n<i>[Attached %1 file(s)]</i>").arg(pendingAttachments.size());
        }

        chatDisplay->append(formatChatBubble("user", displayMsg, isDarkTheme));
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
// api callbacks
// ============================================================================

void MainWindow::onResponseReceived(const QString& responseText, const QString& interactionId) {
    chatDisplay->append(formatChatBubble("model", responseText, isDarkTheme));
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

        agentController->processFunctionCall(functionName, arguments, currentWorkspacePath);
    }

    isBatchProcessing = false;

    QString finalFeedback = batchSystemFeedback.trimmed();
    if (!finalFeedback.isEmpty()) {
        saveInteractionToDb("system", finalFeedback);
        QList<InteractionData> history = dbManager->getInteractions(currentSessionId);
        
        // attachment pipeline injection
        QStringList systemAttachments;
        QString screenshotPath = QDir(currentWorkspacePath).absoluteFilePath("latest_agent_screenshot.png");
        
        if (QFile::exists(screenshotPath)) {
            // add the image
            systemAttachments.append(screenshotPath); 
        }

        // send the batch response with the attachments
        apiClient->sendPrompt(history, finalFeedback, systemAttachments);

        // clean up the image
        if (QFile::exists(screenshotPath)) {
            QFile::remove(screenshotPath);
        }
    }
}

void MainWindow::onAgentSystemFeedback(const QString& feedback) {
    // trap visual fluff: print it to the ui, but do not send to the api or database
    if (feedback.startsWith("UI_ONLY:")) {
        chatDisplay->append(feedback.mid(8));
        return; 
    }

    // safely separate the ui rendering from the api payload (no dangerous regex)
    QString displayFeedback = feedback;
    QString apiFeedback = feedback;

    if (feedback.contains("Visual verification captured.")) {
        int splitIndex = feedback.indexOf("<br><br><img");
        if (splitIndex != -1) {
            // strip the html image tag for the api, but keep it for the ui display
            apiFeedback = feedback.left(splitIndex) + " [Screenshot Attached to Payload]";
        }
    }

    // draw the system bubble in the ui
    chatDisplay->append(formatChatBubble("system", displayFeedback, isDarkTheme));

    // send the clean api data back to gemini
    if (isBatchProcessing) {
        batchSystemFeedback += apiFeedback + "\n\n";
    } else {
        saveInteractionToDb("system", apiFeedback);
        QList<InteractionData> history = dbManager->getInteractions(currentSessionId);
        
        QStringList systemAttachments;
        QString screenshotPath = QDir(currentWorkspacePath).absoluteFilePath("latest_agent_screenshot.png");
        
        if (QFile::exists(screenshotPath)) {
            systemAttachments.append(screenshotPath); 
        }

        apiClient->sendPrompt(history, apiFeedback, systemAttachments);

        if (QFile::exists(screenshotPath)) {
            QFile::remove(screenshotPath);
        }
    }
}

// ============================================================================
// hardware intercepts (keyboard & clipboard)
// ============================================================================

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
    if (obj == inputField && event->type() == QEvent::KeyPress) {
        QKeyEvent *keyEvent = static_cast<QKeyEvent *>(event);
        
        if (keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
            if (keyEvent->modifiers() & Qt::ShiftModifier) {
                // let the qtextedit naturally insert a newline
                return false; 
            } else {
                // fire the prompt
                handleSendClicked(); 
                // consume the event so it doesn't drop a random newline
                return true; 
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

void MainWindow::handleAnchorClicked(const QUrl& url) {
    // check if it's our hidden copy scheme
    if (url.scheme() == "copy") {
        // strip the "copy:" prefix
        QString payload = url.toString().mid(5); 
        
        // decode the url-safe base64 string back into raw c++/python
        QByteArray decodedCode = QByteArray::fromBase64(payload.toUtf8(), QByteArray::Base64UrlEncoding);
        
        // push straight to os clipboard
        QApplication::clipboard()->setText(QString::fromUtf8(decodedCode));
        
        // brief visual feedback
        QMessageBox::information(this, "Copied", "Code block copied to clipboard!");
    }
}

// ============================================================================
// theme & ui rendering engine
// ============================================================================

void MainWindow::applyTheme() {
    bool dark = ThemeManager::isDark();
    
    // update internal bool for the chatformatter
    this->isDarkTheme = dark; 
    
    // apply the global app style
    this->setStyleSheet(ThemeManager::getStyleSheet(dark));
    
    // specifically fix the chat display's viewport background
    QPalette p = chatDisplay->palette();
    p.setColor(QPalette::Base, dark ? QColor("#0F172A") : QColor("#F8FAFC"));
    p.setColor(QPalette::Text, dark ? QColor("#F8FAFC") : QColor("#0F172A"));
    chatDisplay->setPalette(p);
}

QString MainWindow::formatChatBubble(const QString& role, const QString& content, bool isDark) {
    QString bubbleBg;
    QString textColor;
    QString borderColor;
    
    // define a high-contrast palette
    if (isDark) {
        bubbleBg = (role == "user") ? "#334155" : "#020617"; 
        borderColor = "#475569"; 
        textColor = "#F8FAFC";
    } else {
        bubbleBg = (role == "user") ? "#E2E8F0" : "#F8F9FA"; 
        borderColor = "#CBD5E1"; 
        textColor = "#0F172A";
    }
    
    QString name = (role == "user") ? "You" : "Agent";
    QString parsedContent = ChatFormatter::formatMarkdownToHtml(content, isDark);

    // the unified table structure
    QString html = "<table width=\"100%\" cellspacing=\"10\" cellpadding=\"0\"><tr>";
    
    QString bubbleStyle = QString("background-color: %1; border: 1px solid %2;").arg(bubbleBg, borderColor);

    if (role == "user") {
        // push right
        html += "<td width=\"20%\"></td>"; 
        html += QString("<td width=\"80%\" style=\"%1\">"
                        "<table width=\"100%\" cellpadding=\"14\" cellspacing=\"0\">"
                        "<tr><td style=\"color: %2;\"><b>%3</b><br><br>%4</td></tr>"
                        "</table></td>").arg(bubbleStyle, textColor, name, parsedContent);
    } else if (role == "model") {
        html += QString("<td width=\"80%\" style=\"%1\">"
                        "<table width=\"100%\" cellpadding=\"14\" cellspacing=\"0\">"
                        "<tr><td style=\"color: %2;\"><b>%3</b><br><br>%4</td></tr>"
                        "</table></td>").arg(bubbleStyle, textColor, name, parsedContent);
        // push left
        html += "<td width=\"20%\"></td>"; 
    } else {
        // the new system bar
        QString barBg = isDark ? "#1E293B" : "#F8FAFC";
        QString barBorder = isDark ? "#334155" : "#E2E8F0";
        QString systemColor = isDark ? "#94A3B8" : "#64748B";
        
        // use colspan="2" to span the entire chat width evenly
        html += QString("<td colspan=\"2\" align=\"center\">"
                        "<table width=\"100%\" cellpadding=\"8\" cellspacing=\"0\" style=\"background-color: %1; border: 1px solid %2; border-radius: 6px;\">"
                        "<tr><td align=\"center\" style=\"color: %3; font-size: 11px; font-weight: bold; letter-spacing: 0.5px;\">%4</td></tr>"
                        "</table></td>").arg(barBg, barBorder, systemColor, content);
    }
    
    html += "</tr></table>";
    return html;
}