/**
 * @file gemini_payload_builder.cpp
 * @brief Implementation of the Gemini payload builder.
 *
 * Constructs JSON payloads for the Gemini REST API, abstracting multi-modal 
 * requests, base64 file encoding, MIME type detection, and tool schema injection.
 */

#include "gemini_payload_builder.h"
#include "database_manager.h" // required for the InteractionData struct definition

#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QFile>
#include <QMimeDatabase>

// ============================================================================
// payload construction
// ============================================================================

QByteArray GeminiPayloadBuilder::buildRequest(const QList<InteractionData>& history, const QString& text, const QStringList& attachments, const QJsonArray& tools) {
    QJsonObject rootObj;
    rootObj["model"] = "gemini-2.5-flash"; 

    QJsonArray inputArray;

    // pack the sqlite chat history as explicit "turn" objects for the api
    for (const auto& interaction : history) {
        QString apiRole = (interaction.role == "model") ? "model" : "user";
        
        QJsonObject textPart;
        textPart["type"] = "text";
        textPart["text"] = interaction.content;
        
        QJsonArray contentArray;
        contentArray.append(textPart);
        
        QJsonObject turnObj;
        turnObj["role"] = apiRole;
        turnObj["content"] = contentArray;
        
        inputArray.append(turnObj);
    }

    // pack the current turn (combining text and multi-modal files)
    QJsonArray currentContentArray;
    if (!text.isEmpty()) {
        QJsonObject textPart;
        textPart["type"] = "text";
        textPart["text"] = text;
        currentContentArray.append(textPart);
    }

    for (const QString& filePath : attachments) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            QString mimeType = getMimeType(filePath);
            
            // if the file is text-based, read it directly into the prompt string
            if (mimeType.startsWith("text/") || mimeType == "application/json" || mimeType.isEmpty()) {
                QJsonObject textPart;
                textPart["type"] = "text";
                textPart["text"] = QString("\n--- FILE: %1 ---\n%2\n").arg(QFileInfo(filePath).fileName(), QString::fromUtf8(file.readAll()));
                currentContentArray.append(textPart);
            } else {
                // if it is an image, encode it as a base64 inline object
                QJsonObject imgPart;
                imgPart["type"] = "image";
                imgPart["data"] = QString::fromLatin1(file.readAll().toBase64());
                imgPart["mime_type"] = mimeType;
                currentContentArray.append(imgPart);
            }
            file.close();
        }
    }

    if (!currentContentArray.isEmpty()) {
        QJsonObject currentTurnObj;
        currentTurnObj["role"] = "user";
        currentTurnObj["content"] = currentContentArray;
        inputArray.append(currentTurnObj);
    }

    rootObj["input"] = inputArray;

    if (!tools.isEmpty()) {
        rootObj["tools"] = tools;
    }

    return QJsonDocument(rootObj).toJson(QJsonDocument::Compact);
}

// ============================================================================
// mime type detection
// ============================================================================

QString GeminiPayloadBuilder::getMimeType(const QString& filePath) {
    QMimeDatabase mimeDb;
    QMimeType mime = mimeDb.mimeTypeForFile(filePath);
    return mime.name();
}