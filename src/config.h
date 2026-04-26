#pragma once

#include <QKeySequence>
#include <QString>

class Config {
public:
    static Config& instance();

    void load();
    void save();

    QString getIniPath() const;

    // General
    QString exportFormat() const
    {
        return m_exportFormat;
    }
    int exportIndentSpaces() const
    {
        return m_exportIndentSpaces;
    }
    bool doubleClickExpand() const
    {
        return m_doubleClickExpand;
    }

    // Display
    QString themeMode() const
    {
        return m_themeMode;
    }
    bool showStatusBar() const
    {
        return m_showStatusBar;
    }
    bool showHeader() const
    {
        return m_showHeader;
    }
    bool showBranches() const
    {
        return m_showBranches;
    }
    bool showFilterBar() const
    {
        return m_showFilterBar;
    }
    bool showColorPreview() const
    {
        return m_showColorPreview;
    }
    bool arrayIndexStartsAtZero() const
    {
        return m_arrayIndexStartsAtZero;
    }

    // Shortcuts
    QKeySequence shortcutCollapseAll() const
    {
        return m_shortcutCollapseAll;
    }
    QKeySequence shortcutExpandAll() const
    {
        return m_shortcutExpandAll;
    }
    QKeySequence shortcutCopyPath() const
    {
        return m_shortcutCopyPath;
    }
    QKeySequence shortcutExportSelection() const
    {
        return m_shortcutExportSelection;
    }
    QKeySequence shortcutFilter() const
    {
        return m_shortcutFilter;
    }

private:
    Config();
    ~Config()                        = default;
    Config(const Config&)            = delete;
    Config& operator=(const Config&) = delete;

    // General
    QString m_exportFormat   = "indented";
    int m_exportIndentSpaces = 2;
    bool m_doubleClickExpand = true;

    // Display
    QString m_themeMode           = "auto";
    bool m_showStatusBar          = true;
    bool m_showHeader             = true;
    bool m_showBranches           = true;
    bool m_showFilterBar          = true;
    bool m_showColorPreview       = true;
    bool m_arrayIndexStartsAtZero = true;

    // Shortcuts
    QKeySequence m_shortcutCollapseAll     = QKeySequence("Ctrl+Shift+[");
    QKeySequence m_shortcutExpandAll       = QKeySequence("Ctrl+Shift+]");
    QKeySequence m_shortcutCopyPath        = QKeySequence("Ctrl+Shift+C");
    QKeySequence m_shortcutExportSelection = QKeySequence("Ctrl+E");
    QKeySequence m_shortcutFilter          = QKeySequence("Ctrl+F");
};
