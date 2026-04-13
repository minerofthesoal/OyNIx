/*
 * OyNIx Browser v3.1 — CrawlerPanel
 * Inline sidebar panel for local web crawling.
 * Two modes: crawl from a URL list, or broad crawl (with confirmation).
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
class QProgressBar;
class QTimer;
class QStackedWidget;

class CrawlerPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CrawlerPanel(QWidget *parent = nullptr);
    ~CrawlerPanel() override;

signals:
    /// Emitted when user clicks a crawled URL to open it in a tab
    void openUrl(const QString &url);
    /// Emitted when crawl completes with pages ready to index
    void crawlFinished(int pageCount);

private slots:
    void onStartListCrawl();
    void onStartBroadCrawl();
    void onStopCrawl();
    void onPollStatus();
    void onResultDoubleClicked();

private:
    void setupUi();
    void setupStyles();
    void connectCrawlerSignals();
    void setRunningState(bool running);
    void refreshResults();

    // Input page
    QTextEdit     *m_urlInput       = nullptr;
    QPushButton   *m_crawlListBtn   = nullptr;
    QPushButton   *m_crawlWebBtn    = nullptr;

    // Settings
    QSpinBox      *m_depthSpin      = nullptr;
    QSpinBox      *m_maxPagesSpin   = nullptr;
    QCheckBox     *m_followExternal = nullptr;

    // Progress
    QProgressBar  *m_progressBar    = nullptr;
    QLabel        *m_statusLabel    = nullptr;
    QPushButton   *m_stopBtn        = nullptr;

    // Results
    QListWidget   *m_resultsList    = nullptr;
    QLabel        *m_resultsCount   = nullptr;

    // State
    QTimer        *m_pollTimer      = nullptr;
    int            m_lastCount      = 0;
};
