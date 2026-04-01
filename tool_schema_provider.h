/**
 * @file tool_schema_provider.h
 * @brief Provides the JSON schema definitions for all agent capabilities.
 *
 * This class encapsulates the construction of the JSON tools array required
 * by the Gemini API, decoupling the heavy schema definitions from the
 * asynchronous networking client.
 */

#ifndef TOOL_SCHEMA_PROVIDER_H
#define TOOL_SCHEMA_PROVIDER_H

#include <QJsonArray>

class ToolSchemaProvider {
public:
    /**
     * @brief Generates the flattened JSON schema for all available agent tools.
     * @return A JSON array structured strictly according to the API specification.
     */
    [[nodiscard]] static QJsonArray getAvailableTools();
};

#endif // TOOL_SCHEMA_PROVIDER_H