/**
 * @file database_manager.cpp
 * @brief Implementation of the database manager.
 *
 * Handles the initialization, querying, and mutation of the local sqlite 
 * database used for storing agent sessions and multi-turn chat histories.
 */

#include "database_manager.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDateTime>
#include <QVariant>

// ============================================================================
// constructor and initialization
// ============================================================================

DatabaseManager::DatabaseManager(QObject* parent) : QObject(parent) {
    // initialization is deferred to initializedatabase to handle errors cleanly
}

bool DatabaseManager::initializeDatabase() {
    // establish the sqlite connection if it does not already exist
    QSqlDatabase db;
    if (QSqlDatabase::contains("qt_sql_default_connection")) {
        db = QSqlDatabase::database("qt_sql_default_connection");
    } else {
        db = QSqlDatabase::addDatabase("QSQLITE");
        db.setDatabaseName("agent_history.db");
    }

    if (!db.open()) {
        return false;
    }

    return ensureTablesExist();
}

// ============================================================================
// session management
// ============================================================================

QList<SessionData> DatabaseManager::getAllSessions() const {
    QList<SessionData> sessions;
    QSqlQuery query("SELECT id, name, workspace FROM sessions ORDER BY created_at DESC");
    
    // iterate through the result set and populate the data structures
    while (query.next()) {
        SessionData data;
        data.id = query.value(0).toString();
        data.name = query.value(1).toString();
        data.workspace = query.value(2).toString();
        sessions.append(data);
    }
    
    return sessions;
}

bool DatabaseManager::createSession(const QString& id, const QString& name, const QString& workspace) {
    QSqlQuery query;
    query.prepare("INSERT INTO sessions (id, name, workspace, created_at) VALUES (:id, :name, :ws, :time)");
    query.bindValue(":id", id);
    query.bindValue(":name", name);
    query.bindValue(":ws", workspace);
    query.bindValue(":time", QDateTime::currentSecsSinceEpoch());
    
    return query.exec();
}

bool DatabaseManager::deleteSession(const QString& id) {
    QSqlQuery query;
    
    // wipe the master session record
    query.prepare("DELETE FROM sessions WHERE id = :id");
    query.bindValue(":id", id);
    bool sessionDeleted = query.exec();
    
    // wipe the associated chat history to prevent orphaned records
    query.prepare("DELETE FROM interactions WHERE session_id = :id");
    query.bindValue(":id", id);
    bool historyDeleted = query.exec();
    
    return sessionDeleted && historyDeleted;
}

void DatabaseManager::updateSessionTimestamp(const QString& id) {
    QSqlQuery query;
    query.prepare("UPDATE sessions SET created_at = :time WHERE id = :id");
    query.bindValue(":time", QDateTime::currentSecsSinceEpoch());
    query.bindValue(":id", id);
    query.exec();
}

// ============================================================================
// interaction history
// ============================================================================

bool DatabaseManager::saveInteraction(const QString& sessionId, const QString& role, const QString& content, const QString& apiInteractionId) {
    QSqlQuery query;
    query.prepare("INSERT INTO interactions (session_id, role, content, api_interaction_id) VALUES (:sid, :role, :content, :api_id)");
    query.bindValue(":sid", sessionId);
    query.bindValue(":role", role);
    query.bindValue(":content", content);
    query.bindValue(":api_id", apiInteractionId);
    
    return query.exec();
}

QList<InteractionData> DatabaseManager::getInteractions(const QString& sessionId) const {
    QList<InteractionData> interactions;
    QSqlQuery query;
    
    // pull chronological history for the specific workspace
    query.prepare("SELECT role, content, api_interaction_id FROM interactions WHERE session_id = :session_id ORDER BY timestamp ASC");
    query.bindValue(":session_id", sessionId);
    query.exec();
    
    while (query.next()) {
        InteractionData data;
        data.role = query.value(0).toString();
        data.content = query.value(1).toString();
        data.apiId = query.value(2).toString();
        interactions.append(data);
    }
    
    return interactions;
}

// ============================================================================
// private helper methods
// ============================================================================

bool DatabaseManager::ensureTablesExist() {
    QSqlQuery query;
    
    // build the sessions table
    bool sessionsOk = query.exec("CREATE TABLE IF NOT EXISTS sessions ("
                                 "id TEXT PRIMARY KEY, "
                                 "name TEXT, "
                                 "workspace TEXT, "
                                 "created_at INTEGER)");

    // build the interactions table for conversational memory
    bool interactionsOk = query.exec("CREATE TABLE IF NOT EXISTS interactions ("
                                     "id INTEGER PRIMARY KEY AUTOINCREMENT, "
                                     "session_id TEXT, "
                                     "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP, "
                                     "role TEXT, "
                                     "content TEXT, "
                                     "api_interaction_id TEXT)");

    return sessionsOk && interactionsOk;
}