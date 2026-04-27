#pragma once

#include "seer/viewerbase.h"
#include "strategies/jsonstrategy.h"

class QLabel;
class QLineEdit;
class JsonTreeView;
class JsonTreeModel;
class QPushButton;
class QProgressBar;
class QListView;
class QThread;
class SearchWorker;
class PathNavigator;
class SearchPanel;
struct SearchResult;

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
    void updateTheme(int) override;

private:
    void loadImpl(QBoxLayout* lay_content, QHBoxLayout* lay_ctrlbar) override;
    void onTextViewBtnClicked();

    void initTopWnd();
    void startBackgroundLoad(JsonTreeModel* model, const QString& path);
    void updateStatusBarStats(JsonTreeModel* model);
    QString formatFileSize(qint64 bytes) const;

    void startSearch();
    void cancelSearch();

    struct {
        QWidget* wnd_bg        = nullptr;
        QLineEdit* input       = nullptr;
        QAction* action_global = nullptr;
    } m_top;

    struct {
        QWidget* wnd_bg              = nullptr;
        QWidget* breadcrumbs_wnd     = nullptr;
        QHBoxLayout* breadcrumbs_lay = nullptr;
        QLabel* value_label          = nullptr;  // For the " = value" part
        QLabel* stats                = nullptr;  // Center: node statistics
        QLabel* info = nullptr;  // Right: info icon with tooltip
    } m_btm;

    JsonTreeView* m_view;
    JsonTreeModel* m_model;
    QProgressBar* m_progress_bar;

    // control bar btn
    QPushButton* m_btn_text_view;

    // Search & Navigation
    SearchPanel* m_search_panel = nullptr;
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
