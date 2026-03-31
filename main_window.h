#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QSqlDatabase>
#include "agent_manager.h" 
#include <QLabel>
#include <QString>
#include <QStringList>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>

// forward declarations
class QVBoxLayout;
class QTextEdit;
class QLineEdit;
class QPushButton;
class QWidget;
class GeminiApiClient; 
class QLabel;

class MainWindow : public QMainWindow {
    Q_OBJECT

protected:
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void handleSendClicked();
    void appendStandardOutput(const QString& text, const QString& interactionId);
    void appendStandardError(const QString& text);
    void handleAgentActionRequest(const AgentCommand& command);
    void handleNativeFunctionCall(const QString& functionName, const QJsonObject& arguments);
    void updateTokenDisplay(int inputTokens, int outputTokens, int totalTokens);
    void openSettings();
    void switchSession();
    void attachFiles();
    void clearAttachments();

private:
    QWidget* centralWidget;
    QVBoxLayout* mainLayout;
    QTextEdit* chatDisplay;
    QLineEdit* inputField;
    QPushButton* sendButton;
    QLabel* tokenDisplayLabel;
    QPushButton* btnSettings;
    QPushButton* btnManageSessions;
    QPushButton* btnAttach;        
    QPushButton* btnClearFiles;    
    QLabel* lblAttachments;        

    GeminiApiClient* apiClient; 
    AgentActionManager* agentController;

    QSqlDatabase db;
    QString currentSessionId;
    QString currentWorkspacePath;
    QStringList pendingAttachments;

    void setupUi();
    void initializeConnections();
    
    // local storage setup and execution
    void initDatabase();
    bool loadHistoryFromDb();
    void saveInteractionToDb(const QString& role, const QString& content, const QString& apiInteractionId = "");
    QString resolveAndVerifyPath(const QString& relativeTarget);

    void updateAttachmentUi();
    QString buildSystemPrompt();
};

#endif // MAIN_WINDOW_H