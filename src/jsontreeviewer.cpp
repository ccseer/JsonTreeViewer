#include "jsontreeviewer.h"

#include <QApplication>
#include <QClipboard>
#include <QDateTime>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimer>

#include "jsontreemodel.h"
#include "jsontreeview.h"
#include "loadworker.h"
#include "seer/viewerhelper.h"

#define qprintt qDebug() << "[JsonTreeViewer]"

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
        m_btn_text_view->setFixedSize(
            m_btn_text_view->fontMetrics().horizontalAdvance(
                m_btn_text_view->text())
                + 8 * r * 2,
            30 * r);
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

    if (lay_ctrlbar) {
        lay_ctrlbar->addStretch();
        m_btn_text_view = new QPushButton(this);
        m_btn_text_view->setText("Text View");
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
        [this, model](bool success, qint64 elapsedMs) {
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

            qprintt << "[BG LOAD] Adding info icon...";

            // Add info icon for all files
            if (m_statusbar.info) {
                m_statusbar.info->setText(" ℹ ");
                m_statusbar.info->setAlignment(Qt::AlignCenter);

                QStringList tooltipLines;
                // First line: load time
                tooltipLines << tr("Loaded in %1s")
                                    .arg(m_load_time_ms / 1000.0, 0, 'f', 2);

                // Add file mode specific info
                using FM = JsonTreeModel::FileMode;
                if (model->fileMode() == FM::Extreme) {
                    tooltipLines
                        << "" << tr("Extreme file (>1 GB):")
                        << tr("• Only Key/Value copy supported")
                        << tr("• Path and Subtree operations disabled");
                }
                else if (model->fileMode() == FM::Large) {
                    tooltipLines << "" << tr("Large file (>100 MB):")
                                 << tr("• Path copy supported")
                                 << tr("• Subtree and Key:Value copy disabled");
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
                qprintt << "=== [BG LOAD END] Success (no children) ===";
                emit sigCommand(ViewCommandType::VCT_StateChange, VCV_Loaded);
            }
        },
        Qt::SingleShotConnection);

    connect(
        model, &JsonTreeModel::firstFetchCompleted, this,
        [this](qint64) {
            qprintt << "[BG LOAD] firstFetchCompleted received";

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
