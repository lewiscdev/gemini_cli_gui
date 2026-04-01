/**
 * @file main_window.h
 * @brief Primary GUI and agent orchestration logic.
 *
 * This file defines the MainWindow class, which manages the central UI, 
 * user inputs, multi-modal attachments, and routes commands between the 
 * Gemini API client and the local agent execution manager.
 */

#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QString>
#include <QStringList>
#include <QPixmap>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QJsonObject>

#include "agent_manager.h"
#include "database_manager.h"

// forward declarations to reduce header bloat
class GeminiApiClient;
class QTextEdit;
class QLineEdit;
class QPushButton;
class QLabel;
class QVBoxLayout;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    /**
     * @brief Constructs the main window and initializes all agent subsystems.
     * @param parent The parent widget.
     */
    explicit MainWindow(QWidget* parent = nullptr);
    
    /**
     * @brief Safely cleans up the ui.
     */
    ~MainWindow() override;

protected:
    // --- drag and drop event overrides for multi-modal attachments ---
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private slots:
    // --- ui interaction slots ---
    void handleSendClicked();
    void attachFiles();
    void clearAttachments();
    void openSettings();
    void switchSession();

    // --- api routing slots ---
    void onResponseReceived(const QString& responseText, const QString& interactionId);
    void onNetworkError(const QString& errorDetails);
    void onUsageMetricsReceived(int inputTokens, int outputTokens, int totalTokens);
    void handleNativeFunctionCall(const QString& functionName, const QJsonObject& arguments);
    
    /**
     * @brief Catches the execution results from isolated tool classes.
     * @param feedback The standard output, standard error, or system confirmation.
     */
    void onAgentSystemFeedback(const QString& feedback);

private:
    // --- ui elements ---
    QWidget* centralWidget{nullptr};
    QVBoxLayout* mainLayout{nullptr};
    
    QTextEdit* chatDisplay{nullptr};
    QLabel* tokenDisplayLabel{nullptr};
    
    QLabel* lblAttachments{nullptr};
    QPushButton* btnClearFiles{nullptr};
    QPushButton* btnAttach{nullptr};
    QLineEdit* inputField{nullptr};
    QPushButton* sendButton{nullptr};
    
    QPushButton* btnManageSessions{nullptr};
    QPushButton* btnSettings{nullptr};

    // --- core subsystems ---
    GeminiApiClient* apiClient{nullptr};
    AgentActionManager* agentController{nullptr};
    DatabaseManager* dbManager{nullptr};   ///< central manager for all sqlite operations

    // --- session state ---
    QString currentSessionId;
    QString currentWorkspacePath;
    QStringList pendingAttachments;

    // --- internal helper methods ---
    
    /**
     * @brief Refreshes the ui elements associated with multi-modal file attachments.
     */
    void updateAttachmentUi();

    /**
     * @brief Constructs the visual layout of the application.
     */
    void setupUi();

    /**
     * @brief Wires ui actions and api signals to their respective logic handlers.
     */
    void initializeConnections();

    /**
     * @brief Saves a single chat interaction via the database manager.
     */
    void saveInteractionToDb(const QString& role, const QString& content, const QString& apiInteractionId = "");

    /**
     * @brief Restores ui state and api memory thread for the active session.
     * @return True if existing history was found and loaded.
     */
    bool loadHistoryFromDb();
};

#endif // MAIN_WINDOW_H