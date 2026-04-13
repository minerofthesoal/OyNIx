#include "DownloadManager.h"
#include "theme/ThemeEngine.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>
#include <QWebEngineDownloadRequest>
#include <QPropertyAnimation>
#include <QGraphicsOpacityEffect>

// ── Constructor / Destructor ─────────────────────────────────────────
DownloadManager::DownloadManager(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
}

DownloadManager::~DownloadManager() = default;

// ── UI Setup ─────────────────────────────────────────────────────────
void DownloadManager::setupUi()
{
    const auto &c = ThemeEngine::instance().colors();

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Header
    auto *header = new QWidget(this);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(14, 10, 14, 10);
    auto *titleLabel = new QLabel(QStringLiteral("Downloads"), this);
    titleLabel->setStyleSheet(QStringLiteral("color: ") + c["purple-light"]
        + QStringLiteral("; font-size: 14px; font-weight: bold;"));
    headerLayout->addWidget(titleLabel);
    headerLayout->addStretch();

    m_clearAllBtn = new QPushButton(QStringLiteral("Clear Completed"), this);
    m_clearAllBtn->setStyleSheet(
        QStringLiteral("QPushButton { background: rgba(110,106,179,0.15); color: ")
        + c["purple-light"]
        + QStringLiteral("; border: 1px solid rgba(110,106,179,0.3); border-radius: 8px;"
                         " padding: 5px 14px; font-size: 11px; }"
                         "QPushButton:hover { background: rgba(110,106,179,0.3); }"));
    connect(m_clearAllBtn, &QPushButton::clicked, this, &DownloadManager::clearCompleted);
    headerLayout->addWidget(m_clearAllBtn);
    mainLayout->addWidget(header);

    // Scroll area for downloads
    auto *scrollArea = new QScrollArea(this);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setStyleSheet(
        QStringLiteral("QScrollArea { background: transparent; border: none; }"
                       "QScrollBar:vertical { background: transparent; width: 6px; }"
                       "QScrollBar::handle:vertical { background: ") + c["scrollbar"]
        + QStringLiteral("; border-radius: 3px; }"
                         "QScrollBar::handle:vertical:hover { background: ") + c["scrollbar-hover"]
        + QStringLiteral("; }"
                         "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"));

    auto *listWidget = new QWidget(this);
    m_listLayout = new QVBoxLayout(listWidget);
    m_listLayout->setContentsMargins(10, 4, 10, 4);
    m_listLayout->setSpacing(8);

    m_emptyLabel = new QLabel(QStringLiteral("No downloads yet"), listWidget);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setStyleSheet(QStringLiteral("color: ") + c["text-muted"]
        + QStringLiteral("; font-size: 13px; padding: 40px;"));
    m_listLayout->addWidget(m_emptyLabel);
    m_listLayout->addStretch();

    scrollArea->setWidget(listWidget);
    mainLayout->addWidget(scrollArea, 1);

    setStyleSheet(QStringLiteral("DownloadManager { background: ") + c["bg-darkest"]
        + QStringLiteral("; }"));
}

// ── Add download ─────────────────────────────────────────────────────
void DownloadManager::addDownload(QWebEngineDownloadRequest *download)
{
    if (!download) return;

    m_emptyLabel->hide();

    auto *widget = createDownloadWidget(download);

    DownloadItem item;
    item.widget = widget;
    item.filePath = download->downloadDirectory() + QLatin1Char('/') + download->downloadFileName();
    item.progressBar = widget->findChild<QProgressBar *>(QStringLiteral("dlProgress"));
    item.statusLabel = widget->findChild<QLabel *>(QStringLiteral("dlStatus"));
    item.nameLabel   = widget->findChild<QLabel *>(QStringLiteral("dlName"));
    item.cancelBtn   = widget->findChild<QPushButton *>(QStringLiteral("dlCancel"));
    item.openBtn     = widget->findChild<QPushButton *>(QStringLiteral("dlOpen"));

    m_items.append(item);
    const int idx = m_items.size() - 1;

    // Insert before the stretch
    m_listLayout->insertWidget(m_listLayout->count() - 1, widget);

    // Connect download signals
    connect(download, &QWebEngineDownloadRequest::receivedBytesChanged, this,
            [this, idx, download]() {
                if (idx >= m_items.size()) return;
                auto &it = m_items[idx];
                const qint64 received = download->receivedBytes();
                const qint64 total = download->totalBytes();
                if (it.progressBar) {
                    if (total > 0) {
                        it.progressBar->setMaximum(100);
                        it.progressBar->setValue(static_cast<int>(received * 100 / total));
                    } else {
                        it.progressBar->setMaximum(0); // indeterminate
                    }
                }
                if (it.statusLabel) {
                    it.statusLabel->setText(QStringLiteral("%1 / %2")
                        .arg(formatBytes(received), formatBytes(total)));
                }
            });

    connect(download, &QWebEngineDownloadRequest::isFinishedChanged, this,
            [this, idx, download]() {
                if (idx >= m_items.size()) return;
                auto &it = m_items[idx];
                it.completed = true;
                if (it.progressBar) it.progressBar->setValue(100);
                if (it.statusLabel) it.statusLabel->setText(QStringLiteral("Complete"));
                if (it.cancelBtn) it.cancelBtn->hide();
                if (it.openBtn) it.openBtn->show();
                updateCountBadge();
            });

    // Cancel button
    if (item.cancelBtn) {
        connect(item.cancelBtn, &QPushButton::clicked, this, [this, idx, download]() {
            download->cancel();
            if (idx < m_items.size()) {
                m_items[idx].cancelled = true;
                if (m_items[idx].statusLabel)
                    m_items[idx].statusLabel->setText(QStringLiteral("Cancelled"));
                if (m_items[idx].cancelBtn) m_items[idx].cancelBtn->hide();
            }
            updateCountBadge();
        });
    }

    // Open button
    if (item.openBtn) {
        item.openBtn->hide();
        connect(item.openBtn, &QPushButton::clicked, this, [this, idx]() {
            if (idx < m_items.size())
                QDesktopServices::openUrl(QUrl::fromLocalFile(m_items[idx].filePath));
        });
    }

    download->accept();
    updateCountBadge();

    // Slide-in animation
    auto *opacity = new QGraphicsOpacityEffect(widget);
    opacity->setOpacity(0.0);
    widget->setGraphicsEffect(opacity);
    auto *anim = new QPropertyAnimation(opacity, "opacity", widget);
    anim->setDuration(300);
    anim->setStartValue(0.0);
    anim->setEndValue(1.0);
    anim->setEasingCurve(QEasingCurve::OutCubic);
    anim->start(QAbstractAnimation::DeleteWhenStopped);
}

QWidget *DownloadManager::createDownloadWidget(QWebEngineDownloadRequest *download)
{
    const auto &c = ThemeEngine::instance().colors();

    auto *widget = new QWidget(this);
    widget->setStyleSheet(QStringLiteral("QWidget#dlCard { background: ") + c["bg-mid"]
        + QStringLiteral("; border: 1px solid ") + c["border"]
        + QStringLiteral("; border-radius: 12px; }"));
    widget->setObjectName(QStringLiteral("dlCard"));

    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(12, 10, 12, 10);
    layout->setSpacing(6);

    // Filename row
    auto *topRow = new QHBoxLayout;
    auto *nameLabel = new QLabel(download->downloadFileName(), widget);
    nameLabel->setObjectName(QStringLiteral("dlName"));
    nameLabel->setStyleSheet(QStringLiteral("color: ") + c["text-primary"]
        + QStringLiteral("; font-size: 13px; font-weight: bold;"));
    nameLabel->setMaximumWidth(280);
    topRow->addWidget(nameLabel, 1);

    auto *cancelBtn = new QPushButton(QStringLiteral("X"), widget);
    cancelBtn->setObjectName(QStringLiteral("dlCancel"));
    cancelBtn->setFixedSize(26, 26);
    cancelBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: rgba(212,86,94,0.2); color: ") + c["error"]
        + QStringLiteral("; border-radius: 13px; font-size: 12px; border: none; }"
                         "QPushButton:hover { background: rgba(212,86,94,0.4); }"));
    topRow->addWidget(cancelBtn);

    auto *openBtn = new QPushButton(QStringLiteral("Open"), widget);
    openBtn->setObjectName(QStringLiteral("dlOpen"));
    openBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: rgba(115,201,145,0.15); color: ") + c["success"]
        + QStringLiteral("; border-radius: 6px; padding: 3px 12px; font-size: 11px;"
                         " border: none; font-weight: 600; }"
                         "QPushButton:hover { background: rgba(115,201,145,0.3); }"));
    topRow->addWidget(openBtn);
    layout->addLayout(topRow);

    // Progress bar with gradient
    auto *progressBar = new QProgressBar(widget);
    progressBar->setObjectName(QStringLiteral("dlProgress"));
    progressBar->setMaximum(100);
    progressBar->setValue(0);
    progressBar->setFixedHeight(4);
    progressBar->setTextVisible(false);
    progressBar->setStyleSheet(QStringLiteral(
        "QProgressBar { background: ") + c["bg-lighter"]
        + QStringLiteral("; border: none; border-radius: 2px; }"
                         "QProgressBar::chunk { background: qlineargradient("
                         "x1:0,y1:0,x2:1,y2:0,stop:0 ") + c["purple-mid"]
        + QStringLiteral(",stop:1 ") + c["purple-light"]
        + QStringLiteral("); border-radius: 2px; }"));
    layout->addWidget(progressBar);

    // Status row
    auto *statusLabel = new QLabel(QStringLiteral("Starting..."), widget);
    statusLabel->setObjectName(QStringLiteral("dlStatus"));
    statusLabel->setStyleSheet(QStringLiteral("color: ") + c["text-secondary"]
        + QStringLiteral("; font-size: 11px;"));
    layout->addWidget(statusLabel);

    return widget;
}

// ── Clear completed ──────────────────────────────────────────────────
void DownloadManager::clearCompleted()
{
    for (int i = m_items.size() - 1; i >= 0; --i) {
        if (m_items[i].completed || m_items[i].cancelled) {
            if (m_items[i].widget) {
                m_listLayout->removeWidget(m_items[i].widget);
                m_items[i].widget->deleteLater();
            }
            m_items.removeAt(i);
        }
    }
    if (m_items.isEmpty()) m_emptyLabel->show();
    updateCountBadge();
}

// ── Counts ───────────────────────────────────────────────────────────
int DownloadManager::activeCount() const
{
    int count = 0;
    for (const auto &item : m_items) {
        if (!item.completed && !item.cancelled) ++count;
    }
    return count;
}

int DownloadManager::totalCount() const
{
    return static_cast<int>(m_items.size());
}

void DownloadManager::updateCountBadge()
{
    emit downloadCountChanged(activeCount(), totalCount());
}

// ── Utility ──────────────────────────────────────────────────────────
QString DownloadManager::formatBytes(qint64 bytes)
{
    if (bytes < 0) return QStringLiteral("?");
    if (bytes < 1024) return QStringLiteral("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QStringLiteral("%1 KB").arg(bytes / 1024.0, 0, 'f', 1);
    if (bytes < 1024 * 1024 * 1024) return QStringLiteral("%1 MB").arg(bytes / (1024.0 * 1024.0), 0, 'f', 1);
    return QStringLiteral("%1 GB").arg(bytes / (1024.0 * 1024.0 * 1024.0), 0, 'f', 2);
}
