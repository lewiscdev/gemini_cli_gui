/**
 * @file tool_schema_provider.h
 * @brief Generates the json schema definitions for the gemini api.
 *
 * This static utility class provides the exact json structure required 
 * by the gemini api to understand what local tools are available, 
 * what parameters they require, and how to invoke them.
 */

#ifndef TOOL_SCHEMA_PROVIDER_H
#define TOOL_SCHEMA_PROVIDER_H

#include <QJsonArray>

class ToolSchemaProvider {
public:
    // ============================================================================
    // public schema provider
    // ============================================================================

    /**
     * @brief Builds and returns the comprehensive array of tool definitions.
     * @return A QJsonArray containing the strict OpenAPI-style schemas for every capability.
     */
    [[nodiscard]] static QJsonArray getAvailableTools();
};

#endif // TOOL_SCHEMA_PROVIDER_H