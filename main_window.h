#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include <QMainWindow>
#include <QSqlDatabase>
#include "agent_manager.h" 

// forward declarations
class QVBoxLayout;
class QTextEdit;
class QLineEdit;
class QPushButton;
class QWidget;
class GeminiApiClient; 

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void handleSendClicked();
    void appendStandardOutput(const QString& text, const QString& interactionId);
    void appendStandardError(const QString& text);
    void handleAgentActionRequest(const AgentCommand& command);
    void handleNativeFunctionCall(const QString& functionName, const QJsonObject& arguments);

private:
    QWidget* centralWidget;
    QVBoxLayout* mainLayout;
    QTextEdit* chatDisplay;
    QLineEdit* inputField;
    QPushButton* sendButton;

    GeminiApiClient* apiClient; 
    AgentActionManager* agentController;

    QSqlDatabase db;
    QString currentSessionId;

    void setupUi();
    void initializeConnections();
    
    // local storage setup and execution
    void initDatabase();
    void saveInteractionToDb(const QString& role, const QString& content, const QString& apiInteractionId = "");
};

#endif // MAIN_WINDOW_H