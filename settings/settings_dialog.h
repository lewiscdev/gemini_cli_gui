/**
 * @file settings_dialog.h
 * @brief Dialog window for managing global application settings.
 *
 * Handles the input, validation, and persistent storage of sensitive 
 * user credentials such as the Google Gemini API Key, GitHub PAT, 
 * and default FTP server configurations for autonomous deployment.
 */

#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <QDialog>
#include <QString>

// forward declarations to reduce header dependency bloat
class QLineEdit;
class QSpinBox;
class QComboBox;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    // ============================================================================
    // constructor
    // ============================================================================

    /**
     * @brief Constructs the settings dialog and populates it with existing saved keys.
     * @param parent The parent widget, typically the main window.
     */
    explicit SettingsDialog(QWidget* parent = nullptr);

    // ============================================================================
    // public accessors
    // ============================================================================

    /**
     * @brief Retrieves the currently selected application theme.
     * @return A string representing the theme (e.g., "dark" or "light").
     */
    QString getTheme() const;

    /**
     * @brief Retrieves the configured Gemini API key.
     * @return The API key string.
     */
    QString getApiKey() const;
    
    /**
     * @brief Retrieves the configured GitHub personal access token.
     * @return The GitHub PAT string.
     */
    QString getGithubPat() const;
    
    /**
     * @brief Retrieves the configured FTP host address.
     * @return The FTP host string.
     */
    QString getFtpHost() const;
    
    /**
     * @brief Retrieves the configured FTP port number.
     * @return The integer port number.
     */
    int getFtpPort() const;
    
    /**
     * @brief Retrieves the configured FTP username.
     * @return The FTP username string.
     */
    QString getFtpUsername() const;
    
    /**
     * @brief Retrieves the configured FTP password.
     * @return The FTP password string.
     */
    QString getFtpPassword() const;

private slots:
    // ============================================================================
    // ui interaction slots
    // ============================================================================

    /**
     * @brief Persists the active input values to the local OS settings registry.
     */
    void saveSettings();

private:
    // ============================================================================
    // ui elements
    // ============================================================================

    QComboBox* themeSelector;     ///< dropdown for selecting application theme

    QLineEdit* apiKeyInput;       ///< input field for the gemini api key
    QLineEdit* githubPatInput;    ///< input field for the github pat
    
    QLineEdit* ftpHostInput;      ///< input field for the ftp server hostname or ip
    QSpinBox* ftpPortInput;       ///< numeric spinner for the ftp port
    QLineEdit* ftpUsernameInput;  ///< input field for the ftp username
    QLineEdit* ftpPasswordInput;  ///< input field for the ftp password
};

#endif // SETTINGS_DIALOG_H