#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>

// an encapsulated modal for secure credential entry
class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    
    // allows the main window to retrieve the entered key
    QString getApiKey() const;

private slots:
    void saveAndClose();

private:
    QLineEdit* apiKeyInput;
    QPushButton* saveButton;
    
    void setupUi();
    void loadExistingSettings();
};

#endif // SETTINGS_DIALOG_H