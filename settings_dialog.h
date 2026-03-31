#ifndef SETTINGS_DIALOG_H
#define SETTINGS_DIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);
    QString getApiKey() const;
    QString getWorkspaceDirectory() const; // NEW

private slots:
    void browseWorkspace(); // NEW: Opens folder picker
    void saveSettings();

private:
    QLineEdit* apiKeyInput;
    QLineEdit* workspaceInput; // NEW
    QPushButton* browseButton; // NEW
    QPushButton* saveButton;
};

#endif // SETTINGS_DIALOG_H