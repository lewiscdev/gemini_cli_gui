/**
 * @file chat_syntax_highlighter.h
 * @brief Real-time syntax highlighting for the chat interface.
 *
 * This class applies a state-machine-driven syntax highlighter to the 
 * underlying QTextDocument of the chat window. It detects Markdown 
 * code blocks (```) and applies native IDE-style color theming to 
 * keywords, strings, comments, and functions.
 */

#ifndef CHAT_SYNTAX_HIGHLIGHTER_H
#define CHAT_SYNTAX_HIGHLIGHTER_H

#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegularExpression>
#include <QVector>

class ChatSyntaxHighlighter : public QSyntaxHighlighter {
    Q_OBJECT

public:
    /**
     * @brief Constructs the highlighter and binds it to the chat document.
     * @param parent The QTextDocument belonging to the MainWindow's chat display.
     */
    explicit ChatSyntaxHighlighter(QTextDocument* parent = nullptr);

protected:
    /**
     * @brief Automatically called by Qt whenever a block of text changes.
     * @param text The current line of text being evaluated.
     */
    void highlightBlock(const QString& text) override;

private:
    /**
     * @brief A structure linking a regex pattern to a specific text color/style.
     */
    struct HighlightingRule {
        QRegularExpression pattern;
        QTextCharFormat format;
    };

    QVector<HighlightingRule> programmingRules; ///< Collection of regex rules for code

    // --- Formatting Styles ---
    QTextCharFormat codeBlockBackgroundFormat;
    QTextCharFormat keywordFormat;
    QTextCharFormat classFormat;
    QTextCharFormat singleLineCommentFormat;
    QTextCharFormat multiLineCommentFormat;
    QTextCharFormat quotationFormat;
    QTextCharFormat functionFormat;
};

#endif // CHAT_SYNTAX_HIGHLIGHTER_H