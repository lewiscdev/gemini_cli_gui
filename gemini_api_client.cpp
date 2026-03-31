#include "gemini_api_client.h"
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonValue>
#include <QUrl>

GeminiApiClient::GeminiApiClient(QObject* parent) : QObject(parent) {
    networkManager = new QNetworkAccessManager(this);
    
    // wire the network manager's finished signal to our internal parser
    connect(networkManager, &QNetworkAccessManager::finished, this, &GeminiApiClient::onNetworkReply);
}

GeminiApiClient::~GeminiApiClient() {
    // QObject hierarchy handles the networkManager cleanup automatically
}

void GeminiApiClient::setApiKey(const QString& key) {
    apiKey = key;
}

void GeminiApiClient::restoreSession(const QString& interactionId) {
    // Injects the saved state from SQLite back into the networking client
    currentApiInteractionId = interactionId;
}

void GeminiApiClient::resetSession() {
    // clears the current interaction to start a fresh thread
    currentApiInteractionId.clear();
}

void GeminiApiClient::sendPrompt(const QString& userInput) {
    if (apiKey.isEmpty()) {
        emit networkError("API Key is missing. Please configure it in the settings.");
        return;
    }

    QNetworkRequest request = createRequest();
    QByteArray payload = buildJsonPayload(userInput);

    // execute the asynchronous POST request
    networkManager->post(request, payload);
}

QNetworkRequest GeminiApiClient::createRequest() const {
    // construct the official v1beta interactions endpoint url
    QString endpoint = "https://generativelanguage.googleapis.com/v1beta/interactions?key=" + apiKey;
    
    QNetworkRequest request((QUrl(endpoint)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    return request;
}

QJsonArray GeminiApiClient::defineAvailableTools() const {
    QJsonArray toolsArray;

    // --- TOOL 1: write_file ---
    QJsonObject wfPayload; wfPayload["type"] = "STRING"; wfPayload["description"] = "The exact text content to write.";
    QJsonObject wfTarget; wfTarget["type"] = "STRING"; wfTarget["description"] = "Relative file path (e.g., src/main.cpp).";
    QJsonObject wfProps; wfProps["payload"] = wfPayload; wfProps["target"] = wfTarget;
    QJsonArray wfReq; wfReq.append("target"); wfReq.append("payload");
    QJsonObject wfParams; wfParams["type"] = "OBJECT"; wfParams["properties"] = wfProps; wfParams["required"] = wfReq;
    
    QJsonObject writeFileTool;
    writeFileTool["type"] = "function"; // The critical flattened schema discriminator!
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
    QJsonObject escCommand; 
    escCommand["type"] = "STRING"; 
    escCommand["description"] = "The terminal/shell command to execute (e.g., 'git status', 'cmake --build .', 'npm run test').";
    
    QJsonObject escProps; 
    escProps["command"] = escCommand;
    
    QJsonArray escReq; 
    escReq.append("command");
    
    QJsonObject escParams; 
    escParams["type"] = "OBJECT"; 
    escParams["properties"] = escProps; 
    escParams["required"] = escReq;

    QJsonObject executeShellTool;
    executeShellTool["type"] = "function"; // Flattened schema discriminator!
    executeShellTool["name"] = "execute_shell_command";
    executeShellTool["description"] = "Executes a shell/terminal command in the workspace and returns the console output (stdout/stderr).";
    executeShellTool["parameters"] = escParams;
    toolsArray.append(executeShellTool);

    return toolsArray;
}

QByteArray GeminiApiClient::buildJsonPayload(const QString& newPrompt) {
    QJsonObject payloadObject;
    
    // specify the required model
    payloadObject["model"] = "gemini-2.5-flash"; 
    payloadObject["input"] = newPrompt;
    
    // handle the stateful connection logic (Replaces chatHistory array)
    if (currentApiInteractionId.isEmpty()) {
        payloadObject["store"] = true;
    } else {
        payloadObject["previous_interaction_id"] = currentApiInteractionId;
    }
    
    // attach our defined tools to every request
    payloadObject["tools"] = defineAvailableTools();
    
    return QJsonDocument(payloadObject).toJson(QJsonDocument::Compact);
}

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
    
    if (rootObj.contains("id")) {
        currentApiInteractionId = rootObj["id"].toString();
    }
    
    if (rootObj.contains("usage")) {
        QJsonObject usageObj = rootObj["usage"].toObject();
        int inputTokens = usageObj["total_input_tokens"].toInt();
        int outputTokens = usageObj["total_output_tokens"].toInt();
        int totalTokens = usageObj["total_tokens"].toInt();
        
        emit usageMetricsReceived(inputTokens, outputTokens, totalTokens);
    }

    if (rootObj.contains("outputs")) {
        QJsonArray outputsArray = rootObj["outputs"].toArray();
        QString combinedText = "";
        bool actionProcessed = false;

        for (int i = 0; i < outputsArray.size(); ++i) {
            QJsonObject outputObj = outputsArray[i].toObject();
            QString type = outputObj["type"].toString();

            // 1. Handle Text Output
            if (type == "text" && outputObj.contains("text")) {
                combinedText += outputObj["text"].toString() + "\n";
                actionProcessed = true;
            }
            // 2. Handle Function/Tool Call
            else if (type == "function_call") {
                QString funcName = outputObj["name"].toString();
                QJsonObject funcArgs = outputObj["arguments"].toObject();
                
                // Signal the UI to show the confirmation dialog
                emit functionCallRequested(funcName, funcArgs);
                actionProcessed = true;
            }
        }

        if (!combinedText.isEmpty()) {
            emit responseReceived(combinedText.trimmed(), currentApiInteractionId);
        }

        if (actionProcessed) return;
    }
    
    QString rawJson = jsonDoc.toJson(QJsonDocument::Indented);
    emit networkError("Unexpected response format. Raw Google Response:\n" + rawJson);
}