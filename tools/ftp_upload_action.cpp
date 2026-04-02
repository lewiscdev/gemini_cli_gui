/**
 * @file ftp_upload_action.cpp
 * @brief Implementation of the ftp upload tool.
 * * Manages secure file reading, json credential parsing, global setting 
 * fallbacks, and blocking execution until the asynchronous upload completes.
 */

#include "ftp_upload_action.h"

#include <QSettings>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QEventLoop>
#include <QUrl>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

// ============================================================================
// constructor and initialization
// ============================================================================

FtpUploadAction::FtpUploadAction(QObject* parent) : BaseAgentAction(parent) {
    networkManager = new QNetworkAccessManager(this);
}

// ============================================================================
// public interface
// ============================================================================

QString FtpUploadAction::getName() const {
    return "upload_ftp";
}

// ============================================================================
// execution logic
// ============================================================================

void FtpUploadAction::execute(const AgentCommand& command, const QString& workspacePath) {
    Q_UNUSED(workspacePath); // the path is already verified and made absolute by the manager

    QString localFilePath = command.target;
    QFileInfo fileInfo(localFilePath);
    currentFileName = fileInfo.fileName();

    // extract the credentials payload provided by the llm
    QJsonDocument doc = QJsonDocument::fromJson(command.payload.toUtf8());
    QJsonObject ftpArgs = doc.object();
    
    QString remoteUrlStr = ftpArgs["remote_url"].toString();
    QString username = ftpArgs["username"].toString();
    QString password = ftpArgs["password"].toString();

    // fallback to global settings if the llm left credentials blank
    QSettings settings;
    if (remoteUrlStr.isEmpty() || remoteUrlStr == "ftp://placeholder.com") {
        QString host = settings.value("ftp_host", "").toString();
        int port = settings.value("ftp_port", 21).toInt();
        if (!host.isEmpty()) {
            remoteUrlStr = QString("ftp://%1:%2/%3").arg(host).arg(port).arg(currentFileName);
        }
    }
    if (username.isEmpty() || username == "placeholder_user") {
        username = settings.value("ftp_username", "").toString();
    }
    if (password.isEmpty() || password == "placeholder_pass") {
        password = settings.value("ftp_password", "").toString();
    }

    if (remoteUrlStr.isEmpty()) {
        emit actionFinished("System Error: No valid FTP configuration found. Please provide a URL or update Global Settings.");
        return;
    }

    QFile* file = new QFile(localFilePath);
    if (!file->open(QIODevice::ReadOnly)) {
        emit actionFinished(QString("System Error: Could not read local file '%1'.").arg(localFilePath));
        file->deleteLater();
        return;
    }

    QUrl url(remoteUrlStr);
    if (!username.isEmpty()) url.setUserName(username);
    if (!password.isEmpty()) url.setPassword(password);

    QNetworkRequest request(url);
    QNetworkReply* reply = networkManager->put(request, file);

    // print progress to the ui without sending it back to the llm
    connect(reply, &QNetworkReply::uploadProgress, this, &FtpUploadAction::onUploadProgress);

    // block execution until the upload finishes
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() == QNetworkReply::NoError) {
        emit actionFinished(QString("[system upload_ftp]: Successfully deployed %1 to %2").arg(currentFileName, url.host()));
    } else {
        emit actionFinished(QString("System Error [upload_ftp]: %1").arg(reply->errorString()));
    }

    file->setParent(reply);
    reply->deleteLater();
}

// ============================================================================
// network callbacks
// ============================================================================

void FtpUploadAction::onUploadProgress(qint64 bytesSent, qint64 bytesTotal) {
    if (bytesTotal > 0) {
        int percent = (bytesSent * 100) / bytesTotal;
        // prepend ui_only so the main window knows not to save this to sqlite or send it to google
        emit actionFinished(QString("UI_ONLY:<span style=\"color: gray;\"><i>[FTP Uploading %1: %2%]</i></span>").arg(currentFileName).arg(percent));
    }
}