/*
 * OyNIx Browser v3.1 — CrawlerPanel implementation
 * Local web crawler UI with two modes:
 *   1) Crawl from a list of URLs
 *   2) Broad web crawl (asks for confirmation)
 *
 * Uses the native C++ WebCrawler instead of the C# NativeAOT CoreBridge.
 */

#include "CrawlerPanel.h"
#include "search/WebCrawler.h"
#include "search/NyxSearch.h"
#include "theme/ThemeEngine.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTextEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QCheckBox>
#include <QProgressBar>
#include <QTimer>
#include <QMessageBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QInputDialog>

CrawlerPanel::CrawlerPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    setupStyles();
    connectCrawlerSignals();
}

CrawlerPanel::~CrawlerPanel() = default;

// ── UI Setup ────────────────────────────────────────────────────────────

void CrawlerPanel::setupUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    // ── Title ───────────────────────────────────────────────────────────
    auto *titleLabel = new QLabel(QStringLiteral("Web Crawler"), this);
    titleLabel->setObjectName(QStringLiteral("panelTitle"));
    root->addWidget(titleLabel);

    // ── URL Input ───────────────────────────────────────────────────────
    auto *inputLabel = new QLabel(QStringLiteral("URLs to crawl (one per line):"), this);
    inputLabel->setObjectName(QStringLiteral("sectionLabel"));
    root->addWidget(inputLabel);

    m_urlInput = new QTextEdit(this);
    m_urlInput->setObjectName(QStringLiteral("crawlerUrlInput"));
    m_urlInput->setPlaceholderText(
        QStringLiteral("https://example.com\nhttps://docs.qt.io\nhttps://en.wikipedia.org"));
    m_urlInput->setAcceptRichText(false);
    m_urlInput->setMaximumHeight(120);
    root->addWidget(m_urlInput);

    // ── Settings ────────────────────────────────────────────────────────
    auto *settingsLabel = new QLabel(QStringLiteral("Settings"), this);
    settingsLabel->setObjectName(QStringLiteral("sectionLabel"));
    root->addWidget(settingsLabel);

    // Depth
    auto *depthRow = new QHBoxLayout;
    depthRow->addWidget(new QLabel(QStringLiteral("Max depth:"), this));
    m_depthSpin = new QSpinBox(this);
    m_depthSpin->setRange(1, 10);
    m_depthSpin->setValue(2);
    m_depthSpin->setToolTip(QStringLiteral("How many link levels deep to crawl"));
    depthRow->addWidget(m_depthSpin);
    depthRow->addStretch();
    root->addLayout(depthRow);

    // Max pages
    auto *pagesRow = new QHBoxLayout;
    pagesRow->addWidget(new QLabel(QStringLiteral("Max pages:"), this));
    m_maxPagesSpin = new QSpinBox(this);
    m_maxPagesSpin->setRange(10, 100000);
    m_maxPagesSpin->setValue(500);
    m_maxPagesSpin->setSingleStep(100);
    m_maxPagesSpin->setToolTip(QStringLiteral("Maximum number of pages to crawl"));
    pagesRow->addWidget(m_maxPagesSpin);
    pagesRow->addStretch();
    root->addLayout(pagesRow);

    // Follow external links
    m_followExternal = new QCheckBox(QStringLiteral("Follow external links"), this);
    m_followExternal->setChecked(false);
    m_followExternal->setToolTip(
        QStringLiteral("When unchecked, only crawls links within the same domain"));
    root->addWidget(m_followExternal);

    // ── Action Buttons ──────────────────────────────────────────────────
    auto *btnRow = new QHBoxLayout;

    m_crawlListBtn = new QPushButton(QStringLiteral("Crawl List"), this);
    m_crawlListBtn->setObjectName(QStringLiteral("crawlBtn"));
    m_crawlListBtn->setToolTip(QStringLiteral("Crawl the URLs listed above"));
    connect(m_crawlListBtn, &QPushButton::clicked,
            this, &CrawlerPanel::onStartListCrawl);
    btnRow->addWidget(m_crawlListBtn);

    m_crawlWebBtn = new QPushButton(QStringLiteral("Crawl Web"), this);
    m_crawlWebBtn->setObjectName(QStringLiteral("crawlWebBtn"));
    m_crawlWebBtn->setToolTip(
        QStringLiteral("Broad crawl starting from a seed URL (asks for confirmation)"));
    connect(m_crawlWebBtn, &QPushButton::clicked,
            this, &CrawlerPanel::onStartBroadCrawl);
    btnRow->addWidget(m_crawlWebBtn);

    root->addLayout(btnRow);

    // ── Progress Area ───────────────────────────────────────────────────
    m_progressBar = new QProgressBar(this);
    m_progressBar->setObjectName(QStringLiteral("crawlerProgress"));
    m_progressBar->setRange(0, 0); // indeterminate by default
    m_progressBar->setTextVisible(false);
    m_progressBar->setMaximumHeight(6);
    m_progressBar->hide();
    root->addWidget(m_progressBar);

    auto *statusRow = new QHBoxLayout;
    m_statusLabel = new QLabel(QStringLiteral("Ready"), this);
    m_statusLabel->setObjectName(QStringLiteral("crawlerStatus"));
    statusRow->addWidget(m_statusLabel, 1);

    m_stopBtn = new QPushButton(QStringLiteral("Stop"), this);
    m_stopBtn->setObjectName(QStringLiteral("stopBtn"));
    m_stopBtn->hide();
    connect(m_stopBtn, &QPushButton::clicked,
            this, &CrawlerPanel::onStopCrawl);
    statusRow->addWidget(m_stopBtn);

    root->addLayout(statusRow);

    // ── Results ─────────────────────────────────────────────────────────
    auto *resultsHeader = new QHBoxLayout;
    auto *resultsLabel = new QLabel(QStringLiteral("Crawled Pages"), this);
    resultsLabel->setObjectName(QStringLiteral("sectionLabel"));
    resultsHeader->addWidget(resultsLabel);

    m_resultsCount = new QLabel(QStringLiteral("0"), this);
    m_resultsCount->setObjectName(QStringLiteral("badge"));
    resultsHeader->addWidget(m_resultsCount);
    resultsHeader->addStretch();

    root->addLayout(resultsHeader);

    m_resultsList = new QListWidget(this);
    m_resultsList->setObjectName(QStringLiteral("crawlerResults"));
    m_resultsList->setSpacing(1);
    m_resultsList->setWordWrap(true);
    connect(m_resultsList, &QListWidget::itemDoubleClicked,
            this, &CrawlerPanel::onResultDoubleClicked);
    root->addWidget(m_resultsList, 1);

    // ── Poll timer ──────────────────────────────────────────────────────
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(500);
    connect(m_pollTimer, &QTimer::timeout,
            this, &CrawlerPanel::onPollStatus);

    setFixedWidth(300);
}

// ── Styles ──��──────────────────────────────────────────────────────────

void CrawlerPanel::setupStyles()
{
    const auto &c = ThemeEngine::instance().colors();

    QString ss;
    ss += QStringLiteral("CrawlerPanel { background: ") + c["bg-darkest"] + QStringLiteral("; }\n");
    ss += QStringLiteral("#panelTitle { color: ") + c["purple-light"]
       + QStringLiteral("; font-size: 13px; font-weight: 700;"
                        " letter-spacing: 0.05em; text-transform: uppercase; }\n");
    ss += QStringLiteral("#sectionLabel { color: ") + c["text-secondary"]
       + QStringLiteral("; font-size: 11px; font-weight: 600;"
                        " text-transform: uppercase; letter-spacing: 0.05em; }\n");
    ss += QStringLiteral("#badge { color: ") + c["bg-darkest"]
       + QStringLiteral("; background: ") + c["purple-mid"]
       + QStringLiteral("; font-size: 10px; font-weight: 700;"
                        " padding: 1px 7px; border-radius: 8px; }\n");
    ss += QStringLiteral("#crawlerUrlInput { background: ") + c["bg-mid"]
       + QStringLiteral("; color: ") + c["text-primary"]
       + QStringLiteral("; border: 1px solid ") + c["border"]
       + QStringLiteral("; border-radius: 8px; padding: 6px; font-size: 12px; }\n");
    ss += QStringLiteral("#crawlerUrlInput:focus { border-color: ") + c["purple-mid"]
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QLabel { color: ") + c["text-primary"] + QStringLiteral("; }\n");
    ss += QStringLiteral("QSpinBox { background: ") + c["bg-mid"]
       + QStringLiteral("; color: ") + c["text-primary"]
       + QStringLiteral("; border: 1px solid ") + c["border"]
       + QStringLiteral("; border-radius: 6px; padding: 4px; }\n");
    ss += QStringLiteral("QCheckBox { color: ") + c["text-primary"]
       + QStringLiteral("; spacing: 6px; }\n");
    ss += QStringLiteral("QCheckBox::indicator { width: 14px; height: 14px; border: 1px solid ")
       + c["purple-mid"] + QStringLiteral("; border-radius: 3px; background: ") + c["bg-mid"]
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QCheckBox::indicator:checked { background: ") + c["purple-mid"]
       + QStringLiteral("; }\n");
    ss += QStringLiteral("#crawlBtn { background: ") + c["purple-mid"]
       + QStringLiteral("; color: white; border: none; border-radius: 8px;"
                        " padding: 6px 14px; font-weight: 600; }\n");
    ss += QStringLiteral("#crawlBtn:hover { background: ") + c["purple-light"]
       + QStringLiteral("; }\n");
    ss += QStringLiteral("#crawlWebBtn { background: rgba(110,106,179,0.15); color: ")
       + c["purple-light"] + QStringLiteral("; border: 1px solid rgba(110,106,179,0.3);"
                                             " border-radius: 8px; padding: 6px 14px;"
                                             " font-weight: 600; }\n");
    ss += QStringLiteral("#crawlWebBtn:hover { background: rgba(110,106,179,0.3); }\n");
    ss += QStringLiteral("#stopBtn { background: rgba(212,86,94,0.2); color: ") + c["error"]
       + QStringLiteral("; border: 1px solid rgba(212,86,94,0.3); border-radius: 6px;"
                        " padding: 4px 12px; }\n");
    ss += QStringLiteral("#stopBtn:hover { background: rgba(212,86,94,0.4); }\n");
    ss += QStringLiteral("#crawlerProgress { background: ") + c["bg-lighter"]
       + QStringLiteral("; border: none; border-radius: 3px; }\n");
    ss += QStringLiteral("#crawlerProgress::chunk { background: qlineargradient("
                         "x1:0,y1:0,x2:1,y2:0,stop:0 ") + c["purple-mid"]
       + QStringLiteral(",stop:1 ") + c["purple-light"]
       + QStringLiteral("); border-radius: 3px; }\n");
    ss += QStringLiteral("#crawlerStatus { color: ") + c["text-secondary"]
       + QStringLiteral("; font-size: 11px; }\n");
    ss += QStringLiteral("#crawlerResults { background: ") + c["bg-dark"]
       + QStringLiteral("; color: ") + c["text-primary"]
       + QStringLiteral("; border: 1px solid ") + c["border"]
       + QStringLiteral("; border-radius: 8px; font-size: 12px; outline: none; }\n");
    ss += QStringLiteral("#crawlerResults::item { padding: 4px 8px; border-radius: 4px; }\n");
    ss += QStringLiteral("#crawlerResults::item:selected { background: ") + c["selection"]
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QScrollBar:vertical { background: transparent; width: 5px; }\n");
    ss += QStringLiteral("QScrollBar::handle:vertical { background: ") + c["scrollbar"]
       + QStringLiteral("; border-radius: 2px; }\n");
    ss += QStringLiteral("QScrollBar::handle:vertical:hover { background: ") + c["scrollbar-hover"]
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }\n");
    setStyleSheet(ss);
}

// ── Connect to native WebCrawler signals ────────────────────────────────

void CrawlerPanel::connectCrawlerSignals()
{
    auto &crawler = WebCrawler::instance();

    // Index each discovered page into the search database
    connect(&crawler, &WebCrawler::pageDiscovered, this, [](const QJsonObject &page) {
        NyxSearch::instance()->indexCrawledPage(page);
    });
}

// ── Crawl Actions ───────────────────────────────────────────────────────

void CrawlerPanel::onStartListCrawl()
{
    const QString text = m_urlInput->toPlainText().trimmed();
    if (text.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Web Crawler"),
            QStringLiteral("Enter one or more URLs in the text box above."));
        return;
    }

    // Parse URLs from text (one per line)
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QJsonArray urlArray;
    for (const auto &line : lines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty())
            urlArray.append(trimmed);
    }

    if (urlArray.isEmpty()) return;

    // Apply settings and start
    auto &crawler = WebCrawler::instance();
    crawler.configure(
        m_depthSpin->value(),
        m_maxPagesSpin->value(),
        4, // concurrency
        m_followExternal->isChecked()
    );

    crawler.startList(urlArray);
    setRunningState(true);
    m_statusLabel->setText(QStringLiteral("Crawling %1 URL(s)...").arg(urlArray.size()));
}

void CrawlerPanel::onStartBroadCrawl()
{
    // Get seed URL
    bool ok = false;
    const QString seedUrl = QInputDialog::getText(
        this,
        QStringLiteral("Broad Web Crawl"),
        QStringLiteral("Enter the seed URL to start crawling from:"),
        QLineEdit::Normal,
        QStringLiteral("https://"),
        &ok);

    if (!ok || seedUrl.trimmed().isEmpty()) return;

    // Confirmation dialog — broad crawl can be intensive
    const auto result = QMessageBox::warning(
        this,
        QStringLiteral("Confirm Broad Crawl"),
        QStringLiteral(
            "You are about to start a broad web crawl.\n\n"
            "This will crawl the internet starting from:\n%1\n\n"
            "Settings:\n"
            "  Max depth: %2\n"
            "  Max pages: %3\n"
            "  Follow external links: %4\n\n"
            "This runs locally on your PC and may take a while.\n"
            "Are you sure you want to proceed?")
            .arg(seedUrl.trimmed())
            .arg(m_depthSpin->value())
            .arg(m_maxPagesSpin->value())
            .arg(m_followExternal->isChecked() ? QStringLiteral("Yes") : QStringLiteral("No")),
        QMessageBox::Yes | QMessageBox::Cancel,
        QMessageBox::Cancel);

    if (result != QMessageBox::Yes) return;

    auto &crawler = WebCrawler::instance();
    crawler.configure(
        m_depthSpin->value(),
        m_maxPagesSpin->value(),
        4,
        m_followExternal->isChecked()
    );

    crawler.startBroad(seedUrl.trimmed());
    setRunningState(true);
    m_statusLabel->setText(QStringLiteral("Broad crawl from %1...").arg(seedUrl.trimmed()));
}

void CrawlerPanel::onStopCrawl()
{
    WebCrawler::instance().stop();
    m_statusLabel->setText(QStringLiteral("Stopping..."));
}

// ── Status Polling ──────────────────────────────────────────────────────

void CrawlerPanel::onPollStatus()
{
    auto &crawler = WebCrawler::instance();
    const auto status = crawler.status();

    const bool running = status[QStringLiteral("running")].toBool();
    const int crawled  = status[QStringLiteral("crawled")].toInt();
    const int queued   = status[QStringLiteral("queued")].toInt();
    const int errored  = status[QStringLiteral("errored")].toInt();

    m_statusLabel->setText(
        QStringLiteral("Crawled: %1 | Queued: %2 | Errors: %3")
            .arg(crawled).arg(queued).arg(errored));

    // Update progress bar
    const int maxPages = m_maxPagesSpin->value();
    m_progressBar->setRange(0, maxPages);
    m_progressBar->setValue(crawled);
    m_progressBar->setTextVisible(true);

    // Refresh results list if new pages were crawled
    if (crawled != m_lastCount) {
        m_lastCount = crawled;
        refreshResults();
    }

    if (!running) {
        setRunningState(false);
        m_statusLabel->setText(
            QStringLiteral("Done. %1 pages crawled, %2 errors.")
                .arg(crawled).arg(errored));
        refreshResults();
        emit crawlFinished(crawled);
    }
}

void CrawlerPanel::refreshResults()
{
    auto &crawler = WebCrawler::instance();
    const int count = crawler.count();
    m_resultsCount->setText(QString::number(count));

    // Show last 200 results
    const int offset = qMax(0, count - 200);
    const auto results = crawler.results(offset, 200);

    m_resultsList->clear();
    for (const auto &val : results) {
        auto page = val.toObject();
        const QString url    = page[QStringLiteral("url")].toString();
        const QString title  = page[QStringLiteral("title")].toString();
        const QString status = page[QStringLiteral("status")].toString();
        const int depth      = page[QStringLiteral("crawl_depth")].toInt();

        auto *item = new QListWidgetItem(m_resultsList);

        QString display = title.isEmpty() ? url : title;
        if (display.length() > 60)
            display = display.left(57) + QStringLiteral("...");

        item->setText(display);
        item->setData(Qt::UserRole, url);
        item->setToolTip(
            QStringLiteral("URL: %1\nStatus: %2\nDepth: %3")
                .arg(url, status).arg(depth));

        // Color code by status
        const auto &tc = ThemeEngine::instance().colors();
        if (status == QLatin1String("ok"))
            item->setForeground(QColor(tc["success"]));
        else if (status.startsWith(QLatin1String("error")))
            item->setForeground(QColor(tc["error"]));
        else
            item->setForeground(QColor(tc["text-primary"]));
    }

    // Auto-scroll to bottom
    if (m_resultsList->count() > 0)
        m_resultsList->scrollToBottom();
}

void CrawlerPanel::onResultDoubleClicked()
{
    auto *item = m_resultsList->currentItem();
    if (!item) return;
    const QString url = item->data(Qt::UserRole).toString();
    if (!url.isEmpty())
        emit openUrl(url);
}

// ── State Management ────────────────────────────────────────────────────

void CrawlerPanel::setRunningState(bool running)
{
    m_crawlListBtn->setEnabled(!running);
    m_crawlWebBtn->setEnabled(!running);
    m_urlInput->setReadOnly(running);
    m_depthSpin->setEnabled(!running);
    m_maxPagesSpin->setEnabled(!running);
    m_followExternal->setEnabled(!running);

    m_stopBtn->setVisible(running);
    m_progressBar->setVisible(running);

    if (running) {
        m_lastCount = 0;
        m_resultsList->clear();
        m_resultsCount->setText(QStringLiteral("0"));
        m_progressBar->setRange(0, 0);
        m_pollTimer->start();
    } else {
        m_pollTimer->stop();
    }
}
