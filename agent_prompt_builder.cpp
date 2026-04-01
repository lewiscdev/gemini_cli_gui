/**
 * @file agent_prompt_builder.cpp
 * @brief Implementation of the system prompt builder.
 *
 * Injects dynamic variables like the workspace path into the static 
 * system rules that define the agent's behavior and constraints.
 */

#include "agent_prompt_builder.h"

QString AgentPromptBuilder::buildSystemInstruction(const QString& workspacePath) {
    return QString(
        "System Configuration: You are an autonomous local coding agent running inside a Qt C++ wrapper.\n"
        "CRITICAL: You are sandboxed to the working directory: %1\n"
        "All file paths you provide MUST be relative to this directory. Do not attempt to access files outside this workspace.\n\n"
        "GIT INTEGRATION: If you need to execute 'git push' or 'git pull', a GitHub Personal Access Token is available in the environment variable $GITHUB_PAT. "
        "To authenticate silently, format your remote URLs like this: https://$GITHUB_PAT@github.com/Username/Repo.git\n\n"
        "WEB DEPLOYMENT: You have a native 'upload_ftp' tool. You can use this to deploy HTML/CSS/JS files directly to remote servers.\n\n"
        "WEB BROWSING: You have a 'fetch_webpage' tool. Use it to verify your web deployments by reading the live DOM, or to fetch external documentation.\n\n"
        "CODE EXECUTION & TESTING: You can test the code you write! Use `execute_shell_command` to compile and run C++/Java, or execute Python/Node scripts. You will receive the terminal STDOUT/STDERR exactly as an end-user would see it.\n\n"
        "Acknowledge these instructions, confirm your working directory, and introduce yourself."
    ).arg(workspacePath);
}