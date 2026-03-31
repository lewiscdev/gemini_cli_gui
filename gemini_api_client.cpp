/**
 * @file gemini_api_client.cpp
 * @brief Implementation of the Google Gemini API network client.
 *
 * This file handles asynchronous HTTPS communication with the Gemini v1beta API.
 * It is responsible for constructing JSON payloads, injecting multi-modal file 
 * attachments, defining the local agent's tool schema, and parsing Google's responses.
 */

#include "gemini_api_client.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonValue>
#include <QNetworkRequest>
#include <QUrl>

/**
 * @brief Constructs the API client and binds the network response signals.
 */
GeminiApiClient::GeminiApiClient(QObject* parent) : QObject(parent) {
    networkManager = new QNetworkAccessManager(this);
    
    // Wire the asynchronous network reply to our internal JSON parser
    connect(networkManager, &QNetworkAccessManager::finished, this, &GeminiApiClient::onNetworkReply);
}

GeminiApiClient::~GeminiApiClient() {
    // QObject hierarchy handles the deletion of networkManager automatically
}

// ============================================================================
// CONFIGURATION & SESSION MANAGEMENT
// ============================================================================

void GeminiApiClient::setApiKey(const QString& key) {
    apiKey = key;
}

void GeminiApiClient::restoreSession(const QString& interactionId) {
    // Injects the saved state from SQLite back into the networking client
    currentApiInteractionId = interactionId;
}

void GeminiApiClient::resetSession() {
    // Clears the current interaction ID so the next request starts a fresh Google thread
    currentApiInteractionId.clear();
}

// ============================================================================
// CORE EXECUTION & PAYLOAD CONSTRUCTION
// ============================================================================

/**
 * @brief Packages the user input and any attachments, then transmits the POST request.
 */
void GeminiApiClient::sendPrompt(const QString& userInput, const QStringList& attachments) {
    if (apiKey.isEmpty()) {
        emit networkError("API Key is missing. Please configure it in the settings.");
        return;
    }

    QNetworkRequest request = createRequest();
    QByteArray payload = buildJsonPayload(userInput, attachments);

    // Execute the asynchronous POST request
    networkManager->post(request, payload);
}

/**
 * @brief Configures the HTTP headers and targets the v1beta interactions endpoint.
 */
QNetworkRequest GeminiApiClient::createRequest() const {
    QString endpoint = "https://generativelanguage.googleapis.com/v1beta/interactions?key=" + apiKey;
    
    QNetworkRequest request((QUrl(endpoint)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    return request;
}

/**
 * @brief Assembles the JSON payload, handles the stateful connection ID, and injects local files.
 */
QByteArray GeminiApiClient::buildJsonPayload(const QString& newPrompt, const QStringList& attachments) {
    QJsonObject payloadObject;
    payloadObject["model"] = "gemini-2.5-flash"; 
    
    QString finalPrompt = newPrompt;
    
    // --- MULTI-MODAL FILE INJECTION & BINARY FILTER ---
    // Define safe, text-based extensions to prevent sending massive binary blobs to the LLM
    QStringList allowedExtensions = {"cpp", "h", "c", "hpp", "txt", "cmake", "json", "xml", "js", "html", "css", "md", "sh", "py", "bat"};

    for (const QString& filePath : attachments) {
        QFileInfo fileInfo(filePath);
        QString ext = fileInfo.suffix().toLower();
        
        // Filter out images, executables, and unsupported binaries
        if (!allowedExtensions.contains(ext)) {
            finalPrompt += QString("\n\n[System Note: Attached file '%1' was ignored. Binary/Image formats are filtered to save tokens.]\n").arg(fileInfo.fileName());
            continue;
        }

        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString content = file.readAll();
            // Wrap the file content in clear delimiters so the LLM understands the context
            finalPrompt += QString("\n\n=== [ATTACHED FILE: %1] ===\n%2\n=========================\n")
                           .arg(fileInfo.fileName(), content);
            file.close();
        } else {
            finalPrompt += QString("\n\n[System Note: Failed to read attached file: %1]\n").arg(fileInfo.fileName());
        }
    }
    
    payloadObject["input"] = finalPrompt;
    
    // --- STATEFUL CONNECTION LOGIC ---
    if (currentApiInteractionId.isEmpty()) {
        payloadObject["store"] = true; // Tell Google to start a new memory thread
    } else {
        payloadObject["previous_interaction_id"] = currentApiInteractionId; // Continue existing thread
    }
    
    // Attach our native C++ tools to every outgoing request
    payloadObject["tools"] = defineAvailableTools();
    
    return QJsonDocument(payloadObject).toJson(QJsonDocument::Compact);
}

// ============================================================================
// AGENT TOOL SCHEMA DEFINITIONS
// ============================================================================

/**
 * @brief Defines the flattened JSON schema for all available agent capabilities.
 */
QJsonArray GeminiApiClient::defineAvailableTools() const {
    QJsonArray toolsArray;

    // --- TOOL 1: write_file ---
    QJsonObject wfPayload; wfPayload["type"] = "STRING"; wfPayload["description"] = "The exact text content to write.";
    QJsonObject wfTarget; wfTarget["type"] = "STRING"; wfTarget["description"] = "Relative file path (e.g., src/main.cpp).";
    QJsonObject wfProps; wfProps["payload"] = wfPayload; wfProps["target"] = wfTarget;
    QJsonArray wfReq; wfReq.append("target"); wfReq.append("payload");
    QJsonObject wfParams; wfParams["type"] = "OBJECT"; wfParams["properties"] = wfProps; wfParams["required"] = wfReq;
    
    QJsonObject writeFileTool;
    writeFileTool["type"] = "function"; 
    writeFileTool["name"] = "write_file";
    writeFileTool["description"] = "Writes content to a local file in the workspace.";
    writeFileTool["parameters"] = wfParams;
    toolsArray.append(writeFileTool);

    // --- TOOL 2: read_file ---
    QJsonObject rfTarget; rfTarget["type"] = "STRING"; rfTarget["description"] = "Relative file path to read (e.g., CMakeLists.txt).";
    QJsonObject rfProps; rfProps["target"] = rfTarget;
    QJsonArray rfReq; rfReq.append("target");
    QJsonObject rfParams; rfParams["type"] = "OBJECT"; rfParams["properties"] = rfProps; rfParams["required"] = rfReq;

    QJsonObject readFileTool;
    readFileTool["type"] = "function"; 
    readFileTool["name"] = "read_file";
    readFileTool["description"] = "Reads and returns the exact text content of a local file.";
    readFileTool["parameters"] = rfParams;
    toolsArray.append(readFileTool);

    // --- TOOL 3: list_directory ---
    QJsonObject ldTarget; ldTarget["type"] = "STRING"; ldTarget["description"] = "Relative directory path to list. Use '.' for the root workspace.";
    QJsonObject ldProps; ldProps["target"] = ldTarget;
    QJsonArray ldReq; ldReq.append("target");
    QJsonObject ldParams; ldParams["type"] = "OBJECT"; ldParams["properties"] = ldProps; ldParams["required"] = ldReq;

    QJsonObject listDirTool;
    listDirTool["type"] = "function"; 
    listDirTool["name"] = "list_directory";
    listDirTool["description"] = "Lists all files and folders in a specified directory.";
    listDirTool["parameters"] = ldParams;
    toolsArray.append(listDirTool);

    // --- TOOL 4: execute_shell_command ---
    QJsonObject escCommand; escCommand["type"] = "STRING"; escCommand["description"] = "The terminal/shell command to execute (e.g., 'git status', 'cmake --build .', 'npm run test').";
    QJsonObject escProps; escProps["command"] = escCommand;
    QJsonArray escReq; escReq.append("command");
    QJsonObject escParams; escParams["type"] = "OBJECT"; escParams["properties"] = escProps; escParams["required"] = escReq;

    QJsonObject executeShellTool;
    executeShellTool["type"] = "function"; 
    executeShellTool["name"] = "execute_shell_command";
    executeShellTool["description"] = "Executes a shell/terminal command in the workspace and returns the console output (stdout/stderr).";
    executeShellTool["parameters"] = escParams;
    toolsArray.append(executeShellTool);

    // --- TOOL 5: upload_ftp ---
    QJsonObject ftpLocal; ftpLocal["type"] = "STRING"; ftpLocal["description"] = "Relative path of the local file to upload.";
    QJsonObject ftpRemote; ftpRemote["type"] = "STRING"; ftpRemote["description"] = "The destination directory/file on the server (e.g., /public_html/index.html).";
    
    QJsonObject ftpProps; ftpProps["local_path"] = ftpLocal; ftpProps["remote_path"] = ftpRemote; 
    QJsonArray ftpReq; ftpReq.append("local_path"); ftpReq.append("remote_path");
    QJsonObject ftpParams; ftpParams["type"] = "OBJECT"; ftpParams["properties"] = ftpProps; ftpParams["required"] = ftpReq;

    QJsonObject uploadFtpTool;
    uploadFtpTool["type"] = "function"; 
    uploadFtpTool["name"] = "upload_ftp";
    uploadFtpTool["description"] = "Uploads a local file to the pre-configured FTP server. You only need to provide the local file and remote destination path.";
    uploadFtpTool["parameters"] = ftpParams;
    toolsArray.append(uploadFtpTool);

    // --- TOOL 6: fetch_webpage ---
    QJsonObject fwUrl; fwUrl["type"] = "STRING"; fwUrl["description"] = "The full HTTP/HTTPS URL of the webpage to fetch (e.g., https://my-test-site.com/index.html).";
    QJsonObject fwProps; fwProps["url"] = fwUrl;
    QJsonArray fwReq; fwReq.append("url");
    QJsonObject fwParams; fwParams["type"] = "OBJECT"; fwParams["properties"] = fwProps; fwParams["required"] = fwReq;

    QJsonObject fetchWebpageTool;
    fetchWebpageTool["type"] = "function"; 
    fetchWebpageTool["name"] = "fetch_webpage";
    fetchWebpageTool["description"] = "Fetches the raw HTML/text content of a live webpage. Useful for verifying deployments.";
    fetchWebpageTool["parameters"] = fwParams;
    toolsArray.append(fetchWebpageTool);

    // --- TOOL 7: take_screenshot ---
    QJsonObject tsParams; tsParams["type"] = "OBJECT"; tsParams["properties"] = QJsonObject(); 
    
    QJsonObject screenshotTool;
    screenshotTool["type"] = "function"; 
    screenshotTool["name"] = "take_screenshot";
    screenshotTool["description"] = "Takes a screenshot of the active GUI application you just executed to visually verify the UI layout or read errors.";
    screenshotTool["parameters"] = tsParams;
    toolsArray.append(screenshotTool);

    // --- TOOL 8: execute_code ---
    QJsonObject ecLang; ecLang["type"] = "STRING"; ecLang["description"] = "The programming language to execute (supported: 'python', 'javascript', 'cpp').";
    QJsonObject ecCode; ecCode["type"] = "STRING"; ecCode["description"] = "The raw source code to execute.";
    QJsonObject ecProps; ecProps["language"] = ecLang; ecProps["code"] = ecCode;
    QJsonArray ecReq; ecReq.append("language"); ecReq.append("code");
    QJsonObject ecParams; ecParams["type"] = "OBJECT"; ecParams["properties"] = ecProps; ecParams["required"] = ecReq;

    QJsonObject executeCodeTool;
    executeCodeTool["type"] = "function"; 
    executeCodeTool["name"] = "execute_code";
    executeCodeTool["description"] = "Executes raw source code in a secure local sandbox and returns the stdout/stderr output.";
    executeCodeTool["parameters"] = ecParams;
    toolsArray.append(executeCodeTool);

    return toolsArray;
}

// ============================================================================
// NETWORK RESPONSE PARSER
// ============================================================================

/**
 * @brief Parses the asynchronous JSON response from the Gemini API.
 */
void GeminiApiClient::onNetworkReply(QNetworkReply* reply) {
    if (reply->error() != QNetworkReply::NoError) {
        QByteArray errorBody = reply->readAll();
        QString detailedError = reply->errorString();
        if (!errorBody.isEmpty()) {
            detailedError += "\nGoogle API Details: " + QString(errorBody);
        }
        emit networkError(detailedError);
        reply->deleteLater();
        return;
    }

    QByteArray responseData = reply->readAll();
    reply->deleteLater();

    QJsonDocument jsonDoc = QJsonDocument::fromJson(responseData);
    if (!jsonDoc.isObject()) {
        emit networkError("Failed to parse JSON response from Google.");
        return;
    }

    QJsonObject rootObj = jsonDoc.object();
    
    // Save the interaction ID to maintain conversational memory on the server
    if (rootObj.contains("id")) {
        currentApiInteractionId = rootObj["id"].toString();
    }
    
    // Emit token usage for the UI overlay
    if (rootObj.contains("usage")) {
        QJsonObject usageObj = rootObj["usage"].toObject();
        int inputTokens = usageObj["total_input_tokens"].toInt();
        int outputTokens = usageObj["total_output_tokens"].toInt();
        int totalTokens = usageObj["total_tokens"].toInt();
        
        emit usageMetricsReceived(inputTokens, outputTokens, totalTokens);
    }

    // Parse the actual response payload (Text or Tool Call)
    if (rootObj.contains("outputs")) {
        QJsonArray outputsArray = rootObj["outputs"].toArray();
        QString combinedText = "";
        bool actionProcessed = false;

        for (int i = 0; i < outputsArray.size(); ++i) {
            QJsonObject outputObj = outputsArray[i].toObject();
            QString type = outputObj["type"].toString();

            // Handle Standard Text Output
            if (type == "text" && outputObj.contains("text")) {
                combinedText += outputObj["text"].toString() + "\n";
                actionProcessed = true;
            }
            // Handle Autonomous Tool Request
            else if (type == "function_call") {
                QString funcName = outputObj["name"].toString();
                QJsonObject funcArgs = outputObj["arguments"].toObject();
                
                // Signal the Main Window to execute the local action
                emit functionCallRequested(funcName, funcArgs);
                actionProcessed = true;
            }
        }

        // If the agent replied with text, send it to the UI
        if (!combinedText.isEmpty()) {
            emit responseReceived(combinedText.trimmed(), currentApiInteractionId);
        }

        if (actionProcessed) return;
    }
    
    // Fallback error logging if the payload format changes unexpectedly
    QString rawJson = jsonDoc.toJson(QJsonDocument::Indented);
    emit networkError("Unexpected response format. Raw Google Response:\n" + rawJson);
}