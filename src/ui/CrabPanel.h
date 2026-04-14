/*
 * OyNIx Browser v3.1 — CrabPanel ("Crabs")
 * Inline sidebar panel for the CrabCrawler web crawler.
 * Two modes: crawl from a URL list, or broad crawl (with confirmation).
 * Uses signal-driven updates instead of polling for efficiency.
 */

#pragma once

#include <QWidget>

class QVBoxLayout;
class QTextEdit;
class QListWidget;
class QPushButton;
class QLabel;
class QSpinBox;
class QCheckBox;
class QComboBox;
class QProgressBar;

class CrabPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CrabPanel(QWidget *parent = nullptr);
    ~CrabPanel() override;

signals:
    /// Emitted when user clicks a crawled URL to open it in a tab
    void openUrl(const QString &url);
    /// Emitted when crawl completes
    void crawlFinished(int pageCount);

private slots:
    void onStartListCrawl();
    void onStartBroadCrawl();
    void onStopCrawl();
    void onPauseCrawl();
    void onProgress(int crawled, int queued, int errors);
    void onPageIndexed(const QString &url, const QString &title, const QString &status);
    void onCrawlFinished();
    void onStateChanged(const QString &stateString);
    void onResultDoubleClicked();

private:
    void setupUi();
    void setupStyles();
    void connectCrawlerSignals();
    void setRunningState(bool running);

    // Input page
    QTextEdit     *m_urlInput       = nullptr;
    QPushButton   *m_crawlListBtn   = nullptr;
    QPushButton   *m_crawlWebBtn    = nullptr;

    // Settings
    QSpinBox      *m_depthSpin      = nullptr;
    QSpinBox      *m_maxPagesSpin   = nullptr;
    QCheckBox     *m_followExternal = nullptr;
    QComboBox     *m_langFilter     = nullptr;

    // Progress
    QProgressBar  *m_progressBar    = nullptr;
    QLabel        *m_statusLabel    = nullptr;
    QPushButton   *m_stopBtn        = nullptr;
    QPushButton   *m_pauseBtn       = nullptr;

    // Results
    QListWidget   *m_resultsList    = nullptr;
    QLabel        *m_resultsCount   = nullptr;

    // State
    int            m_crawledCount   = 0;

    // Bounded result list: only keep last N items in the QListWidget
    static constexpr int MaxDisplayedResults = 300;
};
