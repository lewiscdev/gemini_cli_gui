/**
 * @file gemini_payload_builder.cpp
 * @brief Implementation of the Gemini payload builder.
 *
 * Handles the reading of local files, base64 encoding for images, 
 * raw text extraction for code files, and the strict formatting 
 * of the Google Gemini REST API json payload.
 */

#include "gemini_payload_builder.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QFileInfo>
#include <QFile>
#include <QMimeDatabase>

QByteArray GeminiPayloadBuilder::buildRequest(const QString& text, const QStringList& attachments, const QJsonArray& tools) {
    QJsonObject rootObj;
    QJsonArray partsArray;

    // --- Explicitly define the target LLM ---
    rootObj["model"] = "gemini-2.5-flash"; 

    // Append the primary text prompt directly to the flattened array
    if (!text.isEmpty()) {
        QJsonObject textPart;
        textPart["text"] = text;
        partsArray.append(textPart);
    }

    // Process and append any multi-modal file attachments
    for (const QString& filePath : attachments) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            QString mimeType = getMimeType(filePath);
            
            // If it's a text file, append as raw text to save vision tokens
            if (mimeType.startsWith("text/") || mimeType == "application/json" || mimeType.isEmpty()) {
                QString fileContent = QString::fromUtf8(file.readAll());
                QJsonObject textPart;
                textPart["text"] = QString("\n--- FILE: %1 ---\n%2\n").arg(QFileInfo(filePath).fileName(), fileContent);
                partsArray.append(textPart);
            } 
            // If it's an image or binary, base64 encode it for the vision model
            else {
                QByteArray fileData = file.readAll();
                QJsonObject inlineDataObj;
                inlineDataObj["mime_type"] = mimeType;
                inlineDataObj["data"] = QString::fromLatin1(fileData.toBase64());
                
                QJsonObject partObj;
                partObj["inline_data"] = inlineDataObj;
                partsArray.append(partObj);
            }
            file.close();
        }
    }

    // --- CRITICAL FIX: The v1beta Interactions API is stateful and flattened. ---
    // We do not wrap this in 'roles' or 'contents'. We just pass the parts directly to 'input'.
    if (partsArray.size() == 1 && partsArray[0].toObject().contains("text")) {
        // If it's just text, we can pass it as a simple string to be ultra-clean
        rootObj["input"] = partsArray[0].toObject()["text"].toString();
    } else {
        // If it's multi-modal, we pass the flattened array of parts
        rootObj["input"] = partsArray;
    }

    // Inject the agent's tools into the payload
    if (!tools.isEmpty()) {
        rootObj["tools"] = tools;
    }

    QJsonDocument doc(rootObj);
    return doc.toJson(QJsonDocument::Compact);
}

QString GeminiPayloadBuilder::getMimeType(const QString& filePath) {
    QMimeDatabase mimeDb;
    QMimeType mime = mimeDb.mimeTypeForFile(filePath);
    return mime.name();
}