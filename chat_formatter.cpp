/**
 * @file chat_formatter.cpp
 * @brief Implementation of the markdown-to-HTML parser.
 */

#include "chat_formatter.h"

#include <QStringList>
#include <QRegularExpression>

QString ChatFormatter::formatMarkdownToHtml(const QString& markdown) {
    QString html;
    QStringList lines = markdown.split('\n');
    
    bool inCodeBlock = false;
    QString currentLang;
    QString rawCodeBuffer;
    QString styledHtmlBuffer;

    for (const QString& line : lines) {
        QString trimmed = line.trimmed();

        if (trimmed.startsWith("```")) {
            if (!inCodeBlock) {
                // START OF CODE BLOCK
                inCodeBlock = true;
                currentLang = trimmed.mid(3).trimmed();
                if (currentLang.isEmpty()) currentLang = "CODE";
                
                rawCodeBuffer.clear();
                styledHtmlBuffer.clear();
            } else {
                // END OF CODE BLOCK
                inCodeBlock = false;
                
                // Encode the raw code securely into a URL-safe Base64 string for the Copy link
                QByteArray encodedPayload = rawCodeBuffer.trimmed().toUtf8().toBase64(QByteArray::Base64UrlEncoding);
                
                // Build the web UI box with the Copy Button
                html += QString("<br><table width=\"100%\" style=\"background-color: #F8F9FA; border: 1px solid #E5E7EB; border-collapse: collapse;\">"
                                "<tr>"
                                "<td style=\"background-color: #E5E7EB; padding: 4px 8px; font-family: sans-serif; font-size: 11px; color: #374151;\"><b>%1</b></td>"
                                "<td align=\"right\" style=\"background-color: #E5E7EB; padding: 4px 8px;\">"
                                "<a href=\"copy:%2\" style=\"color: #0078D7; text-decoration: none; font-family: sans-serif; font-size: 11px; font-weight: bold;\">📋 Copy</a></td>"
                                "</tr>"
                                "<tr><td colspan=\"2\" style=\"padding: 8px; font-family: 'Courier New', monospace; font-size: 13px; color: #1F2937;\">"
                                "%3</td></tr></table><br>")
                                .arg(currentLang.toUpper())
                                .arg(QString::fromLatin1(encodedPayload))
                                .arg(styledHtmlBuffer);
            }
        } else {
            if (inCodeBlock) {
                // Buffer the RAW code exactly as it is (for the clipboard)
                rawCodeBuffer += line + "\n";

                // Buffer the STYLED code for the UI
                QString escaped = line.toHtmlEscaped();
                escaped.replace(QRegularExpression("(&quot;.*?&quot;|'.*?')"), "<span style=\"color: #A31515;\">\\1</span>"); 
                escaped.replace(QRegularExpression("(//.*|#.*)"), "<span style=\"color: #008000;\">\\1</span>"); 
                escaped.replace(QRegularExpression("\\b(def|class|return|if|for|while|import|from|int|void|bool|auto|const|let|var|function|namespace|public|private)\\b"), "<span style=\"color: #0000FF; font-weight: bold;\">\\1</span>"); 
                escaped.replace(QRegularExpression("\\b([A-Za-z0-9_]+)(?=\\()"), "<span style=\"color: #795E26;\">\\1</span>"); 
                escaped.replace("  ", "&nbsp;&nbsp;"); 
                
                styledHtmlBuffer += escaped + "<br>";
            } else {
                // Standard Chat Text Formatting
                QString escaped = line.toHtmlEscaped();
                escaped.replace(QRegularExpression("\\*\\*(.*?)\\*\\*"), "<b>\\1</b>"); 
                escaped.replace(QRegularExpression("`(.*?)`"), "<span style=\"background-color: #F3F4F6; font-family: monospace; padding: 2px 4px; border-radius: 3px;\">\\1</span>"); 
                html += escaped + "<br>";
            }
        }
    }

    // Failsafe for unclosed code blocks
    if (inCodeBlock) {
        QByteArray encodedPayload = rawCodeBuffer.trimmed().toUtf8().toBase64(QByteArray::Base64UrlEncoding);
        html += QString("<br><table width=\"100%\" style=\"background-color: #F8F9FA; border: 1px solid #E5E7EB; border-collapse: collapse;\"><tr><td style=\"background-color: #E5E7EB; padding: 4px 8px; font-family: sans-serif; font-size: 11px; color: #374151;\"><b>%1</b></td><td align=\"right\" style=\"background-color: #E5E7EB; padding: 4px 8px;\"><a href=\"copy:%2\" style=\"color: #0078D7; text-decoration: none; font-family: sans-serif; font-size: 11px; font-weight: bold;\">📋 Copy</a></td></tr><tr><td colspan=\"2\" style=\"padding: 8px; font-family: 'Courier New', monospace; font-size: 13px; color: #1F2937;\">%3</td></tr></table><br>").arg(currentLang.toUpper()).arg(QString::fromLatin1(encodedPayload)).arg(styledHtmlBuffer);
    }

    return html;
}