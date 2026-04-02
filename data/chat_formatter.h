/**
 * @file chat_formatter.h
 * @brief Utility class for parsing markdown into styled HTML.
 *
 * This class isolates all regex, string manipulation, and syntax 
 * highlighting logic away from the main UI thread. It converts raw 
 * LLM markdown into beautiful, web-style HTML tables.
 */

#ifndef CHAT_FORMATTER_H
#define CHAT_FORMATTER_H

#include <QString>

class ChatFormatter {
public:
    // ============================================================================
    // public formatting methods
    // ============================================================================

    /**
     * @brief Parses raw markdown into rich HTML tables with isolated inline syntax highlighting.
     * @param markdown The raw text from the LLM or Database.
     * @return A styled HTML string ready for injection into a QTextEdit.
     */
    [[nodiscard]] static QString formatMarkdownToHtml(const QString& markdown);

    /**
     * @brief Parses raw markdown into HTML while explicitly applying theme-aware colors.
     * @param markdown The raw text from the LLM or Database.
     * @param isDarkTheme True if the dark theme is currently active.
     * @return A themed, styled HTML string ready for UI rendering.
     */
    [[nodiscard]] static QString formatMarkdownToHtml(const QString& markdown, bool isDarkTheme);
};

#endif // CHAT_FORMATTER_H