/**
 * @file main_window.h
 * @brief Primary GUI definition for the Gemini Native Agent.
 *
 * Handles chat rendering, user input, drag-and-drop file attachments,
 * tool execution routing, and session management.
 */

#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QString>
#include <QStringList>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QProcess>
#include <QPixmap>
#include <QSqlDatabase>
#include <QJsonObject>

#include "agent_manager.h"

// Forward declarations for UI elements to optimize compile times
class QTextEdit;
class QLineEdit;
class QPushButton;
class QVBoxLayout;
class QLabel;
class GeminiApiClient;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

protected:
    // Native Qt event overrides for Drag & Drop functionality
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

signals:
    void cleanTextReady(const QString& text);

private slots:
    // --- UI Interaction Slots ---
    void handleSendClicked();
    void attachFiles();
    void clearAttachments();
    void openSettings();
    void switchSession();

    // --- API & Backend Slots ---
    void onResponseReceived(const QString& responseText, const QString& interactionId);
    void onNetworkError(const QString& errorDetails);
    void onUsageMetricsReceived(int inputTokens, int outputTokens, int totalTokens);
    
    // --- Tool Execution Slots ---
    void handleNativeFunctionCall(const QString& functionName, const QJsonObject& arguments);
    void handleAgentActionRequest(const AgentCommand& command);

private:
    // --- Initialization Helpers ---
    void setupUi();
    void initializeConnections();
    void initDatabase();
    
    // --- State & Memory Helpers ---
    bool loadHistoryFromDb();
    void saveInteractionToDb(const QString& role, const QString& content, const QString& apiInteractionId = "");
    
    // --- Agent Capabilities Helpers ---
    QString resolveAndVerifyPath(const QString& relativeTarget);
    QString buildSystemPrompt();
    QPixmap captureProcessWindow(qint64 processId);
    void updateAttachmentUi();

    // --- UI Pointers ---
    QWidget* centralWidget;
    QVBoxLayout* mainLayout;
    QTextEdit* chatDisplay;
    QLineEdit* inputField;
    QPushButton* sendButton;
    QLabel* tokenDisplayLabel;
    
    // Toolbar pointers
    QPushButton* btnManageSessions;
    QPushButton* btnSettings;
    
    // Attachment pointers
    QPushButton* btnAttach;
    QPushButton* btnClearFiles;
    QLabel* lblAttachments;

    // --- Backend Managers & State ---
    GeminiApiClient* apiClient;
    AgentActionManager* agentController;
    QSqlDatabase db;
    QProcess* agentProcess;

    QString currentSessionId;
    QString currentWorkspacePath;
    QStringList pendingAttachments;
};

#endif // MAIN_WINDOW_H