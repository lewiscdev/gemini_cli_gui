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
#include <QComboBox>

// Forward declarations to reduce header dependency bloat
class QLineEdit;
class QSpinBox;

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    /**
     * @brief Constructs the Settings Dialog and populates it with existing saved keys.
     * @param parent The parent widget, typically the MainWindow.
     */
    explicit SettingsDialog(QWidget* parent = nullptr);

    QString getTheme() const;

    // --- Credential Getters ---
    QString getApiKey() const;
    QString getGithubPat() const;
    
    // --- FTP Configuration Getters ---
    QString getFtpHost() const;
    int getFtpPort() const;
    QString getFtpUsername() const;
    QString getFtpPassword() const;

private slots:
    /**
     * @brief Persists the active input values to the local OS settings registry.
     */
    void saveSettings();

private:
    QComboBox* themeSelector;

    // --- API Input Fields ---
    QLineEdit* apiKeyInput;     ///< Input field for the Gemini API Key
    QLineEdit* githubPatInput;  ///< Input field for the GitHub PAT
    
    // --- FTP Input Fields ---
    QLineEdit* ftpHostInput;      ///< Input field for the FTP server hostname or IP
    QSpinBox* ftpPortInput;      ///< Numeric spinner for the FTP port (default 21)
    QLineEdit* ftpUsernameInput;  ///< Input field for the FTP username
    QLineEdit* ftpPasswordInput;  ///< Input field for the FTP password
};

#endif // SETTINGS_DIALOG_H