/**
 * @file gemini_payload_builder.h
 * @brief Constructs the JSON payloads for the Gemini REST API.
 *
 * This helper class abstracts the complexity of building multi-modal 
 * requests, including base64 file encoding, MIME type detection, 
 * and injecting the defined tool schemas.
 */

#ifndef GEMINI_PAYLOAD_BUILDER_H
#define GEMINI_PAYLOAD_BUILDER_H

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QJsonArray>
#include <QList>

// forward declarations to reduce header bloat
struct InteractionData;

class GeminiPayloadBuilder {
public:
    // ============================================================================
    // public payload builders
    // ============================================================================

    /**
     * @brief Builds the complete JSON request body for the LLM.
     * @param history The chronological list of prior interactions.
     * @param text The user's text prompt or system instruction.
     * @param attachments A list of local file paths to attach (images/text).
     * @param tools The JSON array of available agent tools.
     * @return The formatted UTF-8 JSON byte array ready for network transmission.
     */
    [[nodiscard]] static QByteArray buildRequest(const QList<InteractionData>& history, const QString& text, const QStringList& attachments, const QJsonArray& tools);

private:
    // ============================================================================
    // private helper methods
    // ============================================================================

    /**
     * @brief Determines the correct MIME type based on file extension.
     * @param filePath The local path to the file.
     * @return The MIME type string (e.g., "image/png", "text/plain").
     */
    [[nodiscard]] static QString getMimeType(const QString& filePath);
};

#endif // GEMINI_PAYLOAD_BUILDER_H