/**
 * @file ftp_upload_action.cpp
 * @brief Implementation of the native FTP deployment tool.
 *
 * Parses the agent's connection credentials, opens the local file stream,
 * and manages the asynchronous network put request to upload the file.
 */

#include "ftp_upload_action.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QUrl>
#include <QEventLoop>

FtpUploadAction::FtpUploadAction(QObject* parent) : BaseAgentAction(parent) {}

QString FtpUploadAction::getName() const {
    return "upload_ftp";
}

void FtpUploadAction::execute(const AgentCommand& command, const QString& workspacePath) {
    // parse the json payload to extract the ftp credentials and destination
    QJsonObject ftpArgs = QJsonDocument::fromJson(command.payload.toUtf8()).object();
    QString remoteUrl = ftpArgs["remote_url"].toString();
    
    QUrl url(remoteUrl);
    url.setUserName(ftpArgs["username"].toString());
    url.setPassword(ftpArgs["password"].toString());

    // the target already contains the absolute, verified local path from the router
    QFile* fileToUpload = new QFile(command.target);
    if (!fileToUpload->open(QIODevice::ReadOnly)) {
        emit actionFinished("[system error: could not open local file for ftp upload]");
        delete fileToUpload;
        return;
    }

    QNetworkAccessManager ftpManager;
    QNetworkRequest request(url);
    
    // execute the network put request
    QNetworkReply* reply = ftpManager.put(request, fileToUpload);
    
    // pause local execution until the asynchronous upload completes
    QEventLoop loop;
    connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QString systemFeedbackMsg;
    
    // evaluate the network response
    if (reply->error() == QNetworkReply::NoError) {
        systemFeedbackMsg = "[system success: file uploaded successfully to " + remoteUrl + "]";
    } else {
        systemFeedbackMsg = "[system error: ftp upload failed - " + reply->errorString() + "]";
    }
    
    // cleanup memory and bind the file stream to the reply for safe deletion
    reply->deleteLater();
    fileToUpload->setParent(reply); 

    // broadcast the result back to the llm
    emit actionFinished(systemFeedbackMsg);
}