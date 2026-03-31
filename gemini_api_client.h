#ifndef GEMINI_API_CLIENT_H
#define GEMINI_API_CLIENT_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

class GeminiApiClient : public QObject {
    Q_OBJECT

public:
    explicit GeminiApiClient(QObject* parent = nullptr);
    ~GeminiApiClient();

    // configuration
    void setApiKey(const QString& key);
    
    // primary execution method
    void sendPrompt(const QString& userInput);

    // resets the interaction chain for a new chat
    void resetSession();

signals:
    // emitted when a successful json payload is parsed, passing the new interaction id
    void responseReceived(const QString& responseText, const QString& interactionId);
    
    // emitted if the https request fails
    void networkError(const QString& errorDetails);

    // emitted when the response contains a function call request
    void functionCallRequested(const QString& functionName, const QJsonObject& arguments);

private slots:
    // internal handler for when the google server responds
    void onNetworkReply(QNetworkReply* reply);

private:
    QNetworkAccessManager* networkManager;
    QString apiKey;
    QString currentApiInteractionId; // tracks the stateful api thread
    
    // helper to format the outgoing http request
    QNetworkRequest createRequest() const;
    
    // helper to assemble the json body
    QByteArray buildJsonPayload(const QString& newPrompt);

    // helper to define the available tools in the system prompt
    QJsonArray defineAvailableTools() const;
};

#endif // GEMINI_API_CLIENT_H