#include "config.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QStandardPaths>

#include "seer/viewerhelper.h"

Config& Config::ins()
{
    static Config inst;
    return inst;
}

Config::Config()
{
    load();
}

QString Config::getIniPath() const
{
    const QString filename = "jsontreeviewer.ini";
    QString dir            = seer::getDLLPath();
    if (dir.isEmpty()) {
        dir = QCoreApplication::applicationDirPath();
    }
    dir.replace("\\", "/");
    if (!dir.endsWith("/")) {
        dir.append("/");
    }
    return dir + filename;
}

void Config::load()
{
    QString iniPath = getIniPath();
    bool fileExists = QFile::exists(iniPath);

    QSettings settings(iniPath, QSettings::IniFormat);

    settings.beginGroup("General");
    m_exportFormat = settings.value("ExportFormat", "indented").toString();
    if (m_exportFormat != "indented" && m_exportFormat != "compact") {
        m_exportFormat = "indented";
    }

    m_exportIndentSpaces = settings.value("ExportIndentSpaces", 2).toInt();
    if (m_exportIndentSpaces < 0 || m_exportIndentSpaces > 8) {
        m_exportIndentSpaces = 2;
    }

    m_doubleClickExpand = settings.value("DoubleClickExpand", true).toBool();
    settings.endGroup();

    settings.beginGroup("Display");
    m_themeMode = settings.value("ThemeMode", "auto").toString();
    if (m_themeMode != "auto" && m_themeMode != "light"
        && m_themeMode != "dark") {
        m_themeMode = "auto";
    }
    m_showStatusBar    = settings.value("ShowStatusBar", true).toBool();
    m_showHeader       = settings.value("ShowHeader", true).toBool();
    m_showBranches     = settings.value("ShowBranches", true).toBool();
    m_showFilterBar    = settings.value("ShowFilterBar", true).toBool();
    m_showColorPreview = settings.value("ShowColorPreview", true).toBool();
    m_arrayIndexStartsAtZero
        = settings.value("ArrayIndexStartsAtZero", true).toBool();
    settings.endGroup();

    settings.beginGroup("Shortcuts");
    m_shortcutCollapseAll = QKeySequence(
        settings.value("CollapseAll", "Ctrl+Shift+[").toString());
    m_shortcutExpandAll
        = QKeySequence(settings.value("ExpandAll", "Ctrl+Shift+]").toString());
    m_shortcutCopyPath
        = QKeySequence(settings.value("CopyPath", "Ctrl+Shift+C").toString());
    m_shortcutExportSelection
        = QKeySequence(settings.value("ExportSelection", "Ctrl+E").toString());
    m_shortcutFilter
        = QKeySequence(settings.value("Filter", "Ctrl+F").toString());
    m_shortcutToggleMode = QKeySequence(
        settings.value("ToggleMode", QKeySequence(Qt::Key_Tab).toString())
            .toString());
    settings.endGroup();

    if (!fileExists) {
        save();
    }
}

void Config::save()
{
    QSettings settings(getIniPath(), QSettings::IniFormat);

    settings.beginGroup("General");
    settings.setValue("ExportFormat", m_exportFormat);
    settings.setValue("ExportIndentSpaces", m_exportIndentSpaces);
    settings.setValue("DoubleClickExpand", m_doubleClickExpand);
    settings.endGroup();

    settings.beginGroup("Display");
    settings.setValue("ThemeMode", m_themeMode);
    settings.setValue("ShowStatusBar", m_showStatusBar);
    settings.setValue("ShowHeader", m_showHeader);
    settings.setValue("ShowBranches", m_showBranches);
    settings.setValue("ShowFilterBar", m_showFilterBar);
    settings.setValue("ShowColorPreview", m_showColorPreview);
    settings.setValue("ArrayIndexStartsAtZero", m_arrayIndexStartsAtZero);
    settings.endGroup();

    settings.beginGroup("Shortcuts");
    settings.setValue("CollapseAll", m_shortcutCollapseAll.toString());
    settings.setValue("ExpandAll", m_shortcutExpandAll.toString());
    settings.setValue("CopyPath", m_shortcutCopyPath.toString());
    settings.setValue("ExportSelection", m_shortcutExportSelection.toString());
    settings.setValue("Filter", m_shortcutFilter.toString());
    settings.setValue("ToggleMode", m_shortcutToggleMode.toString());
    settings.endGroup();
}
