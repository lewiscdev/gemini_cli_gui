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
#include <QNetworkReply>
#include <QUrl>

// ============================================================================
// constructor and initialization
// ============================================================================

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

// ============================================================================
// session management
// ============================================================================

void GeminiApiClient::restoreSession(const QString& interactionId) {
    // injects the saved state from sqlite back into the networking client
    currentApiInteractionId = interactionId;
}

void GeminiApiClient::resetSession() {
    // severs the contextual thread so the llm starts with a blank memory slate
    currentApiInteractionId.clear();
}

// ============================================================================
// prompt transmission
// ============================================================================

void GeminiApiClient::sendPrompt(const QList<InteractionData>& history, const QString& text, const QStringList& attachments) {
    if (apiKey.isEmpty()) {
        emit networkError("API Key is missing. Please update your settings.");
        return;
    }

    QJsonArray availableTools = ToolSchemaProvider::getAvailableTools();
    QByteArray finalPayload = GeminiPayloadBuilder::buildRequest(history, text, attachments, availableTools);
    
    QUrl endpoint("https://generativelanguage.googleapis.com/v1beta/interactions?key=" + apiKey);
    QNetworkRequest request(endpoint);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    networkManager->post(request, finalPayload);
}

// ============================================================================
// network callbacks
// ============================================================================

void GeminiApiClient::onNetworkReply(QNetworkReply* reply) {
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError) {
        emit networkError("HTTP Error: " + reply->errorString() + "\nDetails: " + QString::fromUtf8(reply->readAll()));
        return;
    }

    QByteArray responseData = reply->readAll();
    QJsonObject rootObj = QJsonDocument::fromJson(responseData).object();

    // extract the stateful tracking id
    if (rootObj.contains("id")) {
        currentApiInteractionId = rootObj["id"].toString();
    }

    // extract live token consumption metrics (snake_case as per docs)
    if (rootObj.contains("usage")) {
        QJsonObject usage = rootObj["usage"].toObject();
        emit usageMetricsReceived(
            usage["total_input_tokens"].toInt(),
            usage["total_output_tokens"].toInt(),
            usage["total_tokens"].toInt()
        );
    }

    // parse the outputs array for text and tool executions
    if (rootObj.contains("outputs")) {
        QJsonArray outputsArray = rootObj["outputs"].toArray();
        QString combinedText = "";
        QJsonArray requestedTools;

        for (int i = 0; i < outputsArray.size(); ++i) {
            QJsonObject outputObj = outputsArray[i].toObject();
            QString type = outputObj["type"].toString();

            if (type == "text" && outputObj.contains("text")) {
                combinedText += outputObj["text"].toString() + "\n";
            }
            else if (type == "function_call") {
                requestedTools.append(outputObj);
            }
        }

        if (!combinedText.isEmpty()) {
            emit responseReceived(combinedText.trimmed(), currentApiInteractionId);
        }

        if (!requestedTools.isEmpty()) {
            emit functionCallsRequested(requestedTools);
        }
        return;
    }
    else if (rootObj.contains("status") && rootObj["status"].toString() == "completed") {
        // The model successfully thought about the prompt, but decided to return 0 output tokens.
        emit responseReceived("*(The agent awaits further instructions.)*", currentApiInteractionId);
        return;
    }
    
    QString rawJson = QJsonDocument(rootObj).toJson(QJsonDocument::Indented);
    emit networkError("Unexpected response format. Raw Google Response:\n" + rawJson);
}