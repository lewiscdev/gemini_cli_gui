/**
 * @file gemini_api_client.cpp
 * @brief Implementation of the Google Gemini API network client.
 *
 * This file handles asynchronous HTTPS communication. It delegates payload 
 * construction to external providers, allowing it to focus strictly on 
 * transmission, state tracking, and parsing the multi-modal json response.
 */

#include "gemini_api_client.h"
#include "tool_schema_provider.h"
#include "gemini_payload_builder.h"

#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QUrl>

GeminiApiClient::GeminiApiClient(QObject* parent) : QObject(parent) {
    networkManager = new QNetworkAccessManager(this);
    
    // wire the asynchronous network reply to our internal json parser
    connect(networkManager, &QNetworkAccessManager::finished, this, &GeminiApiClient::onNetworkReply);
}

GeminiApiClient::~GeminiApiClient() {
    // qobject hierarchy handles the deletion of networkmanager automatically
}

void GeminiApiClient::setApiKey(const QString& key) {
    apiKey = key;
}

void GeminiApiClient::restoreSession(const QString& interactionId) {
    // injects the saved state from sqlite back into the networking client
    currentApiInteractionId = interactionId;
}

void GeminiApiClient::resetSession() {
    // severs the contextual thread so the llm starts with a blank memory slate
    currentApiInteractionId.clear();
}

void GeminiApiClient::sendPrompt(const QString& text, const QStringList& attachments) {
    if (apiKey.isEmpty()) {
        emit networkError("API Key is missing. Please update your settings.");
        return;
    }

    // securely retrieve our massive tool schema array
    QJsonArray availableTools = ToolSchemaProvider::getAvailableTools();

    // delegate the heavy base64 and mime-type processing to our builder class
    QByteArray rawPayload = GeminiPayloadBuilder::buildRequest(text, attachments, availableTools);
    
    // inject our stateful interaction id to offload memory management to the server
    QJsonObject payloadObj = QJsonDocument::fromJson(rawPayload).object();
    if (!currentApiInteractionId.isEmpty()) {
        payloadObj["previous_interaction_id"] = currentApiInteractionId;
    }
    QByteArray finalPayload = QJsonDocument(payloadObj).toJson(QJsonDocument::Compact);

    // construct the secure rest api endpoint
    QUrl endpoint("https://generativelanguage.googleapis.com/v1beta/interactions?key=" + apiKey);
    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // transmit the payload asynchronously
    networkManager->post(request, finalPayload);
}

void GeminiApiClient::onNetworkReply(QNetworkReply* reply) {
    // queue the reply object for garbage collection to prevent memory leaks
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit networkError("HTTP Error: " + reply->errorString() + "\nDetails: " + QString::fromUtf8(reply->readAll()));
        return;
    }

    QByteArray responseData = reply->readAll();
    QJsonObject rootObj = QJsonDocument::fromJson(responseData).object();

    // update our stateful tracking id if the server provided a new one
    if (rootObj.contains("interaction_id")) {
        currentApiInteractionId = rootObj["interaction_id"].toString();
    }

    // extract and broadcast live token consumption metrics
    if (rootObj.contains("usage_metadata")) {
        QJsonObject usage = rootObj["usage_metadata"].toObject();
        emit usageMetricsReceived(
            usage["prompt_token_count"].toInt(),
            usage["candidates_token_count"].toInt(),
            usage["total_token_count"].toInt()
        );
    }

    // parse the multi-modal outputs array for text and tool executions
    if (rootObj.contains("outputs")) {
        QJsonArray outputsArray = rootObj["outputs"].toArray();
        QString combinedText = "";
        bool actionProcessed = false;

        for (int i = 0; i < outputsArray.size(); ++i) {
            QJsonObject outputObj = outputsArray[i].toObject();
            QString type = outputObj["type"].toString();

            // handle standard text output
            if (type == "text" && outputObj.contains("text")) {
                combinedText += outputObj["text"].toString() + "\n";
                actionProcessed = true;
            }
            // handle autonomous tool request
            else if (type == "function_call") {
                QString funcName = outputObj["name"].toString();
                QJsonObject funcArgs = outputObj["arguments"].toObject();
                
                // signal the main window to execute the local action
                emit functionCallRequested(funcName, funcArgs);
                actionProcessed = true;
            }
        }

        // if the agent replied with text, send it to the ui
        if (!combinedText.isEmpty()) {
            emit responseReceived(combinedText.trimmed(), currentApiInteractionId);
        }

        if (actionProcessed) return;
    }
    
    // fallback error logging if the payload format changes unexpectedly
    QString rawJson = QJsonDocument(rootObj).toJson(QJsonDocument::Indented);
    emit networkError("Unexpected response format. Raw Google Response:\n" + rawJson);
}