/**
 * @file tool_schema_provider.cpp
 * @brief Implementation of the tool schema provider.
 *
 * Constructs and returns the static, multi-tool JSON array that informs 
 * the LLM of its autonomous capabilities and their exact parameters.
 */

#include "tool_schema_provider.h"

#include <QJsonObject>

QJsonArray ToolSchemaProvider::getAvailableTools() {
    QJsonArray toolsArray;

    // --- tool 1: write_file ---
    QJsonObject wfPayload; wfPayload["type"] = "STRING"; wfPayload["description"] = "The exact text content to write.";
    QJsonObject wfTarget; wfTarget["type"] = "STRING"; wfTarget["description"] = "Relative file path (e.g., src/main.cpp).";
    QJsonObject wfProps; wfProps["payload"] = wfPayload; wfProps["target"] = wfTarget;
    QJsonArray wfReq; wfReq.append("target"); wfReq.append("payload");
    QJsonObject wfParams; wfParams["type"] = "OBJECT"; wfParams["properties"] = wfProps; wfParams["required"] = wfReq;
    
    QJsonObject writeFileTool;
    writeFileTool["type"] = "function"; 
    writeFileTool["name"] = "write_file";
    writeFileTool["description"] = "Writes content to a local file in the workspace.";
    writeFileTool["parameters"] = wfParams;
    toolsArray.append(writeFileTool);

    // --- tool 2: read_file ---
    QJsonObject rfTarget; rfTarget["type"] = "STRING"; rfTarget["description"] = "Relative file path to read (e.g., CMakeLists.txt).";
    QJsonObject rfProps; rfProps["target"] = rfTarget;
    QJsonArray rfReq; rfReq.append("target");
    QJsonObject rfParams; rfParams["type"] = "OBJECT"; rfParams["properties"] = rfProps; rfParams["required"] = rfReq;

    QJsonObject readFileTool;
    readFileTool["type"] = "function"; 
    readFileTool["name"] = "read_file";
    readFileTool["description"] = "Reads and returns the exact text content of a local file.";
    readFileTool["parameters"] = rfParams;
    toolsArray.append(readFileTool);

    // --- tool 3: list_directory ---
    QJsonObject ldTarget; ldTarget["type"] = "STRING"; ldTarget["description"] = "Relative directory path to list. Use '.' for the root workspace.";
    QJsonObject ldProps; ldProps["target"] = ldTarget;
    QJsonArray ldReq; ldReq.append("target");
    QJsonObject ldParams; ldParams["type"] = "OBJECT"; ldParams["properties"] = ldProps; ldParams["required"] = ldReq;

    QJsonObject listDirTool;
    listDirTool["type"] = "function"; 
    listDirTool["name"] = "list_directory";
    listDirTool["description"] = "Lists all files and folders in a specified directory.";
    listDirTool["parameters"] = ldParams;
    toolsArray.append(listDirTool);

    // --- tool 4: execute_shell_command ---
    QJsonObject escCommand; escCommand["type"] = "STRING"; escCommand["description"] = "The terminal/shell command to execute (e.g., 'git status', 'cmake --build .', 'npm run test').";
    QJsonObject escProps; escProps["command"] = escCommand;
    QJsonArray escReq; escReq.append("command");
    QJsonObject escParams; escParams["type"] = "OBJECT"; escParams["properties"] = escProps; escParams["required"] = escReq;

    QJsonObject executeShellTool;
    executeShellTool["type"] = "function"; 
    executeShellTool["name"] = "execute_shell_command";
    executeShellTool["description"] = "Executes a shell/terminal command in the workspace and returns the console output (stdout/stderr).";
    executeShellTool["parameters"] = escParams;
    toolsArray.append(executeShellTool);

    // --- tool 5: upload_ftp ---
    QJsonObject ftpLocal; ftpLocal["type"] = "STRING"; ftpLocal["description"] = "Relative path of the local file to upload (e.g., index.html).";
    QJsonObject ftpRemote; ftpRemote["type"] = "STRING"; ftpRemote["description"] = "The destination path and filename on the FTP server (e.g., /public_html/index.html).";
    QJsonObject ftpProps; ftpProps["local_path"] = ftpLocal; ftpProps["remote_path"] = ftpRemote;
    QJsonArray ftpReq; ftpReq.append("local_path"); ftpReq.append("remote_path");
    QJsonObject ftpParams; ftpParams["type"] = "OBJECT"; ftpParams["properties"] = ftpProps; ftpParams["required"] = ftpReq;

    QJsonObject uploadFtpTool;
    uploadFtpTool["type"] = "function"; 
    uploadFtpTool["name"] = "upload_ftp";
    uploadFtpTool["description"] = "Uploads a local file to the live FTP server configured in the user settings.";
    uploadFtpTool["parameters"] = ftpParams;
    toolsArray.append(uploadFtpTool);

    // --- tool 6: fetch_webpage ---
    QJsonObject fwUrl; fwUrl["type"] = "STRING"; fwUrl["description"] = "The full HTTP/HTTPS URL of the webpage to fetch (e.g., https://my-test-site.com/index.html).";
    QJsonObject fwProps; fwProps["url"] = fwUrl;
    QJsonArray fwReq; fwReq.append("url");
    QJsonObject fwParams; fwParams["type"] = "OBJECT"; fwParams["properties"] = fwProps; fwParams["required"] = fwReq;

    QJsonObject fetchWebpageTool;
    fetchWebpageTool["type"] = "function"; 
    fetchWebpageTool["name"] = "fetch_webpage";
    fetchWebpageTool["description"] = "Fetches the raw HTML/text content of a live webpage. Useful for verifying deployments.";
    fetchWebpageTool["parameters"] = fwParams;
    toolsArray.append(fetchWebpageTool);

    // --- tool 7: take_screenshot ---
    QJsonObject tsParams; tsParams["type"] = "OBJECT"; tsParams["properties"] = QJsonObject(); 
    
    QJsonObject screenshotTool;
    screenshotTool["type"] = "function"; 
    screenshotTool["name"] = "take_screenshot";
    screenshotTool["description"] = "Takes a screenshot of the active GUI application you just executed to visually verify the UI layout or read errors.";
    screenshotTool["parameters"] = tsParams;
    toolsArray.append(screenshotTool);

    // --- tool 8: execute_code ---
    QJsonObject ecLang; ecLang["type"] = "STRING"; ecLang["description"] = "The programming language to execute (supported: 'python', 'javascript', 'cpp').";
    QJsonObject ecCode; ecCode["type"] = "STRING"; ecCode["description"] = "The raw source code to execute.";
    QJsonObject ecProps; ecProps["language"] = ecLang; ecProps["code"] = ecCode;
    QJsonArray ecReq; ecReq.append("language"); ecReq.append("code");
    QJsonObject ecParams; ecParams["type"] = "OBJECT"; ecParams["properties"] = ecProps; ecParams["required"] = ecReq;

    QJsonObject executeCodeTool;
    executeCodeTool["type"] = "function"; 
    executeCodeTool["name"] = "execute_code";
    executeCodeTool["description"] = "Executes raw source code in a secure local sandbox and returns the stdout/stderr output.";
    executeCodeTool["parameters"] = ecParams;
    toolsArray.append(executeCodeTool);

    return toolsArray;
}