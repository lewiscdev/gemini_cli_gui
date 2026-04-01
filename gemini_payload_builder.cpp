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
    QJsonArray contentsArray;
    QJsonObject contentObj;
    QJsonArray partsArray;

    contentObj["role"] = "user";

    // append the primary text prompt
    if (!text.isEmpty()) {
        QJsonObject textPart;
        textPart["text"] = text;
        partsArray.append(textPart);
    }

    // process and append any multi-modal file attachments
    for (const QString& filePath : attachments) {
        QFile file(filePath);
        if (file.open(QIODevice::ReadOnly)) {
            QString mimeType = getMimeType(filePath);
            
            // if it's a text file (code, logs), append as raw text to save tokens
            if (mimeType.startsWith("text/") || mimeType == "application/json") {
                QString fileContent = QString::fromUtf8(file.readAll());
                QJsonObject textPart;
                textPart["text"] = QString("--- FILE: %1 ---\n%2").arg(QFileInfo(filePath).fileName(), fileContent);
                partsArray.append(textPart);
            } 
            // if it's an image or binary, base64 encode it for the vision model
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

    contentObj["parts"] = partsArray;
    contentsArray.append(contentObj);
    rootObj["contents"] = contentsArray;

    // inject the agent's tools into the payload
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