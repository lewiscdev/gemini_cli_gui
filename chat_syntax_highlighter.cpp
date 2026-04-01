/**
 * @file chat_syntax_highlighter.cpp
 * @brief Implementation of the real-time chat syntax highlighter.
 *
 * Uses QRegularExpression and Qt's QSyntaxHighlighter state machine 
 * to identify markdown code blocks and apply IDE-style themes to 
 * keywords, functions, strings, and comments.
 */

#include "chat_syntax_highlighter.h"

/**
 * @brief Constructs the highlighter rules and formats.
 */
ChatSyntaxHighlighter::ChatSyntaxHighlighter(QTextDocument* parent)
    : QSyntaxHighlighter(parent) {
    
    HighlightingRule rule;

    // 1. keyword format (ide blue)
    keywordFormat.setForeground(QColor("#569CD6"));
    keywordFormat.setFontWeight(QFont::Bold);
    
    // generic list supporting c++, python, and javascript
    QStringList keywordPatterns = {
        "\\bclass\\b", "\\bconst\\b", "\\bdouble\\b", "\\benum\\b", "\\bexplicit\\b",
        "\\bfriend\\b", "\\binline\\b", "\\bint\\b", "\\blong\\b", "\\bnamespace\\b",
        "\\boperator\\b", "\\bprivate\\b", "\\bprotected\\b", "\\bpublic\\b",
        "\\bshort\\b", "\\bsignals\\b", "\\bsigned\\b", "\\bslots\\b", "\\bstatic\\b",
        "\\bstruct\\b", "\\btemplate\\b", "\\btypedef\\b", "\\btypename\\b",
        "\\bunion\\b", "\\bunsigned\\b", "\\bvirtual\\b", "\\bvoid\\b", "\\bvolatile\\b",
        "\\bbool\\b", "\\bQString\\b", "\\bdef\\b", "\\bimport\\b", "\\bfrom\\b",
        "\\bauto\\b", "\\bvar\\b", "\\blet\\b", "\\bfunction\\b"
    };
    
    for (const QString& pattern : keywordPatterns) {
        rule.pattern = QRegularExpression(pattern);
        rule.format = keywordFormat;
        programmingRules.append(rule);
    }

    // 2. qt class format (ide teal)
    classFormat.setForeground(QColor("#4EC9B0"));
    classFormat.setFontWeight(QFont::Bold);
    rule.pattern = QRegularExpression("\\bQ[A-Z][A-Za-z]+\\b"); 
    rule.format = classFormat;
    programmingRules.append(rule);

    // 3. function format (ide yellow)
    functionFormat.setForeground(QColor("#DCDCAA"));
    rule.pattern = QRegularExpression("\\b[A-Za-z0-9_]+(?=\\()");
    rule.format = functionFormat;
    programmingRules.append(rule);

    // 4. string/quotation format (ide orange)
    quotationFormat.setForeground(QColor("#CE9178"));
    rule.pattern = QRegularExpression("\".*?\"|'.*?'");
    rule.format = quotationFormat;
    programmingRules.append(rule);

    // 5. single line comment format (ide green)
    singleLineCommentFormat.setForeground(QColor("#6A9955"));
    rule.pattern = QRegularExpression("//[^\n]*|#[^\n]*");
    rule.format = singleLineCommentFormat;
    programmingRules.append(rule);

    // 6. code block background (ide dark gray)
    codeBlockBackgroundFormat.setBackground(QColor("#1E1E1E"));
    codeBlockBackgroundFormat.setForeground(QColor("#D4D4D4")); 
    codeBlockBackgroundFormat.setFontFamily("Courier New"); // force monospaced font
}

/**
 * @brief Evaluates each text block to apply background and syntax formatting.
 */
void ChatSyntaxHighlighter::highlightBlock(const QString& text) {
    // state 0: standard chat text
    // state 1: trapped inside a markdown code block
    int state = previousBlockState();

    // check if this specific line is a markdown code boundary (e.g., ```cpp or ```)
    QRegularExpression codeBlockMarker("^```");
    QRegularExpressionMatch match = codeBlockMarker.match(text);

    if (match.hasMatch()) {
        // toggle the state machine
        if (state == 1) {
            state = 0; // closing the block
        } else {
            state = 1; // opening the block
        }
        
        // shade the boundary line itself
        setFormat(0, text.length(), codeBlockBackgroundFormat); 
        setCurrentBlockState(state);
        return; 
    }

    // if we are trapped inside a code block, format it
    if (state == 1) {
        // paint the entire background of the line dark gray
        setFormat(0, text.length(), codeBlockBackgroundFormat);

        // layer the syntax highlighting rules on top of the background
        for (const HighlightingRule& rule : std::as_const(programmingRules)) {
            QRegularExpressionMatchIterator matchIterator = rule.pattern.globalMatch(text);
            while (matchIterator.hasNext()) {
                QRegularExpressionMatch rMatch = matchIterator.next();
                setFormat(rMatch.capturedStart(), rMatch.capturedLength(), rule.format);
            }
        }
    }

    // save the state so the next line of text knows if it's still in the block
    setCurrentBlockState(state);
}