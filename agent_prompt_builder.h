/**
 * @file agent_prompt_builder.h
 * @brief Constructs the initial system instructions for the llm.
 *
 * This class isolates the hardcoded persona, capability descriptions, 
 * and sandbox rules away from the core ui layer.
 */

#ifndef AGENT_PROMPT_BUILDER_H
#define AGENT_PROMPT_BUILDER_H

#include <QString>

class AgentPromptBuilder {
public:
    /**
     * @brief Dynamically constructs the comprehensive system instructions.
     * @param workspacePath The absolute path to the active sandboxed directory.
     * @return The formatted system prompt string.
     */
    [[nodiscard]] static QString buildSystemInstruction(const QString& workspacePath);
};

#endif // AGENT_PROMPT_BUILDER_H