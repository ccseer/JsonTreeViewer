#pragma once

#include "seer/viewerbase.h"

class QLabel;
class QLineEdit;
class JsonTreeView;
class JsonTreeModel;
class QPushButton;

class JsonTreeViewer : public ViewerBase {
    Q_OBJECT
public:
    explicit JsonTreeViewer(QWidget* parent = nullptr);
    ~JsonTreeViewer() override;

    QString name() const override
    {
        return "JsonTreeViewer";
    }

    QSize getContentSize() const override;
    void updateDPR(qreal) override;

private:
    void loadImpl(QBoxLayout* lay_content, QHBoxLayout* lay_ctrlbar) override;
    void onTextViewBtnClicked();

    void initTopWnd();
    void startBackgroundLoad(JsonTreeModel* model, const QString& path);
    void updateStatusBarStats(JsonTreeModel* model);
    QString formatFileSize(qint64 bytes) const;

    struct {
        QWidget* wnd_bg   = nullptr;
        QLineEdit* filter = nullptr;
    } m_top;

    struct {
        QLabel* path_value = nullptr;  // Left: path and value
        QLabel* stats      = nullptr;  // Center: node statistics
        QLabel* info       = nullptr;  // Right: info icon with tooltip
    } m_statusbar;

    QPushButton* m_btn_text_view;

    JsonTreeView* m_view;

    qint64 m_load_time_ms = 0;
};

/////////////////////////////////////////////////////////////////
class JTPlugin : public QObject, public ViewerPluginInterface {
    Q_OBJECT
    Q_PLUGIN_METADATA(IID ViewerPluginInterface_iid FILE "../bin/plugin.json")
    Q_INTERFACES(ViewerPluginInterface)
public:
    ViewerBase* createViewer(QWidget* parent = nullptr) override
    {
        return new JsonTreeViewer(parent);
    }
};
