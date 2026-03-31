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

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    setupUi();
    initDatabase();
    
    // generate a unique session id for this launch
    currentSessionId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    apiClient = new GeminiApiClient(this);
    agentController = new AgentActionManager(this);
    agentController->addWhitelistedAction("write_file"); 
    
    initializeConnections();

    // check the system registry for an existing key
    QSettings settings;
    QString storedKey = settings.value("api_key", "").toString();

    // if no key is found, force the user to enter one via the modal
    if (storedKey.isEmpty()) {
        SettingsDialog dialog(this);
        if (dialog.exec() == QDialog::Accepted) {
            storedKey = dialog.getApiKey();
        } else {
            // if they close the dialog without saving, disable the chat
            chatDisplay->append("<span style=\"color: red;\">error: application cannot function without an api key. please restart.</span>");
            inputField->setEnabled(false);
            sendButton->setEnabled(false);
            return;
        }
    }

    // inject the secured key into the api client
    apiClient->setApiKey(storedKey);
    
    // inject the system prompt
    QString systemInstructions = 
        "System Configuration: You are an autonomous local assistant running inside a GUI wrapper. "
        "You have the ability to execute local file operations using the provided tools. "
        "Acknowledge these instructions and introduce yourself.";
        
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
        QMessageBox::critical(this, "database error", "failed to open local sqlite database.");
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
        appendStandardError("failed to save interaction to database: " + query.lastError().text());
    }
}

void MainWindow::setupUi() {
    // initialize the central container and its layout manager
    centralWidget = new QWidget(this);
    mainLayout = new QVBoxLayout(centralWidget);

    // instantiate the core interface elements
    chatDisplay = new QTextEdit(this);
    chatDisplay->setReadOnly(true);

    inputField = new QLineEdit(this);
    inputField->setPlaceholderText("enter command or prompt...");

    sendButton = new QPushButton("send", this);

    // assemble the layout structure
    mainLayout->addWidget(chatDisplay);
    mainLayout->addWidget(inputField);
    mainLayout->addWidget(sendButton);

    setCentralWidget(centralWidget);
    setWindowTitle("gemini native api interface");
    resize(800, 600);
}

void MainWindow::initializeConnections() {
    connect(sendButton, &QPushButton::clicked, this, &MainWindow::handleSendClicked);
    connect(inputField, &QLineEdit::returnPressed, sendButton, &QPushButton::click);

    // standard text goes straight to the ui and database
    connect(apiClient, &GeminiApiClient::responseReceived, this, &MainWindow::appendStandardOutput);
    connect(apiClient, &GeminiApiClient::networkError, this, &MainWindow::appendStandardError);
    
    // native function calls are routed to a new handler
    connect(apiClient, &GeminiApiClient::functionCallRequested, this, &MainWindow::handleNativeFunctionCall);
}

void MainWindow::handleSendClicked() {
    QString userInput = inputField->text();
    
    if (!userInput.isEmpty()) {
        chatDisplay->append("<b>you:</b> " + userInput);
        
        // save the user's prompt to the local sqlite database
        saveInteractionToDb("user", userInput);
        
        // send the prompt over the network
        apiClient->sendPrompt(userInput);
        
        inputField->clear();
    }
}

void MainWindow::appendStandardOutput(const QString& text, const QString& interactionId) {
    chatDisplay->append("<b>agent:</b> " + text);
    
    // save the model's response and the new api interaction id to the database
    saveInteractionToDb("model", text, interactionId);
}

void MainWindow::appendStandardError(const QString& text) {
    chatDisplay->append("<span style=\"color: red;\">network error: " + text + "</span>");
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

    int userChoice = msgBox.exec();

    if (userChoice == QMessageBox::Yes) {
        chatDisplay->append("<span style=\"color: green;\">[system: action approved by user. executing...]</span>");
        
        agentController->executeApprovedAction(command);
        
        // feed success back to the llm over the network
        QString successMsg = "System: The user approved the action. The file was written successfully.";
        saveInteractionToDb("system", successMsg);
        apiClient->sendPrompt(successMsg);
        
    } else {
        chatDisplay->append("<span style=\"color: red;\">[system: action denied by user.]</span>");
        
        // feed failure back to the llm
        QString failureMsg = "System: The user DENIED permission for that action. It was not executed.";
        saveInteractionToDb("system", failureMsg);
        apiClient->sendPrompt(failureMsg);
    }
}

void MainWindow::handleNativeFunctionCall(const QString& functionName, const QJsonObject& arguments) {
    // map the structured json back to our c++ execution struct
    AgentCommand command;
    command.action = functionName;
    command.target = arguments["target"].toString();
    command.payload = arguments["payload"].toString();

    // reuse our existing security intercept modal
    handleAgentActionRequest(command);
}