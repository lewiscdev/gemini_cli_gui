/**
 * @file settings_dialog.cpp
 * @brief Implementation of the global settings dialog.
 *
 * This file handles the UI layout, initialization from stored OS settings,
 * and the secure persistence of API keys, authentication tokens, and 
 * default FTP deployment configurations.
 */

#include "settings_dialog.h"
#include "theme_manager.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QVBoxLayout>

/**
 * @brief Constructs the Settings Dialog UI and loads saved credentials.
 */
SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Global Settings");
    setMinimumWidth(450);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // ========================================================================
    // SECTION 1: API CREDENTIALS
    // ========================================================================
    QGroupBox* apiGroup = new QGroupBox("API Credentials", this);
    QFormLayout* apiLayout = new QFormLayout(apiGroup);

    apiKeyInput = new QLineEdit(this);
    apiKeyInput->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    apiKeyInput->setPlaceholderText("AIzaSy...");
    
    githubPatInput = new QLineEdit(this);
    githubPatInput->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    githubPatInput->setPlaceholderText("ghp_xxxxxxxxxxxxxxxxxxxx");

    apiLayout->addRow("Gemini API Key:", apiKeyInput);
    apiLayout->addRow("GitHub PAT (Optional):", githubPatInput);
    
    mainLayout->addWidget(apiGroup);

    // ========================================================================
    // SECTION 2: FTP DEPLOYMENT CONFIGURATION
    // ========================================================================
    QGroupBox* ftpGroup = new QGroupBox("Default FTP Deployment (Optional)", this);
    QFormLayout* ftpLayout = new QFormLayout(ftpGroup);

    ftpHostInput = new QLineEdit(this);
    ftpHostInput->setPlaceholderText("ftp.yourdomain.com");

    // Use a SpinBox to ensure the user can only enter valid network ports
    ftpPortInput = new QSpinBox(this);
    ftpPortInput->setRange(1, 65535);
    ftpPortInput->setValue(21); // Default standard FTP port

    ftpUsernameInput = new QLineEdit(this);
    ftpUsernameInput->setPlaceholderText("username");

    ftpPasswordInput = new QLineEdit(this);
    ftpPasswordInput->setEchoMode(QLineEdit::PasswordEchoOnEdit);
    ftpPasswordInput->setPlaceholderText("password");

    ftpLayout->addRow("Host / URL:", ftpHostInput);
    ftpLayout->addRow("Port:", ftpPortInput);
    ftpLayout->addRow("Username:", ftpUsernameInput);
    ftpLayout->addRow("Password:", ftpPasswordInput);

    mainLayout->addWidget(ftpGroup);

    // ========================================================================
    // UI APPEARANCE
    // ========================================================================
    QGroupBox* uiGroup = new QGroupBox("Appearance", this);
    QFormLayout* uiLayout = new QFormLayout(uiGroup);
    
    themeSelector = new QComboBox(this);
    themeSelector->addItem("Dark Mode", "dark");
    themeSelector->addItem("Light Mode", "light");
    uiLayout->addRow("Application Theme:", themeSelector);
    
    mainLayout->addWidget(uiGroup);
    
    // ========================================================================
    // DIALOG CONTROLS & INITIALIZATION
    // ========================================================================
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* btnSave = new QPushButton("Save", this);
    QPushButton* btnCancel = new QPushButton("Cancel", this);

    buttonLayout->addStretch();
    buttonLayout->addWidget(btnSave);
    buttonLayout->addWidget(btnCancel);
    
    mainLayout->addLayout(buttonLayout);

    // --- Initialize from OS Registry/Settings ---
    QSettings settings;
    apiKeyInput->setText(settings.value("api_key", "").toString());
    githubPatInput->setText(settings.value("github_pat", "").toString());
    
    ftpHostInput->setText(settings.value("ftp_host", "").toString());
    ftpPortInput->setValue(settings.value("ftp_port", 21).toInt());
    ftpUsernameInput->setText(settings.value("ftp_username", "").toString());
    ftpPasswordInput->setText(settings.value("ftp_password", "").toString());

    // Load theme
    QString savedTheme = settings.value("theme", "dark").toString();
    int index = themeSelector->findData(savedTheme);
    if (index != -1) themeSelector->setCurrentIndex(index);

    // --- Connections ---
    connect(btnSave, &QPushButton::clicked, this, &SettingsDialog::saveSettings);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    ThemeManager::apply(this);
}

/**
 * @brief Saves the inputs to QSettings and closes the dialog.
 */
void SettingsDialog::saveSettings() {
    QString apiKey = apiKeyInput->text().trimmed();
    
    // Validate that the core API key is actually provided to prevent app failure
    if (apiKey.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "The Gemini API Key cannot be empty.");
        return;
    }

    QSettings settings;
    settings.setValue("api_key", apiKey);
    settings.setValue("github_pat", githubPatInput->text().trimmed());
    
    settings.setValue("theme", themeSelector->currentData().toString());

    // Save FTP Configs
    settings.setValue("ftp_host", ftpHostInput->text().trimmed());
    settings.setValue("ftp_port", ftpPortInput->value());
    settings.setValue("ftp_username", ftpUsernameInput->text().trimmed());
    settings.setValue("ftp_password", ftpPasswordInput->text()); // Passwords can contain leading/trailing spaces, don't trim
    
    accept();
}

// ============================================================================
// GETTERS
// ============================================================================

QString SettingsDialog::getApiKey() const {
    return apiKeyInput->text().trimmed();
}

QString SettingsDialog::getGithubPat() const {
    return githubPatInput->text().trimmed();
}

QString SettingsDialog::getFtpHost() const {
    return ftpHostInput->text().trimmed();
}

int SettingsDialog::getFtpPort() const {
    return ftpPortInput->value();
}

QString SettingsDialog::getFtpUsername() const {
    return ftpUsernameInput->text().trimmed();
}

QString SettingsDialog::getFtpPassword() const {
    return ftpPasswordInput->text();
}

QString SettingsDialog::getTheme() const {
    return themeSelector->currentData().toString();
}