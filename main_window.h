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
#include <QTextBrowser>
#include <QUrl>

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
    // --- hardware input intercepts ---
    
    /**
     * @brief Intercepts key presses to handle custom shift+enter logic.
     * @param obj The object that generated the event.
     * @param event The triggered event.
     * @return True if the event was handled, false otherwise.
     */
    bool eventFilter(QObject *obj, QEvent *event) override;
    
    /**
     * @brief Validates dragged objects as they enter the window.
     * @param event The drag enter event.
     */
    void dragEnterEvent(QDragEnterEvent *event) override;
    
    /**
     * @brief Processes the files dropped onto the window.
     * @param event The drop event.
     */
    void dropEvent(QDropEvent *event) override;

private slots:
    // --- ui interaction slots ---
    
    /**
     * @brief Gathers user input and triggers the api payload submission.
     */
    void handleSendClicked();
    
    /**
     * @brief Opens a file dialog to manually attach multi-modal files.
     */
    void attachFiles();
    
    /**
     * @brief Clears all currently pending file attachments.
     */
    void clearAttachments();
    
    /**
     * @brief Launches the global settings dialog.
     */
    void openSettings();
    
    /**
     * @brief Launches the session manager to swap active projects.
     */
    void switchSession();

    // --- api routing slots ---
    
    /**
     * @brief Renders the standard text response from the llm.
     * @param responseText The markdown-formatted response string.
     * @param interactionId The stateful api tracking id.
     */
    void onResponseReceived(const QString& responseText, const QString& interactionId);
    
    /**
     * @brief Displays network or parsing errors to the user.
     * @param errorDetails The specific error string.
     */
    void onNetworkError(const QString& errorDetails);
    
    /**
     * @brief Updates the ui token consumption counters.
     * @param inputTokens The number of prompt tokens.
     * @param outputTokens The number of generated tokens.
     * @param totalTokens The sum of both.
     */
    void onUsageMetricsReceived(int inputTokens, int outputTokens, int totalTokens);
    
    /**
     * @brief Catches batch tool execution requests from the api client.
     * @param toolCalls The json array of requested native tools.
     */
    void handleNativeFunctionCalls(const QJsonArray& toolCalls);
    
    /**
     * @brief Catches the execution results from isolated tool classes.
     * @param feedback The standard output, standard error, or system confirmation.
     */
    void onAgentSystemFeedback(const QString& feedback);

    /**
     * @brief Intercepts clicks on html anchor tags inside the chat display.
     * @param url The clicked url.
     */
    void handleAnchorClicked(const QUrl& url);

private:
    // --- ui elements ---
    QWidget* centralWidget{nullptr};       ///< the central widget of the main window
    QVBoxLayout* mainLayout{nullptr};      ///< the primary vertical layout
    
    QTextBrowser* chatDisplay{nullptr};    ///< renders the html chat history
    QTextEdit* inputField{nullptr};        ///< handles multi-line user input
    QLabel* tokenDisplayLabel{nullptr};    ///< displays token consumption metrics
    
    QLabel* lblAttachments{nullptr};       ///< displays the number of attached files
    QPushButton* btnClearFiles{nullptr};   ///< button to clear pending attachments
    QPushButton* btnAttach{nullptr};       ///< button to open the file attachment dialog
    QPushButton* sendButton{nullptr};      ///< button to submit the prompt
    
    QPushButton* btnManageSessions{nullptr}; ///< button to launch the session manager
    QPushButton* btnSettings{nullptr};       ///< button to launch the settings dialog

    // --- core subsystems ---
    GeminiApiClient* apiClient{nullptr};     ///< handles network communication with google
    AgentActionManager* agentController{nullptr}; ///< routes and executes native tools
    DatabaseManager* dbManager{nullptr};     ///< central manager for all sqlite operations

    // --- session state ---
    QString currentSessionId;              ///< the uuid of the active sqlite session
    QString currentWorkspacePath;          ///< the absolute path to the active sandbox
    QStringList pendingAttachments;        ///< list of files waiting to be sent

    // --- batch processing state ---
    bool isBatchProcessing{false};         ///< flag indicating if a batch of tools is running
    QString batchSystemFeedback;           ///< accumulated feedback buffer for batch operations

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
     * @param role The entity that spoke (e.g., user, model, system).
     * @param content The text payload of the message.
     * @param apiInteractionId The stateful tracking id for the api context.
     */
    void saveInteractionToDb(const QString& role, const QString& content, const QString& apiInteractionId = "");

    /**
     * @brief Restores ui state and api memory thread for the active session.
     * @return True if existing history was found and loaded.
     */
    bool loadHistoryFromDb();

    /**
     * @brief Applies qss stylesheets and updates internal theme state.
     */
    void applyTheme();

    /**
     * @brief Wraps parsed html in a structural layout table to create chat bubbles.
     * @param role The entity that spoke (user, model, system).
     * @param content The raw or parsed html content.
     * @param isDark Whether the dark theme is active.
     * @return The formatted html string.
     */
    QString formatChatBubble(const QString& role, const QString& content, bool isDark);

    bool isDarkTheme{true}; ///< tracks global theme state
};

#endif // MAIN_WINDOW_H