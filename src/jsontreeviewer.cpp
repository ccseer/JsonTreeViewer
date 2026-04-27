#include "jsontreeviewer.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QProgressBar>
#include <QPushButton>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include "config.h"
#include "jsonnode.h"
#include "jsontreemodel.h"
#include "jsontreeview.h"
#include "loadworker.h"
#include "navigation/pathnavigator.h"
#include "navigation/searchpanel.h"
#include "navigation/searchresultdelegate.h"
#include "navigation/searchworker.h"
#include "seer/viewerhelper.h"
#include "style_assets.h"

#define qprintt qDebug() << "[JsonTreeViewer]"

using namespace jtv::ui;

namespace {
constexpr auto g_ctrlbar_btn_sz      = 30;
constexpr auto g_ctrlbar_btn_icon_sz = 24;
}  // namespace

JsonTreeViewer::JsonTreeViewer(QWidget* parent) : ViewerBase(parent)
{
    qprintt << this;
}

JsonTreeViewer::~JsonTreeViewer()
{
    qprintt << "~" << this;
    cancelSearch();
}

void JsonTreeViewer::initTopWnd()
{
    m_top.wnd_bg = new QWidget(this);
    m_top.wnd_bg->setObjectName("topBar");
    // Initial style set in updateTheme() which is called during setup

    QHBoxLayout* layout = new QHBoxLayout(m_top.wnd_bg);
    layout->setContentsMargins(12, 6, 12, 6);
    layout->setSpacing(10);

    m_top.input = new QLineEdit(this);
    m_top.input->setPlaceholderText(tr("Filter current view..."));
    m_top.input->setClearButtonEnabled(true);
    layout->addWidget(m_top.input, 1);

    // Global Search Toggle Icon
    const auto dpr         = options()->dpr();
    const QColor iconColor = qApp->palette().color(QPalette::PlaceholderText);

    m_top.action_global
        = new QAction(svgIcon(g_svg_filter, iconColor, 16, dpr), "", this);
    m_top.action_global->setCheckable(true);
    m_top.action_global->setToolTip(
        tr("Deep Search Mode (%1 to Toggle)")
            .arg(Config::ins().shortcutToggleMode().toString(
                QKeySequence::NativeText)));
    m_top.input->addAction(m_top.action_global, QLineEdit::TrailingPosition);

    connect(m_top.action_global, &QAction::toggled, this,
            [this, iconColor, dpr](bool checked) {
                auto activeColor = QColor("#0288D1");
                m_top.action_global->setIcon(
                    svgIcon(checked ? g_svg_globe : g_svg_filter,
                            checked ? activeColor : iconColor, 16, dpr));
                m_top.input->setPlaceholderText(
                    checked ? tr("Deep search entire file (Enter)...")
                            : tr("Filter current view..."));

                // Reset state on toggle
                m_top.input->clear();
                if (checked) {
                    // Mode: Filter -> Search
                    if (m_search_panel)
                        m_search_panel->hide();
                    // Force reset filter to show all items
                    auto* proxy
                        = qobject_cast<TreeFilterProxyModel*>(m_view->model());
                    if (proxy)
                        proxy->updateFilter("");
                }
                else {
                    // Mode: Search -> Filter
                    cancelSearch();
                    if (m_search_panel) {
                        m_search_panel->clear();
                        m_search_panel->hide();
                    }
                }
                m_top.input->setFocus();
            });

    connect(m_top.input, &QLineEdit::returnPressed, this, [this]() {
        if (m_top.action_global->isChecked()) {
            startSearch();
        }
    });
}

void JsonTreeViewer::initBtmWnd()
{
    m_btm.wnd_bg              = new QWidget(this);
    QHBoxLayout* statusLayout = new QHBoxLayout(m_btm.wnd_bg);

    // Left: Breadcrumbs container
    m_btm.breadcrumbs_wnd = new QWidget(this);
    m_btm.breadcrumbs_lay = new QHBoxLayout(m_btm.breadcrumbs_wnd);
    m_btm.breadcrumbs_lay->setSpacing(0);
    statusLayout->addWidget(m_btm.breadcrumbs_wnd, 0);

    // Separator between breadcrumbs and value
    m_btm.value_label = new QLabel(this);
    m_btm.value_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_btm.value_label->setStyleSheet(g_qss_value_label);
    statusLayout->addWidget(m_btm.value_label, 1);

    // Center: node statistics
    m_btm.stats = new QLabel(this);
    m_btm.stats->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    statusLayout->addWidget(m_btm.stats, 0);

    // Right: info icon (Hover only)
    m_btm.info = new QLabel(this);
    m_btm.info->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_btm.info->setCursor(Qt::ArrowCursor);
    // Tooltip shows on hover
    m_btm.info->setToolTip(tr("JSON File Information"));
    statusLayout->addWidget(m_btm.info, 0);
}

QSize JsonTreeViewer::getContentSize() const
{
    const auto sz_def = options()->dpr() * QSize{600, 800};
    auto cmd
        = options()->property(ViewOptionsKeys::kKeyPluginCmd).toStringList();
    if (!cmd.isEmpty()) {
        auto parsed = seer::parseViewerSizeFromConfig(cmd);
        qprintt << "getContentSize: parsed" << parsed << cmd;
        if (parsed.isValid()) {
            return parsed;
        }
    }
    return sz_def;
}

void JsonTreeViewer::updateDPR(qreal r)
{
    layout()->setSpacing(6 * r);
    auto font = qApp->font();
    font.setPixelSize(12 * r);
    if (m_top.wnd_bg) {
        auto height_top = 30 * r;
        m_top.wnd_bg->setFixedHeight(height_top);
        m_top.wnd_bg->layout()->setContentsMargins(9 * r, 0, 9 * r, 0);
        m_top.wnd_bg->layout()->setSpacing(0);
        m_top.input->setFont(font);
        m_top.input->setFixedHeight(height_top);
    }
    if (m_search_panel) {
        m_search_panel->updateDPR(r);
    }
    if (m_btm.wnd_bg) {
        // Slightly taller for breadcrumbs
        m_btm.wnd_bg->setFixedHeight(24 * r);
        m_btm.wnd_bg->layout()->setContentsMargins(12 * r, 0, 12 * r, 0);
        m_btm.wnd_bg->layout()->setSpacing(12);
        m_btm.breadcrumbs_lay->setContentsMargins(8 * r, 0, 0, 0);
        // Status bar labels
        auto sbFont = qApp->font();
        sbFont.setPixelSize(11 * r);
        if (m_btm.value_label) {
            m_btm.value_label->setFont(sbFont);
        }
        if (m_btm.stats) {
            m_btm.stats->setFont(sbFont);
        }
    }
    if (m_progress_bar) {
        m_progress_bar->setFixedHeight(2 * r);
    }
    if (m_btm.info) {
        auto infoFont = qApp->font();
        infoFont.setPixelSize(13 * r);
        infoFont.setBold(true);
        m_btm.info->setFont(infoFont);
    }

    m_view->upadteDPR(r);

    if (m_btn_text_view) {
        m_btn_text_view->setFixedSize(g_ctrlbar_btn_sz * r,
                                      g_ctrlbar_btn_sz * r);
        m_btn_text_view->setIconSize(
            QSize(g_ctrlbar_btn_icon_sz * r, g_ctrlbar_btn_icon_sz * r));
    }

    if (m_model) {
        m_model->refreshDesign();
    }
}

void JsonTreeViewer::loadImpl(QBoxLayout* lay_content, QHBoxLayout* lay_ctrlbar)
{
    lay_content->setSpacing(0);

    initTopWnd();
    lay_content->addWidget(m_top.wnd_bg);
    const auto& cfg = Config::ins();
    if (!cfg.showFilterBar()) {
        m_top.wnd_bg->hide();
    }

    m_model          = new JsonTreeModel(this);
    auto proxy_model = new TreeFilterProxyModel(this);
    proxy_model->setSourceModel(m_model);
    QTimer* timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(m_top.input, &QLineEdit::textChanged, this,
            [timer, this](const QString&) {
                if (!m_top.action_global->isChecked()) {
                    timer->start(300);
                }
            });
    connect(timer, &QTimer::timeout, proxy_model, [proxy_model, this] {
        if (m_top.action_global->isChecked()) {
            qprint_err;
            return;  // Don't apply filter in search mode
        }
        proxy_model->updateFilter(m_top.input->text());
    });

    m_view = new JsonTreeView(this);
    m_view->setCopyActions(m_model->supportedActions());
    // Default to Small, will be updated after load
    m_view->setFileMode(FileMode::Small);
    m_view->setModel(proxy_model);
    lay_content->addWidget(m_view);

    m_search_panel = new SearchPanel(this);
    m_search_panel->hide();
    lay_content->addWidget(m_search_panel);
    connect(m_search_panel, &SearchPanel::targetResolved, this,
            [this](const QModelIndex& index) {
                if (!index.isValid()) {
                    return;
                }
                QModelIndex proxyIndex = index;
                if (auto proxy
                    = qobject_cast<QSortFilterProxyModel*>(m_view->model()))
                    proxyIndex = proxy->mapFromSource(index);

                if (proxyIndex.isValid()) {
                    m_view->setCurrentIndex(proxyIndex);
                    m_view->scrollTo(proxyIndex,
                                     QAbstractItemView::PositionAtCenter);
                    m_view->selectionModel()->select(
                        proxyIndex, QItemSelectionModel::ClearAndSelect
                                        | QItemSelectionModel::Rows);
                }
            });
    connect(m_search_panel, &SearchPanel::navigationFailed, this,
            [this](const QString& msg) {
                emit sigCommand(ViewCommandType::VCT_ShowToastMsg,
                                tr("Navigation Failed: %1").arg(msg));
            });
    connect(m_search_panel, &SearchPanel::cancelRequested, this,
            &JsonTreeViewer::cancelSearch);

    // Subtle progress bar
    m_progress_bar = new QProgressBar(this);
    m_progress_bar->setRange(0, 0);  // Indeterminate
    m_progress_bar->setTextVisible(false);
    m_progress_bar->setFixedHeight(2);
    m_progress_bar->setStyleSheet(g_qss_progress_bar);
    m_progress_bar->hide();
    lay_content->addWidget(m_progress_bar);

    // Status bar with three sections
    initBtmWnd();
    lay_content->addWidget(m_btm.wnd_bg);
    if (!cfg.showStatusBar()) {
        m_btm.wnd_bg->hide();
    }

    // Set up Shortcuts
    auto* actionCollapseAll = new QAction(this);
    actionCollapseAll->setShortcut(cfg.shortcutCollapseAll());
    connect(actionCollapseAll, &QAction::triggered, m_view,
            &JsonTreeView::collapseAll);
    addAction(actionCollapseAll);

    auto* actionExpandAll = new QAction(this);
    actionExpandAll->setShortcut(cfg.shortcutExpandAll());
    connect(actionExpandAll, &QAction::triggered, m_view,
            &JsonTreeView::expandAll);
    addAction(actionExpandAll);

    auto* actionCopyPath = new QAction(this);
    actionCopyPath->setShortcut(cfg.shortcutCopyPath());
    connect(actionCopyPath, &QAction::triggered, this, [this]() {
        QModelIndex current = m_view->currentIndex();
        if (current.isValid()) {
            emit m_view->copyPathRequested(current);
        }
    });
    addAction(actionCopyPath);

    auto* actionExportSelection = new QAction(this);
    actionExportSelection->setShortcut(cfg.shortcutExportSelection());
    connect(actionExportSelection, &QAction::triggered, this, [this]() {
        QModelIndex current = m_view->currentIndex();
        if (current.isValid()) {
            emit m_view->exportSelectionRequested(current);
        }
    });
    addAction(actionExportSelection);

    // Search/Filter Shortcut (Ctrl+F)
    auto* actionSearch = new QAction(this);
    actionSearch->setShortcut(cfg.shortcutFilter());
    actionSearch->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(actionSearch, &QAction::triggered, this, [this]() {
        m_top.input->setFocus();
        m_top.input->selectAll();
    });
    addAction(actionSearch);

    // Toggle filter/search mode
    auto* actionToggleMode = new QAction(this);
    actionToggleMode->setShortcut(cfg.shortcutToggleMode());
    actionToggleMode->setShortcutContext(Qt::WidgetWithChildrenShortcut);
    connect(actionToggleMode, &QAction::triggered, this, [this]() {
        if (m_top.input->hasFocus()) {
            m_top.action_global->toggle();
        }
    });
    addAction(actionToggleMode);

    auto* actionSearchNext = new QAction(this);
    actionSearchNext->setShortcut(QKeySequence::FindNext);
    connect(actionSearchNext, &QAction::triggered, this,
            &JsonTreeViewer::startSearch);
    addAction(actionSearchNext);

    connect(
        m_view->selectionModel(), &QItemSelectionModel::currentChanged, this,
        [this, proxy_model](const QModelIndex& current, const QModelIndex&) {
            // Clear existing breadcrumbs
            QLayoutItem* item;
            while ((item = m_btm.breadcrumbs_lay->takeAt(0)) != nullptr) {
                if (item->widget())
                    item->widget()->deleteLater();
                delete item;
            }
            m_btm.value_label->clear();

            if (!current.isValid())
                return;

            qreal dpr = options()->dpr();
            QColor separatorColor
                = qApp->palette().color(QPalette::PlaceholderText);
            QIcon sepIcon
                = svgIcon(g_svg_chevron_right, separatorColor, 16, dpr);

            // Collect parents for breadcrumbs
            QList<QModelIndex> hierarchy;
            QModelIndex idx = current;
            while (idx.isValid()) {
                hierarchy.prepend(idx);
                idx = idx.parent();
            }

            // Create breadcrumb segments
            for (int i = 0; i < hierarchy.size(); ++i) {
                const QModelIndex& hIdx = hierarchy[i];

                // Add separator before segment (except the first one)
                if (i > 0) {
                    QLabel* sep = new QLabel(this);
                    sep->setPixmap(sepIcon.pixmap(16 * dpr, 16 * dpr));
                    m_btm.breadcrumbs_lay->addWidget(sep);
                }

                QPushButton* btn = new QPushButton(
                    m_model->getKey(proxy_model->mapToSource(hIdx)), this);
                btn->setFlat(true);
                btn->setCursor(Qt::PointingHandCursor);
                // Premium feel: subtle hover color via stylesheet
                bool isDark
                    = qApp->palette().color(QPalette::Window).lightness() < 128;
                btn->setStyleSheet(
                    QString(g_qss_breadcrumb_btn)
                        .arg(qApp->palette().color(QPalette::WindowText).name())
                        .arg(isDark ? "rgba(255, 255, 255, 30)"
                                    : "rgba(0, 0, 0, 30)"));

                connect(btn, &QPushButton::clicked, this, [this, hIdx]() {
                    m_view->selectionModel()->setCurrentIndex(
                        hIdx, QItemSelectionModel::ClearAndSelect
                                  | QItemSelectionModel::Rows);
                    m_view->scrollTo(hIdx);
                });

                m_btm.breadcrumbs_lay->addWidget(btn);
            }

            // Add value if present
            QModelIndex src = proxy_model->mapToSource(current);
            QString val     = m_model->getValue(src);
            if (!val.isEmpty()) {
                constexpr int MAX_VAL = 60;
                if (val.length() > MAX_VAL)
                    val = val.left(MAX_VAL) + "...";
                m_btm.value_label->setText(" =  " + val);
            }
        });

    // Connect copy signals
    connect(m_view, &JsonTreeView::copyKeyRequested, this,
            [this](const QModelIndex& proxyIndex) {
                auto* proxy
                    = qobject_cast<TreeFilterProxyModel*>(m_view->model());
                if (!proxy)
                    return;
                QString key = m_model->getKey(proxy->mapToSource(proxyIndex));
                if (!key.isEmpty())
                    QApplication::clipboard()->setText(key);
            });

    connect(
        m_view, &JsonTreeView::copyValueRequested, this,
        [this](const QModelIndex& proxyIndex) {
            auto* proxy = qobject_cast<TreeFilterProxyModel*>(m_view->model());
            if (!proxy)
                return;
            QString value = m_model->getValue(proxy->mapToSource(proxyIndex));
            if (!value.isEmpty())
                QApplication::clipboard()->setText(value);
        });

    connect(m_view, &JsonTreeView::copyPathRequested, this,
            [this](const QModelIndex& proxyIndex) {
                auto* proxy
                    = qobject_cast<TreeFilterProxyModel*>(m_view->model());
                if (!proxy)
                    return;
                QString path = m_model->getPath(proxy->mapToSource(proxyIndex));
                if (!path.isEmpty())
                    QApplication::clipboard()->setText(path);
            });

    connect(m_view, &JsonTreeView::copyDotPathRequested, this,
            [this](const QModelIndex& proxyIndex) {
                auto* proxy
                    = qobject_cast<TreeFilterProxyModel*>(m_view->model());
                if (!proxy)
                    return;
                QString dotPath
                    = m_model->getDotPath(proxy->mapToSource(proxyIndex));
                if (!dotPath.isEmpty())
                    QApplication::clipboard()->setText(dotPath);
            });

    connect(
        m_view, &JsonTreeView::copySubtreeRequested, this,
        [this](const QModelIndex& proxyIndex) {
            auto* proxy = qobject_cast<TreeFilterProxyModel*>(m_view->model());
            if (!proxy)
                return;
            bool success = false;
            QString errorMsg;
            QString subtree = m_model->getSubtree(
                proxy->mapToSource(proxyIndex), &success, &errorMsg);
            if (success) {
                QApplication::clipboard()->setText(subtree);
            }
            else {
                emit sigCommand(ViewCommandType::VCT_ShowToastMsg,
                                tr("Copy Subtree Failed") + "\n" + errorMsg);
            }
        });

    connect(
        m_view, &JsonTreeView::copyKeyValueRequested, this,
        [this](const auto& proxyIndex) {
            auto* proxy = qobject_cast<TreeFilterProxyModel*>(m_view->model());
            if (!proxy)
                return;
            bool success = false;
            QString errorMsg;
            QString kv = m_model->getKeyValue(proxy->mapToSource(proxyIndex),
                                              &success, &errorMsg);
            if (success) {
                QApplication::clipboard()->setText(kv);
            }
            else {
                emit sigCommand(ViewCommandType::VCT_ShowToastMsg,
                                tr("Copy Key:Value Failed") + "\n" + errorMsg);
            }
        });

    connect(m_view, &JsonTreeView::exportSelectionRequested, this,
            [this](const QModelIndex& proxyIndex) {
                auto* proxy
                    = qobject_cast<TreeFilterProxyModel*>(m_view->model());
                if (!proxy)
                    return;

                // Get the source model index and item
                QModelIndex srcIndex = proxy->mapToSource(proxyIndex);
                JsonTreeItem* item   = m_model->getItem(srcIndex);
                if (!item) {
                    emit sigCommand(
                        ViewCommandType::VCT_ShowToastMsg,
                        tr("Export Failed") + "\n" + tr("Invalid item"));
                    return;
                }

                // Check size limit BEFORE extracting to avoid wasting time
                // item->byte_length contains the total size of this node and
                // all its children in the original JSON file
                constexpr quint64 MAX_SIZE = 10 * 1024 * 1024;  // 10 MB limit
                if (item->byte_length > MAX_SIZE) {
                    emit sigCommand(
                        ViewCommandType::VCT_ShowToastMsg,
                        tr("Export Failed") + "\n"
                            + tr("Subtree too large (%1 MB). Maximum allowed "
                                 "is 10 MB.")
                                  .arg(item->byte_length / (1024.0 * 1024.0), 0,
                                       'f', 2));
                    return;
                }

                // Extract the subtree as JSON string
                bool success = false;
                QString errorMsg;
                QString subtree
                    = m_model->getSubtree(srcIndex, &success, &errorMsg);
                if (!success) {
                    emit sigCommand(ViewCommandType::VCT_ShowToastMsg,
                                    tr("Export Failed") + "\n" + errorMsg);
                    return;
                }

                // Generate filename with timestamp:
                // Seer-JsonTreeViewer-YYYYMMDD-HHMMSS.json
                QString timestamp
                    = QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss");
                QString filename
                    = QString("Seer-JsonTreeViewer-%1.json").arg(timestamp);

                // Get system downloads folder (fallback to home if not
                // available)
                QString downloadsPath = QStandardPaths::writableLocation(
                    QStandardPaths::DownloadLocation);
                if (downloadsPath.isEmpty()) {
                    downloadsPath = QStandardPaths::writableLocation(
                        QStandardPaths::HomeLocation);
                }

                QString filePath = downloadsPath + "/" + filename;

                // Write JSON to file
                QFile file(filePath);
                if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                    emit sigCommand(
                        ViewCommandType::VCT_ShowToastMsg,
                        tr("Export Failed") + "\n"
                            + tr("Cannot write to file: %1").arg(filePath));
                    return;
                }

                file.write(subtree.toUtf8());
                file.close();

                // Show success message with file path
                emit sigCommand(ViewCommandType::VCT_ShowToastMsg,
                                tr("Exported to:\n%1").arg(filePath));
            });

    connect(m_view, &JsonTreeView::collapseAllRequested, this,
            [this]() { m_view->collapseAll(); });

    connect(m_view, &JsonTreeView::expandAllRequested, this,
            [this]() { m_view->expandAll(); });

    connect(m_view, &JsonTreeView::openUrlRequested, this,
            [this](const QString& url) {
                if (!QDesktopServices::openUrl(QUrl(url))) {
                    emit sigCommand(ViewCommandType::VCT_ShowToastMsg,
                                    tr("Failed to open URL:\n%1").arg(url));
                }
            });

    connect(m_view, &JsonTreeView::copyTimestampAsIso8601Requested, this,
            [this](const QString& value) {
                bool ok;
                qint64 num = value.toLongLong(&ok);
                if (!ok) {
                    emit sigCommand(ViewCommandType::VCT_ShowToastMsg,
                                    tr("Invalid timestamp"));
                    return;
                }

                QDateTime dt;
                if (value.length() == 10) {
                    dt = QDateTime::fromSecsSinceEpoch(num, Qt::UTC);
                }
                else if (value.length() == 13) {
                    dt = QDateTime::fromMSecsSinceEpoch(num, Qt::UTC);
                }
                else {
                    emit sigCommand(ViewCommandType::VCT_ShowToastMsg,
                                    tr("Invalid timestamp"));
                    return;
                }

                QString iso8601 = dt.toString(Qt::ISODate);
                QApplication::clipboard()->setText(iso8601);
            });

    if (lay_ctrlbar) {
        lay_ctrlbar->addStretch();
        m_btn_text_view = new QPushButton(this);
        m_btn_text_view->setFlat(true);
        m_btn_text_view->setToolTip("Text View");
        m_btn_text_view->setFocusPolicy(Qt::NoFocus);
        lay_ctrlbar->addWidget(m_btn_text_view);
        connect(m_btn_text_view, &QPushButton::clicked, this,
                &JsonTreeViewer::onTextViewBtnClicked);
    }

    updateDPR(options()->dpr());
    updateTheme(options()->theme());

    // Start background loading
    startBackgroundLoad(m_model, options()->path());
    // VCV_Loaded will be emitted by startBackgroundLoad after loading completes
}

void JsonTreeViewer::onTextViewBtnClicked()
{
    // built-in viewer
    emit sigCommand(ViewCommandType::VCT_LoadViewerWithNewType,
                    QString("Text"));
}

void JsonTreeViewer::startBackgroundLoad(JsonTreeModel* model,
                                         const QString& path)
{
    qprintt << "=== [BG LOAD START] ===" << path;

    if (m_btm.breadcrumbs_lay) {
        QLayoutItem* item;
        while ((item = m_btm.breadcrumbs_lay->takeAt(0)) != nullptr) {
            if (item->widget())
                item->widget()->deleteLater();
            delete item;
        }
    }
    if (m_btm.value_label) {
        m_btm.value_label->setText(tr("Loading..."));
    }

    connect(
        model, &JsonTreeModel::loadFinished, this,
        [this, model, path](bool success, qint64 elapsedMs) {
            qprintt << "[BG LOAD] loadFinished received";

            // Back in main thread for UI updates
            if (!success) {
                if (m_btm.value_label) {
                    m_btm.value_label->setText(tr("Failed to load JSON file"));
                }
                emit sigCommand(ViewCommandType::VCT_StateChange, VCV_Error);
                qprintt << "=== [BG LOAD END] Failed ===";
                return;
            }

            m_view->setCopyActions(model->supportedActions());
            m_view->setFileMode(model->fileMode());

            // Check for parse errors
            const auto* metrics = model->metrics();
            if (metrics && !metrics->parseError.isEmpty()) {
                // Update status bar
                if (m_btm.value_label) {
                    m_btm.value_label->setText(tr("⚠️ JSON Parse Error"));
                }

                // Expand the root error node to show details
                QTimer::singleShot(0, this, [this]() {
                    auto* proxy
                        = qobject_cast<TreeFilterProxyModel*>(m_view->model());
                    if (proxy) {
                        QModelIndex rootIndex = proxy->index(0, 0);
                        if (rootIndex.isValid()) {
                            m_view->expand(rootIndex);
                        }
                    }
                });

                // Return success to prevent Seer from switching to text viewer
                emit sigCommand(ViewCommandType::VCT_StateChange, VCV_Loaded);
                return;
            }

            qprintt << "[BG LOAD] Adding info icon...";

            // Add info icon for all files
            if (m_btm.info) {
                qreal dpr = options()->dpr();
                m_btm.info->setPixmap(
                    svgIcon(g_svg_info, QColor("#2196F3"), 18, dpr)
                        .pixmap(18 * dpr, 18 * dpr));
                m_btm.info->setAlignment(Qt::AlignCenter);

                QStringList tooltipLines;

                // Performance metrics
                tooltipLines << tr("=== Performance Metrics ===");
                tooltipLines
                    << tr("Load time: %1s").arg(elapsedMs / 1000.0, 0, 'f', 3);

                // File information
                QFileInfo fileInfo(path);
                qint64 fileSize = fileInfo.size();
                QString fileSizeStr;
                if (fileSize < 1024) {
                    fileSizeStr = tr("%1 B").arg(fileSize);
                }
                else if (fileSize < 1024 * 1024) {
                    fileSizeStr = tr("%1 KB").arg(fileSize / 1024.0, 0, 'f', 2);
                }
                else if (fileSize < 1024 * 1024 * 1024) {
                    fileSizeStr = tr("%1 MB").arg(fileSize / 1024.0 / 1024.0, 0,
                                                  'f', 2);
                }
                else {
                    fileSizeStr = tr("%1 GB").arg(
                        fileSize / 1024.0 / 1024.0 / 1024.0, 0, 'f', 2);
                }
                tooltipLines << tr("File size: %1").arg(fileSizeStr);

                // Strategy information
                using FM = FileMode;
                QString strategyName;
                bool usesMmap = false;
                switch (model->fileMode()) {
                case FM::Small:
                    strategyName = "Small";
                    break;
                case FM::Medium:
                    strategyName = "Medium";
                    break;
                case FM::Large:
                    strategyName = "Large";
                    usesMmap     = true;
                    break;
                case FM::Extreme:
                    strategyName = "Extreme";
                    usesMmap     = true;
                    break;
                }
                tooltipLines << tr("Strategy: %1").arg(strategyName);
                if (usesMmap) {
                    tooltipLines << tr("Memory mapping: Enabled");
                }

                // Add file mode specific limitations
                tooltipLines << "";
                tooltipLines << tr("=== Feature Availability ===");
                if (model->fileMode() == FM::Extreme) {
                    tooltipLines << tr("✓ Copy Key/Value");
                    tooltipLines
                        << tr("✗ Copy Path (disabled for extreme files)");
                    tooltipLines
                        << tr("✗ Copy Subtree (disabled for extreme files)");
                }
                else if (model->fileMode() == FM::Large) {
                    tooltipLines << tr("✓ Copy Key/Value/Path");
                    tooltipLines
                        << tr("✗ Copy Subtree (disabled for large files)");
                }
                else {
                    tooltipLines << tr("✓ All copy operations available");
                }

                m_btm.info->setToolTip(tooltipLines.join("\n"));
            }

            qprintt << "[BG LOAD] loadFinished processing complete";

            // Proactively trigger first fetch if root has children
            QModelIndex rootIndex;
            if (model->hasChildren(rootIndex)) {
                qprintt << "[BG LOAD] Triggering first fetch";
                model->fetchMore(rootIndex);
            }
            else {
                // No children, can show UI immediately
                if (m_btm.value_label) {
                    m_btm.value_label->setText(tr("Ready"));
                }
                qprintt << "=== [BG LOAD END] Success (no children) ===";
                emit sigCommand(ViewCommandType::VCT_StateChange, VCV_Loaded);
            }
        },
        Qt::SingleShotConnection);

    connect(
        model, &JsonTreeModel::firstFetchCompleted, this,
        [this](qint64) {
            qprintt << "[BG LOAD] firstFetchCompleted received";

            if (m_btm.value_label) {
                m_btm.value_label->setText(tr("Ready"));
            }

            qprintt << "=== [BG LOAD END] Success ===";
            emit sigCommand(ViewCommandType::VCT_StateChange, VCV_Loaded);
        },
        Qt::SingleShotConnection);

    // Connect fetch queue status updates
    connect(model, &JsonTreeModel::fetchQueueChanged, this,
            [this](int queueSize, bool inProgress) {
                if (m_progress_bar) {
                    m_progress_bar->setVisible(queueSize > 0 || inProgress);
                }

                if (!m_btm.value_label) {
                    return;
                }

                if (queueSize > 0 || inProgress) {
                    m_btm.value_label->setText(
                        tr("Fetching [%1]...").arg(queueSize));
                }
                else {
                    m_btm.value_label->setText(tr("Ready"));
                }
            });

    // Start loading (model handles background thread internally)
    qprintt << "[BG LOAD] Starting model load...";
    model->load(path);
}

QString JsonTreeViewer::formatFileSize(qint64 bytes) const
{
    if (bytes < 1024) {
        return QString::number(bytes) + " B";
    }
    else if (bytes < 1024 * 1024) {
        return QString::number(bytes / 1024.0, 'f', 1) + " KB";
    }
    else if (bytes < 1024 * 1024 * 1024) {
        return QString::number(bytes / (1024.0 * 1024.0), 'f', 2) + " MB";
    }
    else {
        return QString::number(bytes / (1024.0 * 1024.0 * 1024.0), 'f', 2)
               + " GB";
    }
}

void JsonTreeViewer::updateStatusBarStats(JsonTreeModel* model)
{
    if (!m_btm.stats || !model) {
        return;
    }

    // For now, just show a placeholder
    // TODO: Implement node counting and depth calculation
    m_btm.stats->setText("");
}

void JsonTreeViewer::updateTheme(int theme)
{
    const auto& cfg = Config::ins();
    bool isDark     = true;
    if (cfg.themeMode() == "light") {
        isDark = false;
    }
    else if (cfg.themeMode() == "dark") {
        isDark = true;
    }
    else {
        // Auto - 0 is light, 1 is dark in Seer
        isDark = (theme == 1);
    }

    using namespace jtv::ui::Colors;

    // 1. Update Top Bar
    if (m_top.wnd_bg) {
        QString qss = QString(g_qss_top_bar)
                          .arg(isDark ? DarkSurface : LightSurface)
                          .arg(isDark ? DarkBorder : LightBorder)
                          .arg(isDark ? DarkInput : LightInput)
                          .arg(isDark ? DarkText : LightText)
                          .arg(Accent);
        m_top.wnd_bg->setStyleSheet(qss);
    }

    // 2. Update Icons
    const auto dpr   = options()->dpr();
    QColor textColor = QColor(isDark ? DarkText : LightText);
    // QColor dimColor = QColor(isDark ? DarkTextDim : LightTextDim);

    if (m_btn_text_view) {
        m_btn_text_view->setIcon(
            svgIcon(g_svg_article, textColor, g_ctrlbar_btn_icon_sz, dpr));
    }

    if (m_top.action_global) {
        // Update filter icon color
        m_top.action_global->setIcon(svgIcon(
            m_top.action_global->isChecked() ? g_svg_globe : g_svg_filter,
            m_top.action_global->isChecked() ? QColor(Accent) : textColor, 16,
            dpr));
    }

    // 3. Update Progress Bar
    if (m_progress_bar) {
        m_progress_bar->setStyleSheet(QString(g_qss_progress_bar).arg(Accent));
    }

    // 4. Update Bottom Value Label
    if (m_btm.value_label) {
        m_btm.value_label->setStyleSheet(
            QString(g_qss_value_label)
                .arg(isDark ? DarkTextDim : LightTextDim));
    }

    // 5. Update Search Panel
    if (m_search_panel) {
        m_search_panel->updateTheme(isDark);
    }

    if (m_model) {
        m_model->refreshDesign();
    }
}

void JsonTreeViewer::startSearch()
{
    QString text = m_top.input->text();
    if (text.isEmpty())
        return;

    auto strategy = m_model->strategy();
    if (!strategy)
        return;

    SearchQuery query;
    query.text          = text;
    query.type          = SearchType::All;
    query.caseSensitive = false;

    m_search_panel->startSearch(m_model, strategy, query);
}

void JsonTreeViewer::cancelSearch()
{
    if (m_search_panel)
        m_search_panel->cancelSearch();
}
