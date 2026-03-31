#include "settings_dialog.h"
#include <QVBoxLayout>
#include <QLabel>
#include <QSettings>

SettingsDialog::SettingsDialog(QWidget* parent) : QDialog(parent) {
    setupUi();
    loadExistingSettings();
    setWindowTitle("Application Settings");
    resize(400, 150);
}

void SettingsDialog::setupUi() {
    QVBoxLayout* layout = new QVBoxLayout(this);

    QLabel* instructionLabel = new QLabel("Enter your Google Gemini API Key:", this);
    
    apiKeyInput = new QLineEdit(this);
    // mask the input for shoulder-surfing security
    apiKeyInput->setEchoMode(QLineEdit::PasswordEchoOnEdit); 

    saveButton = new QPushButton("Save Configuration", this);
    connect(saveButton, &QPushButton::clicked, this, &SettingsDialog::saveAndClose);

    layout->addWidget(instructionLabel);
    layout->addWidget(apiKeyInput);
    layout->addWidget(saveButton);
}

void SettingsDialog::loadExistingSettings() {
    // initialize QSettings using the global identity set in main.cpp
    QSettings settings;
    // load the key if it exists, otherwise return an empty string
    QString savedKey = settings.value("api_key", "").toString();
    
    if (!savedKey.isEmpty()) {
        apiKeyInput->setText(savedKey);
    }
}

void SettingsDialog::saveAndClose() {
    QSettings settings;
    // securely commit the value to the system registry
    settings.setValue("api_key", apiKeyInput->text().trimmed());
    
    // close the modal and return QDialog::Accepted to the caller
    accept(); 
}

QString SettingsDialog::getApiKey() const {
    return apiKeyInput->text().trimmed();
}