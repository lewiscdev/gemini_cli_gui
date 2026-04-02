/**
 * @file chat_formatter.cpp
 * @brief Implementation of the markdown-to-HTML parser.
 *
 * Provides utilities to safely escape raw text and apply theme-aware 
 * HTML styling, including syntax highlighting and code block formatting.
 */

#include "chat_formatter.h"

#include <QStringList>
#include <QRegularExpression>

// ============================================================================
// public formatting methods
// ============================================================================

QString ChatFormatter::formatMarkdownToHtml(const QString& markdown) {
    // default to light theme if no boolean is provided
    return formatMarkdownToHtml(markdown, false);
}

QString ChatFormatter::formatMarkdownToHtml(const QString& markdown, bool isDark) {
    QString html;
    QStringList lines = markdown.split('\n');
    bool inCodeBlock = false;
    QString currentLang, rawCodeBuffer, styledHtmlBuffer;

    // defined high-contrast palette based on active theme
    
    // code block backgrounds (sunken/inset look)
    QString codeBg = isDark ? "#0F172A" : "#F1F5F9"; 
    QString headBg = isDark ? "#1E293B" : "#E2E8F0"; 
    QString borderColor = isDark ? "#334155" : "#CBD5E1"; 
    QString codeFg = isDark ? "#F8FAFC" : "#0F172A"; 
    
    // syntax highlight colors
    QString strCol = isDark ? "#CE9178" : "#A31515";
    QString cmtCol = isDark ? "#6A9955" : "#008000";
    QString kwCol  = isDark ? "#569CD6" : "#0000FF";
    QString fnCol  = isDark ? "#DCDCAA" : "#795E26";
    
    // inline code background
    QString inCode = isDark ? "#0F172A" : "#CBD5E1"; 
    QString linkCol= isDark ? "#60A5FA" : "#0078D7";

    for (const QString& line : lines) {
        QString trimmed = line.trimmed();

        if (trimmed.startsWith("```")) {
            if (!inCodeBlock) {
                inCodeBlock = true;
                currentLang = trimmed.mid(3).trimmed();
                if (currentLang.isEmpty()) currentLang = "CODE";
                rawCodeBuffer.clear(); 
                styledHtmlBuffer.clear();
            } else {
                inCodeBlock = false;
                QByteArray encodedPayload = rawCodeBuffer.trimmed().toUtf8().toBase64(QByteArray::Base64UrlEncoding);
                
                // single-column outer table, with forced 'bgcolor' on the individual cells
                html += QString("<br><table width=\"100%\" style=\"border: 1px solid %2; border-collapse: collapse;\">"
                                "<tr><td bgcolor=\"%3\" style=\"border-bottom: 1px solid %2; padding: 4px 8px;\">"
                                "<table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" style=\"border: none;\">"
                                "<tr><td style=\"border: none; font-family: sans-serif; font-size: 11px; color: %6;\"><b>%4</b></td>"
                                "<td align=\"right\" style=\"border: none;\"><a href=\"copy:%5\" style=\"color: %7; text-decoration: none; font-size: 11px; font-weight: bold;\">📋 Copy</a></td></tr>"
                                "</table></td></tr>"
                                "<tr><td bgcolor=\"%1\" style=\"padding: 10px; font-family: 'Courier New', monospace; font-size: 13px; color: %6;\">%8</td></tr>"
                                "</table><br>")
                                .arg(codeBg, borderColor, headBg, currentLang.toUpper(), QString::fromLatin1(encodedPayload), codeFg, linkCol, styledHtmlBuffer);
            }
        } else {
            if (inCodeBlock) {
                rawCodeBuffer += line + "\n";
                QString escaped = line.toHtmlEscaped();
                escaped.replace(QRegularExpression("(&quot;.*?&quot;|'.*?')"), QString("<span style=\"color: %1;\">\\1</span>").arg(strCol)); 
                escaped.replace(QRegularExpression("(//.*|#.*)"), QString("<span style=\"color: %1;\">\\1</span>").arg(cmtCol)); 
                escaped.replace(QRegularExpression("\\b(def|class|return|if|for|while|import|from|int|void|bool|auto|const|let|var|function|namespace|public|private)\\b"), QString("<span style=\"color: %1; font-weight: bold;\">\\1</span>").arg(kwCol)); 
                escaped.replace(QRegularExpression("\\b([A-Za-z0-9_]+)(?=\\()"), QString("<span style=\"color: %1;\">\\1</span>").arg(fnCol)); 
                escaped.replace("  ", "&nbsp;&nbsp;"); 
                styledHtmlBuffer += escaped + "<br>";
            } else {
                QString escaped = line.toHtmlEscaped();
                escaped.replace(QRegularExpression("\\*\\*(.*?)\\*\\*"), "<b>\\1</b>"); 
                // updated inline code span to use the contrasting background and border radius
                escaped.replace(QRegularExpression("`(.*?)`"), QString("<span style=\"background-color: %1; font-family: 'Courier New', monospace; padding: 2px 5px; border-radius: 4px;\">\\1</span>").arg(inCode)); 
                html += escaped + "<br>";
            }
        }
    }
    
    // failsafe for unclosed blocks
    if (inCodeBlock) {
        QByteArray encodedPayload = rawCodeBuffer.trimmed().toUtf8().toBase64(QByteArray::Base64UrlEncoding);
        // single-column outer table, with forced 'bgcolor' on the individual cells
        html += QString("<br><table width=\"100%\" style=\"border: 1px solid %2; border-collapse: collapse;\">"
                        "<tr><td bgcolor=\"%3\" style=\"border-bottom: 1px solid %2; padding: 4px 8px;\">"
                        "<table width=\"100%\" cellpadding=\"0\" cellspacing=\"0\" style=\"border: none;\">"
                        "<tr><td style=\"border: none; font-family: sans-serif; font-size: 11px; color: %6;\"><b>%4</b></td>"
                        "<td align=\"right\" style=\"border: none;\"><a href=\"copy:%5\" style=\"color: %7; text-decoration: none; font-size: 11px; font-weight: bold;\">📋 Copy</a></td></tr>"
                        "</table></td></tr>"
                        "<tr><td bgcolor=\"%1\" style=\"padding: 10px; font-family: 'Courier New', monospace; font-size: 13px; color: %6;\">%8</td></tr>"
                        "</table><br>")
                        .arg(codeBg, borderColor, headBg, currentLang.toUpper(), QString::fromLatin1(encodedPayload), codeFg, linkCol, styledHtmlBuffer);
    }

    return html;
}