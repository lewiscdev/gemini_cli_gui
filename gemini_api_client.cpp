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
    // 1. Define the parameters using UPPERCASE Gemini Schema types
    QJsonObject payloadProp;
    payloadProp["type"] = "STRING"; 
    payloadProp["description"] = "The exact text content to write into the file.";

    QJsonObject targetProp;
    targetProp["type"] = "STRING"; 
    targetProp["description"] = "The file path or name (e.g., test.txt).";

    QJsonObject propertiesObj;
    propertiesObj["payload"] = payloadProp;
    propertiesObj["target"] = targetProp;

    QJsonArray requiredArgs;
    requiredArgs.append("target");
    requiredArgs.append("payload");

    QJsonObject parametersObj;
    parametersObj["type"] = "OBJECT"; 
    parametersObj["properties"] = propertiesObj;
    parametersObj["required"] = requiredArgs;

    // 2. Construct the single, FLATTENED tool object
    QJsonObject toolObj;
    
    // The required discriminator
    toolObj["type"] = "function"; 
    
    // The function details go DIRECTLY on the tool object
    toolObj["name"] = "write_file";
    toolObj["description"] = "Writes content to a local file on the user's system.";
    toolObj["parameters"] = parametersObj;

    QJsonArray toolsArray;
    toolsArray.append(toolObj);

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