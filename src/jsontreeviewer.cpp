#include "jsontreeviewer.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QDesktopServices>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QStandardPaths>
#include <QSvgRenderer>
#include <QTimer>
#include <QUrl>

#include "jsonnode.h"
#include "jsontreemodel.h"
#include "jsontreeview.h"
#include "loadworker.h"
#include "seer/viewerhelper.h"

#define qprintt qDebug() << "[JsonTreeViewer]"

namespace {
constexpr auto g_ctrlbar_btn_sz      = 30;
constexpr auto g_ctrlbar_btn_icon_sz = 24;

// Material Symbol: "Article" Rounded, Outline, Weight 300
constexpr auto g_svg_article = R"SVG(
<svg xmlns="http://www.w3.org/2000/svg" height="24" viewBox="0 -960 960 960" width="24">
  <path fill="currentColor" d="M312-300h336v-44H312v44Zm0-160h336v-44H312v44Zm0-160h336v-44H312v44ZM228-156q-29.7 0-50.85-21.15Q156-198.3 156-228v-504q0-29.7 21.15-50.85Q198.3-804 228-804h504q29.7 0 50.85 21.15Q804-761.7 804-732v504q0 29.7-21.15 50.85Q761.7-156 732-156H228Zm0-72h504v-504H228v504Zm0 0v-504 504Z"/>
</svg>)SVG";

QIcon svgIcon(const char* svg_data, const QColor& color, int icon_sz, qreal dpr)
{
    QByteArray data(svg_data);
    data.replace("currentColor", color.name(QColor::HexRgb).toUtf8());

    QSvgRenderer renderer(data);
    if (!renderer.isValid()) {
        return {};
    }

    const int phys = qRound(icon_sz * dpr);
    QPixmap pix(phys, phys);
    pix.setDevicePixelRatio(dpr);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    renderer.render(&p, QRectF(0, 0, icon_sz, icon_sz));
    return QIcon(pix);
}
}  // namespace

JsonTreeViewer::JsonTreeViewer(QWidget* parent)
    : ViewerBase(parent), m_btn_text_view(nullptr), m_view(nullptr)
{
    qprintt << this;
}

JsonTreeViewer::~JsonTreeViewer()
{
    qprintt << "~" << this;
}

void JsonTreeViewer::initTopWnd()
{
    m_top.wnd_bg           = new QWidget(this);
    QHBoxLayout* layout_bg = new QHBoxLayout;
    m_top.wnd_bg->setLayout(layout_bg);
    m_top.filter = new QLineEdit(this);
    layout_bg->addWidget(m_top.filter);

    m_top.filter->setPlaceholderText("Filter...");
    m_top.filter->setClearButtonEnabled(true);
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
        auto m = 9 * r;
        m_top.wnd_bg->layout()->setContentsMargins(m, 0, m, 0);
        m_top.wnd_bg->layout()->setSpacing(0);
        m_top.filter->setFont(font);
        m_top.filter->setFixedHeight(height_top);
    }
    m_view->upadteDPR(r);

    // Status bar labels
    auto sbFont = qApp->font();
    sbFont.setPixelSize(11 * r);
    if (m_statusbar.path_value) {
        m_statusbar.path_value->setFont(sbFont);
        if (auto parent
            = qobject_cast<QWidget*>(m_statusbar.path_value->parent())) {
            parent->setFixedHeight(20 * r);
            parent->layout()->setContentsMargins(6 * r, 0, 6 * r, 0);
        }
    }
    if (m_statusbar.stats) {
        m_statusbar.stats->setFont(sbFont);
    }
    if (m_statusbar.info) {
        auto infoFont = qApp->font();
        infoFont.setPixelSize(13 * r);
        infoFont.setBold(true);
        m_statusbar.info->setFont(infoFont);
    }

    if (m_btn_text_view) {
        m_btn_text_view->setFixedSize(g_ctrlbar_btn_sz * r,
                                      g_ctrlbar_btn_sz * r);
        m_btn_text_view->setIconSize(
            QSize(g_ctrlbar_btn_icon_sz * r, g_ctrlbar_btn_icon_sz * r));
    }
}

void JsonTreeViewer::loadImpl(QBoxLayout* lay_content, QHBoxLayout* lay_ctrlbar)
{
    initTopWnd();
    lay_content->setSpacing(0);
    lay_content->addWidget(m_top.wnd_bg);

    JsonTreeModel* m = new JsonTreeModel(this);

    auto proxy_model = new TreeFilterProxyModel(this);
    proxy_model->setSourceModel(m);
    QTimer* timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(m_top.filter, &QLineEdit::textChanged, this,
            [timer](const QString&) { timer->start(300); });
    connect(timer, &QTimer::timeout, proxy_model, [proxy_model, this] {
        proxy_model->updateFilter(m_top.filter->text());
    });

    m_view = new JsonTreeView(this);
    m_view->setCopyActions(m->supportedActions());
    m_view->setFileMode(
        FileMode::Small);  // Default to Small, will be updated after load
    m_view->setModel(proxy_model);
    lay_content->addWidget(m_view);

    // Status bar with three sections
    QWidget* statusBarWidget  = new QWidget(this);
    QHBoxLayout* statusLayout = new QHBoxLayout(statusBarWidget);
    statusLayout->setContentsMargins(0, 0, 0, 0);
    statusLayout->setSpacing(12);

    // Left: path and value
    m_statusbar.path_value = new QLabel(this);
    m_statusbar.path_value->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_statusbar.path_value->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statusLayout->addWidget(m_statusbar.path_value, 1);

    // Center: node statistics
    m_statusbar.stats = new QLabel(this);
    m_statusbar.stats->setAlignment(Qt::AlignCenter | Qt::AlignVCenter);
    statusLayout->addWidget(m_statusbar.stats, 0);

    // Right: info icon
    m_statusbar.info = new QLabel(this);
    m_statusbar.info->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    statusLayout->addWidget(m_statusbar.info, 0);

    lay_content->addWidget(statusBarWidget);

    connect(
        m_view->selectionModel(), &QItemSelectionModel::currentChanged, this,
        [this, m, proxy_model](const QModelIndex& current, const QModelIndex&) {
            if (!current.isValid()) {
                m_statusbar.path_value->clear();
                return;
            }
            QModelIndex src = proxy_model->mapToSource(current);
            QString path    = m->getPath(src);
            QString key     = m->getKey(src);
            QString val     = m->getValue(src);

            QString text = path.isEmpty() ? key : path;
            if (!val.isEmpty()) {
                constexpr int MAX_VAL = 80;
                if (val.length() > MAX_VAL)
                    val = val.left(MAX_VAL) + "...";
                text += "  =  " + val;
            }
            m_statusbar.path_value->setText(text);
        });

    // Connect copy signals
    connect(m_view, &JsonTreeView::copyKeyRequested, this,
            [this, m](const QModelIndex& proxyIndex) {
                auto* proxy
                    = qobject_cast<TreeFilterProxyModel*>(m_view->model());
                if (!proxy)
                    return;
                QString key = m->getKey(proxy->mapToSource(proxyIndex));
                if (!key.isEmpty())
                    QApplication::clipboard()->setText(key);
            });

    connect(m_view, &JsonTreeView::copyValueRequested, this,
            [this, m](const QModelIndex& proxyIndex) {
                auto* proxy
                    = qobject_cast<TreeFilterProxyModel*>(m_view->model());
                if (!proxy)
                    return;
                QString value = m->getValue(proxy->mapToSource(proxyIndex));
                if (!value.isEmpty())
                    QApplication::clipboard()->setText(value);
            });

    connect(m_view, &JsonTreeView::copyPathRequested, this,
            [this, m](const QModelIndex& proxyIndex) {
                auto* proxy
                    = qobject_cast<TreeFilterProxyModel*>(m_view->model());
                if (!proxy)
                    return;
                QString path = m->getPath(proxy->mapToSource(proxyIndex));
                if (!path.isEmpty())
                    QApplication::clipboard()->setText(path);
            });

    connect(m_view, &JsonTreeView::copyDotPathRequested, this,
            [this, m](const QModelIndex& proxyIndex) {
                auto* proxy
                    = qobject_cast<TreeFilterProxyModel*>(m_view->model());
                if (!proxy)
                    return;
                QString dotPath = m->getDotPath(proxy->mapToSource(proxyIndex));
                if (!dotPath.isEmpty())
                    QApplication::clipboard()->setText(dotPath);
            });

    connect(
        m_view, &JsonTreeView::copySubtreeRequested, this,
        [this, m](const QModelIndex& proxyIndex) {
            auto* proxy = qobject_cast<TreeFilterProxyModel*>(m_view->model());
            if (!proxy)
                return;
            bool success = false;
            QString errorMsg;
            QString subtree = m->getSubtree(proxy->mapToSource(proxyIndex),
                                            &success, &errorMsg);
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
        [this, m](const QModelIndex& proxyIndex) {
            auto* proxy = qobject_cast<TreeFilterProxyModel*>(m_view->model());
            if (!proxy)
                return;
            bool success = false;
            QString errorMsg;
            QString kv = m->getKeyValue(proxy->mapToSource(proxyIndex),
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
            [this, m](const QModelIndex& proxyIndex) {
                auto* proxy
                    = qobject_cast<TreeFilterProxyModel*>(m_view->model());
                if (!proxy)
                    return;

                // Get the source model index and item
                QModelIndex srcIndex = proxy->mapToSource(proxyIndex);
                JsonTreeItem* item   = m->getItem(srcIndex);
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
                QString subtree = m->getSubtree(srcIndex, &success, &errorMsg);
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
    startBackgroundLoad(m, options()->path());
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

    if (m_statusbar.path_value) {
        m_statusbar.path_value->setText(tr("Loading..."));
    }

    connect(
        model, &JsonTreeModel::loadFinished, this,
        [this, model, path](bool success, qint64 elapsedMs) {
            qprintt << "[BG LOAD] loadFinished received";

            // Record load time
            m_load_time_ms = elapsedMs;

            // Back in main thread for UI updates
            if (!success) {
                if (m_statusbar.path_value) {
                    m_statusbar.path_value->setText(
                        tr("Failed to load JSON file"));
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
                if (m_statusbar.path_value) {
                    m_statusbar.path_value->setText(tr("⚠️ JSON Parse Error"));
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
            if (m_statusbar.info) {
                m_statusbar.info->setText(" ℹ ");
                m_statusbar.info->setAlignment(Qt::AlignCenter);

                QStringList tooltipLines;

                // Performance metrics
                tooltipLines << tr("=== Performance Metrics ===");
                tooltipLines << tr("Load time: %1s")
                                    .arg(m_load_time_ms / 1000.0, 0, 'f', 3);

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

                m_statusbar.info->setToolTip(tooltipLines.join("\n"));
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
                if (m_statusbar.path_value) {
                    m_statusbar.path_value->setText(tr("Ready"));
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

            if (m_statusbar.path_value) {
                m_statusbar.path_value->setText(tr("Ready"));
            }

            qprintt << "=== [BG LOAD END] Success ===";
            emit sigCommand(ViewCommandType::VCT_StateChange, VCV_Loaded);
        },
        Qt::SingleShotConnection);

    // Connect fetch queue status updates
    connect(model, &JsonTreeModel::fetchQueueChanged, this,
            [this](int queueSize, bool inProgress) {
                if (!m_statusbar.path_value) {
                    return;
                }

                if (queueSize > 0 || inProgress) {
                    m_statusbar.path_value->setText(
                        tr("Fetching [%1]...").arg(queueSize));
                }
                else {
                    m_statusbar.path_value->setText(tr("Ready"));
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
    if (!m_statusbar.stats || !model) {
        return;
    }

    // For now, just show a placeholder
    // TODO: Implement node counting and depth calculation
    m_statusbar.stats->setText("");
}

void JsonTreeViewer::updateTheme(int theme)
{
    if (!m_btn_text_view) {
        return;
    }
    const auto sz  = m_btn_text_view->width();
    const auto dpr = sz * 1. / g_ctrlbar_btn_sz;
    m_btn_text_view->setIcon(
        svgIcon(g_svg_article, qApp->palette().color(QPalette::WindowText),
                dpr * g_ctrlbar_btn_icon_sz, dpr));
}
