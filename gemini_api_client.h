/**
 * @file gemini_api_client.h
 * @brief Google Gemini API network client.
 *
 * This class handles asynchronous HTTPS communication with the Gemini API.
 * It leverages external helper classes to construct payloads and focuses 
 * entirely on request transmission, state tracking, and response parsing.
 */

#ifndef GEMINI_API_CLIENT_H
#define GEMINI_API_CLIENT_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonObject>

class GeminiApiClient : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs the API client and initializes the network manager.
     * @param parent The parent QObject.
     */
    explicit GeminiApiClient(QObject* parent = nullptr);
    
    /**
     * @brief Ensures safe cleanup of network resources.
     */
    ~GeminiApiClient() override;

    /**
     * @brief Sets the global API key for authentication.
     * @param key The Gemini API key string.
     */
    void setApiKey(const QString& key);

    /**
     * @brief Restores the server-side contextual memory state.
     * @param interactionId The stateful tracking ID from a previous database session.
     */
    void restoreSession(const QString& interactionId);

    /**
     * @brief Severs the current contextual thread, starting a fresh conversation.
     */
    void resetSession();

    /**
     * @brief Transmits a multi-modal prompt to the Gemini API.
     * @param text The user's input or system instruction.
     * @param attachments A list of local file paths to encode and include.
     */
    void sendPrompt(const QString& text, const QStringList& attachments = QStringList());

signals:
    /**
     * @brief Emitted when the agent successfully responds with standard text.
     */
    void responseReceived(const QString& responseText, const QString& interactionId);

    /**
     * @brief Emitted when an HTTP, network, or authentication error occurs.
     */
    void networkError(const QString& errorDetails);

    /**
     * @brief Emitted to update the UI with real-time token consumption.
     */
    void usageMetricsReceived(int inputTokens, int outputTokens, int totalTokens);

    /**
     * @brief Emitted when the LLM natively requests to execute a local tool.
     */
    void functionCallRequested(const QString& functionName, const QJsonObject& arguments);

private slots:
    /**
     * @brief Parses the asynchronous HTTP response from Google's servers.
     * @param reply The completed network reply object.
     */
    void onNetworkReply(QNetworkReply* reply);

private:
    QNetworkAccessManager* networkManager{nullptr}; ///< handles all asynchronous http traffic
    
    QString apiKey;                  ///< the authenticated gemini api key
    QString currentApiInteractionId; ///< tracks the stateful thread on google's servers
};

#endif // GEMINI_API_CLIENT_H