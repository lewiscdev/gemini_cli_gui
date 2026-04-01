/**
 * @file session_dialog.cpp
 * @brief Implementation of the local project and session manager.
 *
 * This file handles the UI for creating, loading, and deleting isolated 
 * workspaces. It delegates all sqlite data persistence and retrieval 
 * to the injected database manager.
 */

#include "session_dialog.h"
#include "theme_manager.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QUuid>
#include <QVBoxLayout>

SessionDialog::SessionDialog(DatabaseManager* db, QWidget* parent) 
    : QDialog(parent), dbManager(db) {
    
    setWindowTitle("Project & Session Manager");
    setMinimumSize(400, 300);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // --- session list view ---
    sessionList = new QListWidget(this);
    mainLayout->addWidget(sessionList);

    // --- control buttons ---
    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnNew = new QPushButton("New Session", this);
    btnDelete = new QPushButton("Delete", this);
    btnLoad = new QPushButton("Load Selected", this);

    // allow the user to hit 'enter' to quickly load the highlighted session
    btnLoad->setDefault(true); 

    btnLayout->addWidget(btnNew);
    btnLayout->addWidget(btnDelete);
    btnLayout->addStretch();
    btnLayout->addWidget(btnLoad);

    mainLayout->addLayout(btnLayout);

    // --- connections ---
    connect(btnNew, &QPushButton::clicked, this, &SessionDialog::createNewSession);
    connect(btnDelete, &QPushButton::clicked, this, &SessionDialog::deleteSelectedSession);
    connect(btnLoad, &QPushButton::clicked, this, &SessionDialog::selectAndClose);
    connect(sessionList, &QListWidget::itemDoubleClicked, this, &SessionDialog::selectAndClose);

    loadSessionsFromDb();

    ThemeManager::apply(this);
}

void SessionDialog::loadSessionsFromDb() {
    sessionList->clear();
    
    // fetch all sessions through the injected database manager
    QList<SessionData> sessions = dbManager->getAllSessions();
    
    for (const SessionData& data : sessions) {
        QListWidgetItem* item = new QListWidgetItem(QString("%1\n(%2)").arg(data.name, data.workspace));
        
        // store the hidden data payload inside the ui item
        item->setData(Qt::UserRole, data.id);
        item->setData(Qt::UserRole + 1, data.name);
        item->setData(Qt::UserRole + 2, data.workspace);
        
        sessionList->addItem(item);
    }

    // auto-select the most recently used session for lightning-fast startups
    if (sessionList->count() > 0) {
        sessionList->setCurrentRow(0); 
    }
}

void SessionDialog::createNewSession() {
    // ask for a project name
    bool ok;
    QString name = QInputDialog::getText(this, "New Session", "Enter project/session name:", QLineEdit::Normal, "", &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    // ask for the workspace directory to sandbox the agent
    QString workspace = QFileDialog::getExistingDirectory(this, "Select Workspace Directory for this Session");
    if (workspace.isEmpty()) return;

    // generate a unique id and save via the database manager
    QString newId = QUuid::createUuid().toString(QUuid::WithoutBraces);
    
    if (dbManager->createSession(newId, name, workspace)) {
        loadSessionsFromDb(); 
    } else {
        QMessageBox::critical(this, "Database Error", "Failed to create session in the database.");
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
        // delegate the cascading delete to the database manager
        if (dbManager->deleteSession(sessionId)) {
            loadSessionsFromDb();
        } else {
            QMessageBox::critical(this, "Database Error", "Failed to delete session from the database.");
        }
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
    
    // update the timestamp so this project bubbles to the top of the list next time
    dbManager->updateSessionTimestamp(selectedSession.id);
    
    accept();
}

SessionData SessionDialog::getSelectedSession() const {
    return selectedSession;
}