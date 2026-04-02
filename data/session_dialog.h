/**
 * @file session_dialog.h
 * @brief Dialog window for managing local projects and chat sessions.
 *
 * This file defines the SessionDialog class, which handles the UI for 
 * creating, deleting, and selecting isolated agent workspaces. It now 
 * delegates all SQLite operations to the injected DatabaseManager.
 */

#ifndef SESSION_DIALOG_H
#define SESSION_DIALOG_H

#include <QDialog>
#include <QString>

#include "database_manager.h"

// forward declarations to optimize compile times
class QListWidget;
class QPushButton;

class SessionDialog : public QDialog {
    Q_OBJECT

public:
    // ============================================================================
    // constructor and initialization
    // ============================================================================

    /**
     * @brief Constructs the session dialog using an injected database manager.
     * @param db Pointer to the active database manager instance.
     * @param parent The parent widget, typically the main window.
     */
    explicit SessionDialog(DatabaseManager* db, QWidget* parent = nullptr);

    // ============================================================================
    // public interface
    // ============================================================================

    /**
     * @brief Retrieves the data for the session the user just selected.
     * @return A session data struct containing the id, name, and workspace path.
     */
    [[nodiscard]] SessionData getSelectedSession() const;

private slots:
    // ============================================================================
    // ui interaction slots
    // ============================================================================
    
    /**
     * @brief Queries the database manager and populates the list widget.
     */
    void loadSessionsFromDb();
    
    /**
     * @brief Prompts the user for a name and directory, then saves a new session.
     */
    void createNewSession();
    
    /**
     * @brief Permanently deletes the selected session and its associated chat history.
     */
    void deleteSelectedSession();
    
    /**
     * @brief Validates the selection, updates the 'last used' timestamp, and closes the dialog.
     */
    void selectAndClose();

private:
    // ============================================================================
    // internal state and ui elements
    // ============================================================================
    
    DatabaseManager* dbManager;        ///< pointer to the centralized database manager
    
    QListWidget* sessionList{nullptr}; ///< displays the list of available saved sessions
    QPushButton* btnNew{nullptr};      ///< button to trigger the creation of a new session
    QPushButton* btnDelete{nullptr};   ///< button to delete the currently highlighted session
    QPushButton* btnLoad{nullptr};     ///< button to load the currently highlighted session
    
    SessionData selectedSession;       ///< holds the data of the currently active session
};

#endif // SESSION_DIALOG_H