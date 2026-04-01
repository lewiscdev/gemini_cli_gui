/**
 * @file database_manager.h
 * @brief Centralized database management for sessions and chat history.
 *
 * This file defines the DatabaseManager class and related data structures.
 * It encapsulates all SQLite operations, removing raw SQL from the UI layer
 * and ensuring a strict separation of concerns.
 */

#ifndef DATABASE_MANAGER_H
#define DATABASE_MANAGER_H

#include <QObject>
#include <QString>
#include <QList>

/**
 * @brief A lightweight data structure representing a saved agent session.
 */
struct SessionData {
    QString id;         ///< Unique UUID linking the session to the SQLite database
    QString name;       ///< Human-readable project/session name
    QString workspace;  ///< Absolute path to the isolated local sandbox directory
};

/**
 * @brief Data structure representing a single chat turn or system event.
 */
struct InteractionData {
    QString role;       ///< The entity that spoke (e.g., "user", "model", "system")
    QString content;    ///< The exact text payload
    QString apiId;      ///< The stateful tracking ID from Google's API
};

class DatabaseManager : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Constructs the database manager.
     * @param parent The parent QObject.
     */
    explicit DatabaseManager(QObject* parent = nullptr);

    /**
     * @brief Initializes the SQLite database connection and ensures schemas exist.
     * @return True if the database opened and tables were created successfully.
     */
    bool initializeDatabase();

    // ============================================================================
    // SESSION MANAGEMENT
    // ============================================================================

    /**
     * @brief Retrieves all saved sessions, sorted by most recently used.
     * @return A list of SessionData structures.
     */
    QList<SessionData> getAllSessions() const;

    /**
     * @brief Creates a new project session in the database.
     * @param id The unique UUID for the session.
     * @param name The human-readable project name.
     * @param workspace The absolute path to the local sandbox.
     * @return True if the insert was successful.
     */
    bool createSession(const QString& id, const QString& name, const QString& workspace);

    /**
     * @brief Permanently deletes a session and its associated chat history.
     * @param id The UUID of the session to delete.
     * @return True if the deletion was successful.
     */
    bool deleteSession(const QString& id);

    /**
     * @brief Updates the 'last used' timestamp for a session to bubble it to the top.
     * @param id The UUID of the session.
     */
    void updateSessionTimestamp(const QString& id);

    // ============================================================================
    // INTERACTION HISTORY
    // ============================================================================

    /**
     * @brief Saves a single chat interaction to the database.
     * @param sessionId The UUID of the active session.
     * @param role The entity that generated the message (user, model, system).
     * @param content The text payload of the message.
     * @param apiInteractionId The stateful tracking ID for the API context.
     * @return True if the interaction was saved successfully.
     */
    bool saveInteraction(const QString& sessionId, const QString& role, const QString& content, const QString& apiInteractionId = "");

    /**
     * @brief Retrieves the chronological chat history for a specific session.
     * @param sessionId The UUID of the session to load.
     * @return A list of interactions in ascending chronological order.
     */
    QList<InteractionData> getInteractions(const QString& sessionId) const;

private:
    /**
     * @brief Internal helper to ensure the necessary tables exist before querying.
     * @return True if tables exist or were created successfully.
     */
    bool ensureTablesExist(); 
};

#endif // DATABASE_MANAGER_H