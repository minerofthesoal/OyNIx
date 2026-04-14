/*
 * OyNIx Browser v3.1 — CrabPanel ("Crabs") implementation
 * Local web crawler UI with two modes:
 *   1) Crawl from a list of URLs
 *   2) Broad web crawl (asks for confirmation)
 *
 * Uses the CrabCrawler engine with signal-driven updates (no polling timer).
 * Results stream in via pageIndexed() signal — no list rebuild.
 */

#include "CrabPanel.h"
#include "data/Database.h"
#include "search/CrabCrawler.h"
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
#include <QComboBox>
#include <QProgressBar>
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QLineEdit>

CrabPanel::CrabPanel(QWidget *parent)
    : QWidget(parent)
{
    setupUi();
    setupStyles();
    connectCrawlerSignals();
}

CrabPanel::~CrabPanel() = default;

// ── UI Setup ────────────────────────────────────────────────────────────

void CrabPanel::setupUi()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    // ── Title ───────────────────────────────────────────────────────────
    auto *titleLabel = new QLabel(QStringLiteral("Crabs"), this);
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
    m_depthSpin->setRange(1, 42);
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

    // Language filter
    auto *langRow = new QHBoxLayout;
    langRow->addWidget(new QLabel(QStringLiteral("Language:"), this));
    m_langFilter = new QComboBox(this);
    m_langFilter->setToolTip(QStringLiteral("Only index pages in this language (based on html lang attribute)"));
    m_langFilter->addItem(QStringLiteral("Any language"), QString());
    m_langFilter->addItem(QStringLiteral("English"),    QStringLiteral("en"));
    m_langFilter->addItem(QStringLiteral("Spanish"),    QStringLiteral("es"));
    m_langFilter->addItem(QStringLiteral("French"),     QStringLiteral("fr"));
    m_langFilter->addItem(QStringLiteral("German"),     QStringLiteral("de"));
    m_langFilter->addItem(QStringLiteral("Portuguese"), QStringLiteral("pt"));
    m_langFilter->addItem(QStringLiteral("Italian"),    QStringLiteral("it"));
    m_langFilter->addItem(QStringLiteral("Dutch"),      QStringLiteral("nl"));
    m_langFilter->addItem(QStringLiteral("Russian"),    QStringLiteral("ru"));
    m_langFilter->addItem(QStringLiteral("Chinese"),    QStringLiteral("zh"));
    m_langFilter->addItem(QStringLiteral("Japanese"),   QStringLiteral("ja"));
    m_langFilter->addItem(QStringLiteral("Korean"),     QStringLiteral("ko"));
    m_langFilter->addItem(QStringLiteral("Arabic"),     QStringLiteral("ar"));
    m_langFilter->addItem(QStringLiteral("Hindi"),      QStringLiteral("hi"));
    m_langFilter->addItem(QStringLiteral("Turkish"),    QStringLiteral("tr"));
    m_langFilter->addItem(QStringLiteral("Polish"),     QStringLiteral("pl"));
    m_langFilter->addItem(QStringLiteral("Swedish"),    QStringLiteral("sv"));
    m_langFilter->addItem(QStringLiteral("Norwegian"),  QStringLiteral("no"));
    m_langFilter->addItem(QStringLiteral("Danish"),     QStringLiteral("da"));
    m_langFilter->addItem(QStringLiteral("Finnish"),    QStringLiteral("fi"));
    m_langFilter->addItem(QStringLiteral("Czech"),      QStringLiteral("cs"));
    langRow->addWidget(m_langFilter, 1);
    root->addLayout(langRow);

    // ── Action Buttons ──────────────────────────────────────────────────
    auto *btnRow = new QHBoxLayout;

    m_crawlListBtn = new QPushButton(QStringLiteral("Crawl List"), this);
    m_crawlListBtn->setObjectName(QStringLiteral("crawlBtn"));
    m_crawlListBtn->setToolTip(QStringLiteral("Crawl the URLs listed above"));
    connect(m_crawlListBtn, &QPushButton::clicked,
            this, &CrabPanel::onStartListCrawl);
    btnRow->addWidget(m_crawlListBtn);

    m_crawlWebBtn = new QPushButton(QStringLiteral("Crawl Web"), this);
    m_crawlWebBtn->setObjectName(QStringLiteral("crawlWebBtn"));
    m_crawlWebBtn->setToolTip(
        QStringLiteral("Broad crawl starting from a seed URL (asks for confirmation)"));
    connect(m_crawlWebBtn, &QPushButton::clicked,
            this, &CrabPanel::onStartBroadCrawl);
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

    m_pauseBtn = new QPushButton(QStringLiteral("Pause"), this);
    m_pauseBtn->setObjectName(QStringLiteral("pauseBtn"));
    m_pauseBtn->hide();
    connect(m_pauseBtn, &QPushButton::clicked,
            this, &CrabPanel::onPauseCrawl);
    statusRow->addWidget(m_pauseBtn);

    m_stopBtn = new QPushButton(QStringLiteral("Stop"), this);
    m_stopBtn->setObjectName(QStringLiteral("stopBtn"));
    m_stopBtn->hide();
    connect(m_stopBtn, &QPushButton::clicked,
            this, &CrabPanel::onStopCrawl);
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
            this, &CrabPanel::onResultDoubleClicked);
    root->addWidget(m_resultsList, 1);

    // ── Community DB export ────────────────────────────────────────────
    auto *exportBtn = new QPushButton(QStringLiteral("Export JSONL for Community DB"), this);
    exportBtn->setObjectName(QStringLiteral("exportBtn"));
    exportBtn->setToolTip(QStringLiteral("Export your crawled data as JSONL to upload to the community extras database on GitHub"));
    connect(exportBtn, &QPushButton::clicked, this, [this]() {
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("Export Community DB"),
            QDir::homePath() + QStringLiteral("/oynix_community_export.jsonl"),
            QStringLiteral("JSONL Files (*.jsonl)"));
        if (path.isEmpty()) return;

        if (Database::instance()->exportToJsonl(path)) {
            QMessageBox::information(this, QStringLiteral("Export Complete"),
                QStringLiteral("Data exported to:\n%1\n\n"
                    "You can upload this file to the OyNIx community extras "
                    "database on GitHub.").arg(path));
        } else {
            QMessageBox::warning(this, QStringLiteral("Export Failed"),
                QStringLiteral("Could not write to:\n%1").arg(path));
        }
    });
    root->addWidget(exportBtn);

    setFixedWidth(300);
}

// ── Styles ──────────────────────────────────────────────────────────────

void CrabPanel::setupStyles()
{
    const auto &c = ThemeEngine::instance().colors();

    QString ss;
    ss += QStringLiteral("CrabPanel { background: ") + c["bg-darkest"] + QStringLiteral("; }\n");
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
    ss += QStringLiteral("#pauseBtn { background: rgba(210,170,60,0.2); color: ") + c["warning"]
       + QStringLiteral("; border: 1px solid rgba(210,170,60,0.3); border-radius: 6px;"
                        " padding: 4px 12px; }\n");
    ss += QStringLiteral("#pauseBtn:hover { background: rgba(210,170,60,0.4); }\n");
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

// ── Connect to CrabCrawler signals (no polling) ─────────────────────────

void CrabPanel::connectCrawlerSignals()
{
    auto &crawler = CrabCrawler::instance();

    connect(&crawler, &CrabCrawler::progress,
            this, &CrabPanel::onProgress);
    connect(&crawler, &CrabCrawler::pageIndexed,
            this, &CrabPanel::onPageIndexed);
    connect(&crawler, &CrabCrawler::finished,
            this, &CrabPanel::onCrawlFinished);
    connect(&crawler, &CrabCrawler::stateChanged,
            this, &CrabPanel::onStateChanged);
}

// ── Crawl Actions ───────────────────────────────────────────────────────

void CrabPanel::onStartListCrawl()
{
    const QString text = m_urlInput->toPlainText().trimmed();
    if (text.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Crabs"),
            QStringLiteral("Enter one or more URLs in the text box above."));
        return;
    }

    const QStringList lines = text.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QStringList urls;
    for (const auto &line : lines) {
        const QString trimmed = line.trimmed();
        if (!trimmed.isEmpty())
            urls.append(trimmed);
    }

    if (urls.isEmpty()) return;

    auto &crawler = CrabCrawler::instance();
    crawler.configure(
        m_depthSpin->value(),
        m_maxPagesSpin->value(),
        4, // concurrency
        m_followExternal->isChecked()
    );
    crawler.setLanguageFilter(m_langFilter->currentData().toString());

    crawler.startList(urls);
    setRunningState(true);
    m_statusLabel->setText(QStringLiteral("Crawling %1 URL(s)...").arg(urls.size()));
}

void CrabPanel::onStartBroadCrawl()
{
    bool ok = false;
    const QString seedUrl = QInputDialog::getText(
        this,
        QStringLiteral("Broad Web Crawl"),
        QStringLiteral("Enter the seed URL to start crawling from:"),
        QLineEdit::Normal,
        QStringLiteral("https://"),
        &ok);

    if (!ok || seedUrl.trimmed().isEmpty()) return;

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

    auto &crawler = CrabCrawler::instance();
    crawler.configure(
        m_depthSpin->value(),
        m_maxPagesSpin->value(),
        4,
        m_followExternal->isChecked()
    );
    crawler.setLanguageFilter(m_langFilter->currentData().toString());

    crawler.startBroad(seedUrl.trimmed());
    setRunningState(true);
    m_statusLabel->setText(QStringLiteral("Broad crawl from %1...").arg(seedUrl.trimmed()));
}

void CrabPanel::onStopCrawl()
{
    CrabCrawler::instance().stop();
    m_statusLabel->setText(QStringLiteral("Stopping..."));
}

void CrabPanel::onPauseCrawl()
{
    auto &crawler = CrabCrawler::instance();
    if (crawler.state() == CrabCrawler::State::Paused) {
        crawler.resume();
        m_pauseBtn->setText(QStringLiteral("Pause"));
    } else {
        crawler.pause();
        m_pauseBtn->setText(QStringLiteral("Resume"));
    }
}

// ── Signal-driven status updates (replaces polling) ─────────────────────

void CrabPanel::onProgress(int crawled, int queued, int errors)
{
    m_crawledCount = crawled;
    m_statusLabel->setText(
        QStringLiteral("Crawled: %1 | Queued: %2 | Errors: %3")
            .arg(crawled).arg(queued).arg(errors));

    const int maxPages = m_maxPagesSpin->value();
    m_progressBar->setRange(0, maxPages);
    m_progressBar->setValue(crawled);
    m_progressBar->setTextVisible(true);

    m_resultsCount->setText(QString::number(crawled));
}

void CrabPanel::onPageIndexed(const QString &url, const QString &title, const QString &status)
{
    // Incremental append — no full list rebuild
    auto *item = new QListWidgetItem(m_resultsList);

    QString display = title.isEmpty() ? url : title;
    if (display.length() > 60)
        display = display.left(57) + QStringLiteral("...");

    item->setText(display);
    item->setData(Qt::UserRole, url);
    item->setToolTip(QStringLiteral("URL: %1\nStatus: %2").arg(url, status));

    const auto &tc = ThemeEngine::instance().colors();
    if (status == QLatin1String("ok"))
        item->setForeground(QColor(tc["success"]));
    else if (status.startsWith(QLatin1String("error")))
        item->setForeground(QColor(tc["error"]));
    else
        item->setForeground(QColor(tc["text-primary"]));

    // Trim old entries to bound memory
    while (m_resultsList->count() > MaxDisplayedResults)
        delete m_resultsList->takeItem(0);

    m_resultsList->scrollToBottom();

    // Index into NyxSearch
    NyxSearch::instance()->indexPage(url, title, QString(), QStringLiteral("nyx"));
}

void CrabPanel::onCrawlFinished()
{
    setRunningState(false);
    const int errors = CrabCrawler::instance().errorCount();
    m_statusLabel->setText(
        QStringLiteral("Done. %1 pages crawled, %2 errors.")
            .arg(m_crawledCount).arg(errors));
    emit crawlFinished(m_crawledCount);
}

void CrabPanel::onStateChanged(const QString &stateString)
{
    Q_UNUSED(stateString)
}

void CrabPanel::onResultDoubleClicked()
{
    auto *item = m_resultsList->currentItem();
    if (!item) return;
    const QString url = item->data(Qt::UserRole).toString();
    if (!url.isEmpty())
        emit openUrl(url);
}

// ── State Management ────────────────────────────────────────────────────

void CrabPanel::setRunningState(bool running)
{
    m_crawlListBtn->setEnabled(!running);
    m_crawlWebBtn->setEnabled(!running);
    m_urlInput->setReadOnly(running);
    m_depthSpin->setEnabled(!running);
    m_maxPagesSpin->setEnabled(!running);
    m_followExternal->setEnabled(!running);

    m_stopBtn->setVisible(running);
    m_pauseBtn->setVisible(running);
    m_progressBar->setVisible(running);

    if (running) {
        m_crawledCount = 0;
        m_resultsList->clear();
        m_resultsCount->setText(QStringLiteral("0"));
        m_progressBar->setRange(0, 0);
        m_pauseBtn->setText(QStringLiteral("Pause"));
    }
}
