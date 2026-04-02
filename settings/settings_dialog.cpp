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
#include <QComboBox>

// ============================================================================
// constructor and initialization
// ============================================================================

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setWindowTitle("Global Settings");
    setMinimumWidth(450);

    QVBoxLayout* mainLayout = new QVBoxLayout(this);

    // ============================================================================
    // api credentials section
    // ============================================================================
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

    // ============================================================================
    // ftp deployment section
    // ============================================================================
    QGroupBox* ftpGroup = new QGroupBox("Default FTP Deployment (Optional)", this);
    QFormLayout* ftpLayout = new QFormLayout(ftpGroup);

    ftpHostInput = new QLineEdit(this);
    ftpHostInput->setPlaceholderText("ftp.yourdomain.com");

    // use a spinbox to ensure the user can only enter valid network ports
    ftpPortInput = new QSpinBox(this);
    ftpPortInput->setRange(1, 65535);
    ftpPortInput->setValue(21); 

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

    // ============================================================================
    // appearance section
    // ============================================================================
    QGroupBox* uiGroup = new QGroupBox("Appearance", this);
    QFormLayout* uiLayout = new QFormLayout(uiGroup);
    
    themeSelector = new QComboBox(this);
    themeSelector->addItem("Dark Mode", "dark");
    themeSelector->addItem("Light Mode", "light");
    uiLayout->addRow("Application Theme:", themeSelector);
    
    mainLayout->addWidget(uiGroup);
    
    // ============================================================================
    // dialog controls
    // ============================================================================
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    QPushButton* btnSave = new QPushButton("Save", this);
    QPushButton* btnCancel = new QPushButton("Cancel", this);

    buttonLayout->addStretch();
    buttonLayout->addWidget(btnSave);
    buttonLayout->addWidget(btnCancel);
    
    mainLayout->addLayout(buttonLayout);

    // initialize from os registry/settings
    QSettings settings;
    apiKeyInput->setText(settings.value("api_key", "").toString());
    githubPatInput->setText(settings.value("github_pat", "").toString());
    
    ftpHostInput->setText(settings.value("ftp_host", "").toString());
    ftpPortInput->setValue(settings.value("ftp_port", 21).toInt());
    ftpUsernameInput->setText(settings.value("ftp_username", "").toString());
    ftpPasswordInput->setText(settings.value("ftp_password", "").toString());

    // load theme
    QString savedTheme = settings.value("theme", "dark").toString();
    int index = themeSelector->findData(savedTheme);
    if (index != -1) themeSelector->setCurrentIndex(index);

    // wire connections
    connect(btnSave, &QPushButton::clicked, this, &SettingsDialog::saveSettings);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);

    ThemeManager::apply(this);
}

// ============================================================================
// save handler
// ============================================================================

void SettingsDialog::saveSettings() {
    QString apiKey = apiKeyInput->text().trimmed();
    
    // validate that the core api key is actually provided to prevent app failure
    if (apiKey.isEmpty()) {
        QMessageBox::warning(this, "Validation Error", "The Gemini API Key cannot be empty.");
        return;
    }

    QSettings settings;
    settings.setValue("api_key", apiKey);
    settings.setValue("github_pat", githubPatInput->text().trimmed());
    
    settings.setValue("theme", themeSelector->currentData().toString());

    // save ftp configs
    settings.setValue("ftp_host", ftpHostInput->text().trimmed());
    settings.setValue("ftp_port", ftpPortInput->value());
    settings.setValue("ftp_username", ftpUsernameInput->text().trimmed());
    settings.setValue("ftp_password", ftpPasswordInput->text()); // passwords can contain leading/trailing spaces, don't trim
    
    accept();
}

// ============================================================================
// public accessors
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