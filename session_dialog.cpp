#include "session_dialog.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QFileDialog>
#include <QMessageBox>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>
#include <QDateTime>

SessionDialog::SessionDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Project & Session Manager");
    setMinimumSize(400, 300);

    ensureTableExists();

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    sessionList = new QListWidget(this);
    mainLayout->addWidget(sessionList);

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnNew = new QPushButton("New Session", this);
    btnDelete = new QPushButton("Delete", this);
    btnLoad = new QPushButton("Load Selected", this);
    btnLoad->setDefault(true); // Makes the 'Enter' key trigger this button

    btnLayout->addWidget(btnNew);
    btnLayout->addWidget(btnDelete);
    btnLayout->addStretch();
    btnLayout->addWidget(btnLoad);

    mainLayout->addLayout(btnLayout);

    connect(btnNew, &QPushButton::clicked, this, &SessionDialog::createNewSession);
    connect(btnDelete, &QPushButton::clicked, this, &SessionDialog::deleteSelectedSession);
    connect(btnLoad, &QPushButton::clicked, this, &SessionDialog::selectAndClose);
    connect(sessionList, &QListWidget::itemDoubleClicked, this, &SessionDialog::selectAndClose);

    loadSessionsFromDb();
}

void SessionDialog::ensureTableExists() {
    QSqlQuery query;
    // Create the master sessions table if it doesn't exist
    query.exec("CREATE TABLE IF NOT EXISTS sessions ("
               "id TEXT PRIMARY KEY, "
               "name TEXT, "
               "workspace TEXT, "
               "created_at INTEGER)");
}

void SessionDialog::loadSessionsFromDb() {
    sessionList->clear();
    QSqlQuery query("SELECT id, name, workspace FROM sessions ORDER BY created_at DESC");
    
    while (query.next()) {
        QString id = query.value(0).toString();
        QString name = query.value(1).toString();
        QString workspace = query.value(2).toString();
        
        QListWidgetItem* item = new QListWidgetItem(QString("%1\n(%2)").arg(name, workspace));
        item->setData(Qt::UserRole, id);
        item->setData(Qt::UserRole + 1, name);
        item->setData(Qt::UserRole + 2, workspace);
        sessionList->addItem(item);
    }

    if (sessionList->count() > 0) {
        sessionList->setCurrentRow(0); // Auto-select the top item
    }
}

void SessionDialog::createNewSession() {
    // 1. Ask for a project name
    bool ok;
    QString name = QInputDialog::getText(this, "New Session", "Enter project/session name:", QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    // 2. Ask for the workspace directory
    QString workspace = QFileDialog::getExistingDirectory(this, "Select Workspace Directory for this Session");
    if (workspace.isEmpty()) return;

    // 3. Save to database
    QString newId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    QSqlQuery query;
    query.prepare("INSERT INTO sessions (id, name, workspace, created_at) VALUES (:id, :name, :ws, :time)");
    query.bindValue(":id", newId);
    query.bindValue(":name", name);
    query.bindValue(":ws", workspace);
    query.bindValue(":time", QDateTime::currentSecsSinceEpoch());
    
    if (query.exec()) {
        loadSessionsFromDb(); // Refresh the UI
    } else {
        QMessageBox::critical(this, "Database Error", "Failed to create session: " + query.lastError().text());
    }
}

void SessionDialog::deleteSelectedSession() {
    QListWidgetItem* item = sessionList->currentItem();
    if (!item) return;

    QString sessionId = item->data(Qt::UserRole).toString();

    int ret = QMessageBox::warning(this, "Delete Session", 
                                   "Are you sure you want to delete this session and its chat history?\n(Local files will NOT be deleted).",
                                   QMessageBox::Yes | QMessageBox::No);
                                   
    if (ret == QMessageBox::Yes) {
        QSqlQuery query;
        // Delete the session record
        query.prepare("DELETE FROM sessions WHERE id = :id");
        query.bindValue(":id", sessionId);
        query.exec();
        
        // Delete the associated chat history
        query.prepare("DELETE FROM interactions WHERE session_id = :id");
        query.bindValue(":id", sessionId);
        query.exec();

        loadSessionsFromDb();
    }
}

void SessionDialog::selectAndClose() {
    QListWidgetItem* item = sessionList->currentItem();
    if (!item) {
        QMessageBox::information(this, "Notice", "Please select a session or create a new one.");
        return;
    }

    selectedSession.id = item->data(Qt::UserRole).toString();
    selectedSession.name = item->data(Qt::UserRole + 1).toString();
    selectedSession.workspace = item->data(Qt::UserRole + 2).toString();
    
    // Update the timestamp so this session is at the top next time
    QSqlQuery query;
    query.prepare("UPDATE sessions SET created_at = :time WHERE id = :id");
    query.bindValue(":time", QDateTime::currentSecsSinceEpoch());
    query.bindValue(":id", selectedSession.id);
    query.exec();

    accept();
}

SessionData SessionDialog::getSelectedSession() const {
    return selectedSession;
}