/**
 * @file theme_manager.h
 * @brief Global theme and stylesheet manager.
 *
 * This utility class provides static methods to determine the current
 * user theme preference and generate the massive raw QSS string needed 
 * to style the entire Qt application globally.
 */

#ifndef THEME_MANAGER_H
#define THEME_MANAGER_H

#include <QString>
#include <QSettings>
#include <QWidget>

class ThemeManager {
public:
    // ============================================================================
    // state queries
    // ============================================================================

    /**
     * @brief Checks the OS registry to see if dark mode is active.
     * @return True if dark mode is selected, false otherwise.
     */
    static bool isDark() {
        QSettings settings;
        return settings.value("theme", "dark").toString() == "dark";
    }

    // ============================================================================
    // stylesheet generation
    // ============================================================================

    /**
     * @brief Generates the full QSS stylesheet based on the requested theme.
     * @param dark True to generate dark mode QSS, false for light mode.
     * @return The raw stylesheet string.
     */
    static QString getStyleSheet(bool dark) {
        if (dark) {
            return R"(
                /* Main App */
                QWidget { background-color: #0F172A; color: #F8FAFC; font-family: 'Segoe UI', sans-serif; }
                QDialog { background-color: #0F172A; border: 1px solid #334155; }
                QGroupBox { color: #94A3B8; font-weight: bold; border: 1px solid #334155; margin-top: 1.5ex; padding: 10px; }
                QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; }
                
                /* Inputs & Standard Buttons */
                QLineEdit, QTextEdit { background-color: #1E293B; color: #F8FAFC; border: 1px solid #334155; border-radius: 6px; padding: 8px; }
                QPushButton { background-color: #334155; color: #F8FAFC; border-radius: 6px; padding: 8px 16px; font-weight: bold; border: none; min-height: 20px; }
                QPushButton:hover { background-color: #475569; }
                QLabel { color: #94A3B8; }

                /* MODERN COMBO BOX (DARK) */
                QComboBox { background-color: #1E293B; color: #F8FAFC; border: 1px solid #334155; border-radius: 6px; padding: 6px 12px; }
                QComboBox::drop-down { border: none; width: 28px; }
                QComboBox::down-arrow { image: url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='14' height='14' fill='none' stroke='%2394A3B8' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><polyline points='6 9 12 15 18 9'></polyline></svg>"); }
                QComboBox QAbstractItemView { background-color: #1E293B; color: #F8FAFC; border: 1px solid #334155; selection-background-color: #334155; border-radius: 4px; outline: none; }

                /* fix: icon-only buttons (restore the paperclip!) */
                QPushButton#iconButton { padding: 8px; min-width: 32px; }

                /* MODERN SCROLLBARS (DARK) */
                QScrollBar:vertical { border: none; background: transparent; width: 12px; margin: 0px; }
                QScrollBar::handle:vertical { background: #334155; border-radius: 6px; min-height: 20px; margin: 2px; }
                QScrollBar::handle:vertical:hover { background: #475569; }
                QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; background: none; } /* hides arrows */
                QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }

                QScrollBar:horizontal { border: none; background: transparent; height: 12px; margin: 0px; }
                QScrollBar::handle:horizontal { background: #334155; border-radius: 6px; min-width: 20px; margin: 2px; }
                QScrollBar::handle:horizontal:hover { background: #475569; }
                QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; background: none; }
                QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }
            
                /* LIST WIDGETS (Session Manager) */
                QListWidget { background-color: #1E293B; color: #F8FAFC; border: 1px solid #334155; border-radius: 6px; padding: 4px; outline: none; }
                QListWidget::item { padding: 8px; border-radius: 4px; }
                QListWidget::item:selected { background-color: #334155; color: #F8FAFC; }
                QListWidget::item:hover:!selected { background-color: #0F172A; }
                )";
        } else {
            return R"(
                /* Main App (Light) */
                QWidget { background-color: #F8FAFC; color: #0F172A; font-family: 'Segoe UI', sans-serif; }
                QDialog { background-color: #F8FAFC; border: 1px solid #E5E7EB; }
                QGroupBox { color: #64748B; font-weight: bold; border: 1px solid #E5E7EB; margin-top: 1.5ex; padding: 10px; }
                QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; }
                
                /* Inputs & Standard Buttons */
                QLineEdit, QTextEdit { background-color: #FFFFFF; color: #0F172A; border: 1px solid #CBD5E1; border-radius: 6px; padding: 8px; }
                QPushButton { background-color: #E2E8F0; color: #0F172A; border-radius: 6px; padding: 8px 16px; font-weight: bold; border: none; min-height: 20px; }
                QPushButton:hover { background-color: #CBD5E1; }
                QLabel { color: #64748B; }

                /* MODERN COMBO BOX (LIGHT) */
                QComboBox { background-color: #FFFFFF; color: #0F172A; border: 1px solid #CBD5E1; border-radius: 6px; padding: 6px 12px; }
                QComboBox::drop-down { border: none; width: 28px; }
                QComboBox::down-arrow { image: url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='14' height='14' fill='none' stroke='%2364748B' stroke-width='2' stroke-linecap='round' stroke-linejoin='round'><polyline points='6 9 12 15 18 9'></polyline></svg>"); }
                QComboBox QAbstractItemView { background-color: #FFFFFF; color: #0F172A; border: 1px solid #CBD5E1; selection-background-color: #E2E8F0; border-radius: 4px; outline: none; }
                
                /* fix: icon-only buttons (restore the paperclip!) */
                QPushButton#iconButton { padding: 8px; min-width: 32px; }

                /* MODERN SCROLLBARS (LIGHT) */
                QScrollBar:vertical { border: none; background: transparent; width: 12px; margin: 0px; }
                QScrollBar::handle:vertical { background: #CBD5E1; border-radius: 6px; min-height: 20px; margin: 2px; }
                QScrollBar::handle:vertical:hover { background: #94A3B8; }
                QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; background: none; } /* hides arrows */
                QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical { background: transparent; }

                QScrollBar:horizontal { border: none; background: transparent; height: 12px; margin: 0px; }
                QScrollBar::handle:horizontal { background: #CBD5E1; border-radius: 6px; min-width: 20px; margin: 2px; }
                QScrollBar::handle:horizontal:hover { background: #94A3B8; }
                QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; background: none; }
                QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal { background: transparent; }
            
                /* LIST WIDGETS (Session Manager) */
                QListWidget { background-color: #FFFFFF; color: #0F172A; border: 1px solid #CBD5E1; border-radius: 6px; padding: 4px; outline: none; }
                QListWidget::item { padding: 8px; border-radius: 4px; }
                QListWidget::item:selected { background-color: #E2E8F0; color: #0F172A; }
                QListWidget::item:hover:!selected { background-color: #F8FAFC; }
                )";
        }
    }

    // ============================================================================
    // application integration
    // ============================================================================

    /**
     * @brief Immediately applies the active theme to the given widget.
     * @param widget Pointer to the widget (typically 'this') to style.
     */
    static void apply(QWidget* widget) {
        widget->setStyleSheet(getStyleSheet(isDark()));
    }
};

#endif