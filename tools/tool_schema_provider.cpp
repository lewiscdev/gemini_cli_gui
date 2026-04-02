/**
 * @file tool_schema_provider.cpp
 * @brief Implementation of the tool schema provider.
 *
 * Hardcodes the schemas for all local actions. ensures that parameters 
 * perfectly match the arguments expected by the agent action manager.
 */

#include "tool_schema_provider.h"

#include <QJsonObject>
#include <QJsonArray>

// ============================================================================
// schema generation
// ============================================================================

QJsonArray ToolSchemaProvider::getAvailableTools() {
    QJsonArray toolsArray;
    QJsonArray functionDeclarations;

    // helper lambda to keep the schema construction readable
    auto addFunction = [&](const QString& name, const QString& description, const QJsonObject& properties, const QJsonArray& requiredParams) {
        QJsonObject funcDecl;
        funcDecl["name"] = name;
        funcDecl["description"] = description;

        QJsonObject parameters;
        parameters["type"] = "OBJECT";
        parameters["properties"] = properties;
        
        // every tool must enforce its required parameters
        QJsonArray required = requiredParams;
        required.append("rationale"); // uniformly demand reasoning
        parameters["required"] = required;

        funcDecl["parameters"] = parameters;
        functionDeclarations.append(funcDecl);
    };

    // --- tool: write_file ---
    QJsonObject writeProps;
    writeProps["target"] = QJsonObject{{"type", "STRING"}, {"description", "relative path to the file."}};
    writeProps["payload"] = QJsonObject{{"type", "STRING"}, {"description", "the complete text or code payload."}};
    writeProps["rationale"] = QJsonObject{{"type", "STRING"}, {"description", "why this file is being created or modified."}};
    addFunction("write_file", "creates or completely overwrites a file.", writeProps, {"target", "payload"});

    // --- tool: read_file ---
    QJsonObject readProps;
    readProps["target"] = QJsonObject{{"type", "STRING"}, {"description", "relative path to the file to read."}};
    readProps["rationale"] = QJsonObject{{"type", "STRING"}, {"description", "why this file needs to be read."}};
    addFunction("read_file", "reads the contents of a local file.", readProps, {"target"});

    // --- tool: list_directory ---
    QJsonObject listProps;
    listProps["target"] = QJsonObject{{"type", "STRING"}, {"description", "relative path to the directory (use '.' for root)."}};
    listProps["rationale"] = QJsonObject{{"type", "STRING"}, {"description", "why you are inspecting this directory."}};
    addFunction("list_directory", "lists all files and folders in a directory.", listProps, {"target"});

    // --- tool: execute_shell_command ---
    QJsonObject shellProps;
    shellProps["command"] = QJsonObject{{"type", "STRING"}, {"description", "the terminal command to run."}};
    shellProps["rationale"] = QJsonObject{{"type", "STRING"}, {"description", "why you are running this command."}};
    addFunction("execute_shell_command", "executes a background terminal command (cmd.exe or /bin/sh).", shellProps, {"command"});

    // --- tool: git_manager ---
    QJsonObject gitProps;
    QJsonObject itemsObj;
    itemsObj["type"] = "STRING";
    QJsonObject blockObj;
    blockObj["type"] = "ARRAY";
    blockObj["items"] = itemsObj;
    gitProps["command_blocks"] = QJsonObject{
        {"type", "ARRAY"}, 
        {"description", "a 2d array of git commands. e.g., [['add', '.'], ['commit', '-m', 'msg']]"}, 
        {"items", blockObj}
    };
    gitProps["rationale"] = QJsonObject{{"type", "STRING"}, {"description", "why these version control changes are being made."}};
    addFunction("git_manager", "executes a batch of git operations sequentially.", gitProps, {"command_blocks"});

    // --- tool: upload_ftp ---
    QJsonObject ftpProps;
    ftpProps["local_path"] = QJsonObject{{"type", "STRING"}, {"description", "relative path to the local file to upload."}};
    ftpProps["remote_url"] = QJsonObject{{"type", "STRING"}, {"description", "optional: remote ftp url. leave blank to use global settings."}};
    ftpProps["username"] = QJsonObject{{"type", "STRING"}, {"description", "optional: ftp username."}};
    ftpProps["password"] = QJsonObject{{"type", "STRING"}, {"description", "optional: ftp password."}};
    ftpProps["rationale"] = QJsonObject{{"type", "STRING"}, {"description", "why this file is being deployed."}};
    addFunction("upload_ftp", "uploads a single file to a remote ftp server.", ftpProps, {"local_path"});

    // --- tool: fetch_webpage ---
    QJsonObject webProps;
    webProps["url"] = QJsonObject{{"type", "STRING"}, {"description", "the full http/https url to fetch."}};
    webProps["rationale"] = QJsonObject{{"type", "STRING"}, {"description", "why you need the html source of this page."}};
    addFunction("fetch_webpage", "fetches the raw html source code from a web url.", webProps, {"url"});

    // --- tool: take_screenshot ---
    QJsonObject screenProps;
    screenProps["rationale"] = QJsonObject{{"type", "STRING"}, {"description", "why you need visual verification right now."}};
    addFunction("take_screenshot", "captures an image of the primary display to verify gui execution.", screenProps, {});

    // wrap the definitions in the root tool object required by google
    QJsonObject functionDeclarationsObj;
    functionDeclarationsObj["function_declarations"] = functionDeclarations;
    toolsArray.append(functionDeclarationsObj);

    return toolsArray;
}