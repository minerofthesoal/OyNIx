/*
 * OyNIx Browser v3.0 — BrowserWindow implementation
 * Main browser window: menus, toolbar, tabs, status bar, config, theming.
 */

#include "BrowserWindow.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPrintDialog>
#include <QPrinter>
#include <QShortcut>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QUrlQuery>
#include <QWebEngineView>

#include "TabWidget.h"
#include "WebView.h"
#include "UrlBar.h"
#include "FindBar.h"

#include "ai/AiChatPanel.h"
#include "ai/AiManager.h"
#include "ui/CommandPalette.h"
#include "ui/DownloadManager.h"
#include "ui/SettingsDialog.h"
#include "ui/TreeTabSidebar.h"
#include "ui/BookmarkPanel.h"
#include "media/AudioPlayer.h"
#include "data/BookmarkManager.h"
#include "data/HistoryManager.h"
#include "data/SessionManager.h"
#include "security/SecurityManager.h"
#include "extensions/ExtensionManager.h"
#include "extensions/ExtensionBridge.h"
#include "data/Database.h"
#include "data/CommunityDbSync.h"
#include "data/WebCache.h"
#include "search/NyxSearch.h"
#include "interop/CoreBridge.h"
#include "ui/ExtensionPanel.h"
#include "ui/CrabPanel.h"
#include "theme/ThemeEngine.h"
#include "pages/InternalPages.h"

#include <QProgressBar>
#include <QSplitter>
#include <QWebEngineProfile>
#include <QWebEngineDownloadRequest>
#include <memory>

// ── Helpers ──────────────────────────────────────────────────────────

static const char * const kConfigFile = "browser_config.json";

// ── Construction / Destruction ───────────────────────────────────────

BrowserWindow::BrowserWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("OyNIx Browser"));
    resize(1280, 800);
    setMinimumSize(640, 400);

    loadConfig();

    // Initialize C# core via NativeAOT bridge
    CoreBridge::instance().initialize(configPath());

    // Initialize data subsystems
    m_bookmarkManager  = new BookmarkManager(configPath(), this);
    m_historyManager   = new HistoryManager(configPath(), this);
    m_sessionManager   = new SessionManager(this);
    m_extensionManager = new ExtensionManager(this);

    // Central layout: tree-tabs | tab-widget | ext-panel | ai-panel
    m_splitter = new QSplitter(Qt::Horizontal, this);

    // Tree tab sidebar (left)
    m_tabWidget = new TabWidget(this);
    m_treeTabSidebar = new TreeTabSidebar(m_tabWidget, this);
    m_treeTabSidebar->setVisible(m_config[QStringLiteral("tree_tabs")].toBool());

    // Extension panel (inline, hidden by default)
    m_extensionPanel = new ExtensionPanel(this);
    m_extensionPanel->hide();

    // Crab panel (inline, hidden by default)
    m_crabPanel = new CrabPanel(this);
    m_crabPanel->hide();

    // AI chat panel (right)
    m_aiPanel = new AiChatPanel(this);

    m_splitter->addWidget(m_treeTabSidebar);
    m_splitter->addWidget(m_tabWidget);
    m_splitter->addWidget(m_extensionPanel);
    m_splitter->addWidget(m_crabPanel);
    m_splitter->addWidget(m_aiPanel);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setStretchFactor(2, 0);
    m_splitter->setStretchFactor(3, 0);
    m_splitter->setStretchFactor(4, 0);
    m_splitter->setHandleWidth(1);
    setCentralWidget(m_splitter);

    // Download manager (dock)
    m_downloadManager = new DownloadManager(this);
    m_downloadManager->hide();

    // Audio player
    m_audioPlayer = new AudioPlayer(this);
    m_audioPlayer->hide();

    // Command palette
    m_commandPalette = new CommandPalette(this);

    // Bookmark panel
    m_bookmarkPanel = new BookmarkPanel(m_bookmarkManager, this);
    m_bookmarkPanel->hide();

    connect(m_tabWidget, &TabWidget::currentTitleChanged, this, &BrowserWindow::updateWindowTitle);
    connect(m_tabWidget, &TabWidget::currentUrlChanged, this, [this](const QUrl &url) {
        if (m_urlBar) m_urlBar->setDisplayUrl(url);
        updateSecurityIndicator(url);
        handleSecurityCheck(url);
    });

    // Handle oyn:// internal URLs from WebPage navigation interception
    connect(m_tabWidget, &TabWidget::internalUrlRequested, this, &BrowserWindow::handleInternalUrl);

    // Download handling
    connect(QWebEngineProfile::defaultProfile(), &QWebEngineProfile::downloadRequested,
            this, &BrowserWindow::handleDownload);

    // Tree tab sidebar connections
    connect(m_treeTabSidebar, &TreeTabSidebar::newTabRequested, this, [this]() { newTab(); });
    connect(m_treeTabSidebar, &TreeTabSidebar::closeTabRequested, this, &BrowserWindow::closeTab);

    // Bookmark panel connections
    connect(m_bookmarkPanel, &BookmarkPanel::bookmarkActivated, this, &BrowserWindow::navigateTo);

    // Crab panel connections
    connect(m_crabPanel, &CrabPanel::openUrl, this, [this](const QString &url) {
        navigateTo(QUrl(url));
    });

    // AI panel connections
    connect(m_aiPanel, &AiChatPanel::summarizePageRequested, this, [this]() {
        auto *view = m_tabWidget ? m_tabWidget->currentWebView() : nullptr;
        if (view) {
            view->page()->toPlainText([this, view](const QString &text) {
                AiManager::instance().summarizePage(text, view->title());
            });
        }
    });

    // Extension system setup
    m_extensionManager->setBrowserContext(m_tabWidget, m_bookmarkManager);
    m_tabWidget->setExtensionManager(m_extensionManager);

    // Database + search: auto-update stats and index visited pages
    connect(Database::instance(), &Database::databaseChanged,
            this, &BrowserWindow::updateDbStats);
    connect(m_tabWidget, &TabWidget::currentUrlChanged, this, [this](const QUrl &url) {
        if (url.scheme() == QLatin1String("http") || url.scheme() == QLatin1String("https")) {
            auto *view = m_tabWidget->currentWebView();
            if (view) {
                const QString urlStr = url.toString();
                const QString title = view->title();
                NyxSearch::instance()->indexPage(urlStr, title, QString());
                if (m_historyManager)
                    m_historyManager->addEntry(urlStr, title);
                // Cache visited page metadata
                WebCache::instance().store(urlStr, title, QString());
            }
        }
    });

    createMenuBar();
    createNavigationToolbar();
    createStatusBar();
    createFindBar();
    registerCommands();

    applyTheme();
    updateDbStats();

    // Start extension background scripts after UI is ready
    m_extensionManager->startBackgroundScripts();

    // Fetch community database from GitHub (async, once per day)
    CommunityDbSync::instance().fetchOnStartup();

    // Prune stale web cache entries (older than 30 days)
    WebCache::instance().pruneOldEntries(30);

    // Restore session or open default tab
    if (m_config[QStringLiteral("restore_session")].toBool() && m_sessionManager) {
        const QJsonArray tabs = m_sessionManager->loadSession();
        if (!tabs.isEmpty()) {
            for (const auto &val : tabs) {
                auto tab = val.toObject();
                newTab(QUrl(tab[QStringLiteral("url")].toString()));
            }
        } else {
            newTab(QUrl(QStringLiteral("oyn://home")));
        }
    } else {
        newTab(QUrl(QStringLiteral("oyn://home")));
    }

    // Shortcuts — only register keys that have NO menu-action equivalent.
    // (Duplicating a shortcut on both a QAction and a QShortcut causes Qt to
    //  treat them as ambiguous and silently ignore both.)
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+L")), this, [this]{
        if (m_urlBar) { m_urlBar->setFocus(); m_urlBar->selectAll(); }
    });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+K")), this, [this]{
        if (m_commandPalette) m_commandPalette->toggle();
    });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+D")), this, [this]{
        auto *v = m_tabWidget ? m_tabWidget->currentWebView() : nullptr;
        if (v && m_bookmarkManager)
            m_bookmarkManager->add(v->url().toString(), v->title());
    });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+B")), this, [this]{
        if (m_bookmarkPanel) m_bookmarkPanel->setVisible(!m_bookmarkPanel->isVisible());
    });
}

BrowserWindow::~BrowserWindow()
{
    // Auto-save session
    if (m_sessionManager && m_sessionManager->autoSave() && m_tabWidget)
        m_sessionManager->saveSession(m_tabWidget);
    saveConfig();
}

// ── Config path ──────────────────────────────────────────────────────

QString BrowserWindow::configPath() const
{
#ifdef Q_OS_WIN
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + QStringLiteral("/oynix");
#else
    return QDir::homePath() + QStringLiteral("/.config/oynix");
#endif
}

// ── Menus ────────────────────────────────────────────────────────────

void BrowserWindow::createMenuBar()
{
    QMenuBar *mb = menuBar();

    // ── File ─────────────────────────────────────────────────────────
    m_fileMenu = mb->addMenu(tr("&File"));
    {
        auto *a = m_fileMenu->addAction(tr("New Tab"), this, [this]{ newTab(); });
        a->setShortcut(QKeySequence::AddTab);
    }
    {
        auto *a = m_fileMenu->addAction(tr("Open File…"), this, [this]{
            const QString path = QFileDialog::getOpenFileName(this, tr("Open File"));
            if (!path.isEmpty())
                navigateTo(QUrl::fromLocalFile(path));
        });
        a->setShortcut(QKeySequence::Open);
    }
    {
        auto *a = m_fileMenu->addAction(tr("Save Page"), this, [this]{
            // Placeholder — requires current WebView
        });
        a->setShortcut(QKeySequence::Save);
    }
    {
        auto *a = m_fileMenu->addAction(tr("Print…"), this, [this]{
#if QT_CONFIG(printer)
            QPrinter printer;
            QPrintDialog dlg(&printer, this);
            if (dlg.exec() == QDialog::Accepted) {
                // Actual printing requires current page reference
            }
#endif
        });
        a->setShortcut(QKeySequence::Print);
    }

    m_fileMenu->addSeparator();
    {
        auto *a = m_fileMenu->addAction(tr("Close Tab"), this, [this]{ closeTab(); });
        a->setShortcut(QKeySequence(QStringLiteral("Ctrl+W")));
    }
    m_fileMenu->addSeparator();
    {
        auto *a = m_fileMenu->addAction(tr("Quit"), this, &QWidget::close);
        a->setShortcut(QKeySequence::Quit);
    }

    // ── Edit ─────────────────────────────────────────────────────────
    m_editMenu = mb->addMenu(tr("&Edit"));
    {
        auto *a = m_editMenu->addAction(tr("Find…"), this, [this]{
            if (m_findBar) m_findBar->showBar();
        });
        a->setShortcut(QKeySequence::Find);
    }
    m_editMenu->addSeparator();
    m_editMenu->addAction(tr("Settings"), this, &BrowserWindow::showSettings);

    // ── View ─────────────────────────────────────────────────────────
    m_viewMenu = mb->addMenu(tr("&View"));
    {
        auto *a = m_viewMenu->addAction(tr("Zoom In"), this, &BrowserWindow::zoomIn);
        a->setShortcut(QKeySequence::ZoomIn);
    }
    {
        auto *a = m_viewMenu->addAction(tr("Zoom Out"), this, &BrowserWindow::zoomOut);
        a->setShortcut(QKeySequence::ZoomOut);
    }
    {
        auto *a = m_viewMenu->addAction(tr("Reset Zoom"), this, &BrowserWindow::resetZoom);
        a->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
    }
    m_viewMenu->addSeparator();
    {
        auto *a = m_viewMenu->addAction(tr("Fullscreen"), this, &BrowserWindow::toggleFullscreen);
        a->setShortcut(QKeySequence(QStringLiteral("F11")));
    }

    // Themes submenu
    QMenu *themesMenu = m_viewMenu->addMenu(tr("Themes"));
    auto *themeGroup = new QActionGroup(this);
    themeGroup->setExclusive(true);

    const QStringList themeNames = {
        QStringLiteral("obsidian"),
        QStringLiteral("midnight"),
        QStringLiteral("slate"),
        QStringLiteral("nord"),
        QStringLiteral("ember"),
    };

    for (const auto &name : themeNames) {
        QAction *a = themesMenu->addAction(name);
        a->setCheckable(true);
        themeGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, name]{ setTheme(name); });
    }

    m_viewMenu->addSeparator();
    {
        auto *a = m_viewMenu->addAction(tr("Toggle Tree Tabs"), this, [this]{
            if (m_treeTabSidebar) m_treeTabSidebar->setVisible(!m_treeTabSidebar->isVisible());
        });
        a->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+T")));
    }
    {
        auto *a = m_viewMenu->addAction(tr("Toggle AI Panel"), this, [this]{
            if (m_aiPanel) m_aiPanel->togglePanel();
        });
        a->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+A")));
    }

    // ── Search ───────────────────────────────────────────────────────
    m_searchMenu = mb->addMenu(tr("&Search"));
    m_searchMenu->addAction(tr("Search Engine Selector"), this, []{ /* placeholder */ });

    QMenu *syncMenu = m_searchMenu->addMenu(tr("GitHub Sync"));
    syncMenu->addAction(tr("Sync Bookmarks"), this, []{ /* placeholder */ });
    syncMenu->addAction(tr("Sync Settings"),  this, []{ /* placeholder */ });

    // ── Tools ────────────────────────────────────────────────────────
    m_toolsMenu = mb->addMenu(tr("&Tools"));
    m_toolsMenu->addAction(tr("Downloads"),      this, &BrowserWindow::showDownloads);
    m_toolsMenu->addAction(tr("History"),         this, &BrowserWindow::showHistory);
    m_toolsMenu->addAction(tr("Bookmarks"),       this, &BrowserWindow::showBookmarks);
    m_toolsMenu->addSeparator();
    m_toolsMenu->addAction(tr("Reading List"),    this, []{ /* placeholder */ });
    m_toolsMenu->addAction(tr("Extensions"),      this, [this]{
        if (m_extensionPanel) m_extensionPanel->setVisible(!m_extensionPanel->isVisible());
    });
    {
        auto *a = m_toolsMenu->addAction(tr("Crabs"), this, [this]{
            if (m_crabPanel) m_crabPanel->setVisible(!m_crabPanel->isVisible());
        });
        a->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+W")));
    }
    m_toolsMenu->addAction(tr("Profiles"),        this, [this]{
        navigateTo(QUrl(QStringLiteral("oyn://profiles")));
    });
    m_toolsMenu->addAction(tr("Sessions"),        this, [this]{
        navigateTo(QUrl(QStringLiteral("oyn://sessions")));
    });
    m_toolsMenu->addSeparator();
    m_toolsMenu->addAction(tr("Audio Player"),    this, [this]{
        if (m_audioPlayer) m_audioPlayer->setVisible(!m_audioPlayer->isVisible());
    });
    {
        auto *a = m_toolsMenu->addAction(tr("Command Palette"), this, [this]{
            if (m_commandPalette) m_commandPalette->toggle();
        });
        a->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+P")));
    }

    // ── AI ───────────────────────────────────────────────────────────
    m_aiMenu = mb->addMenu(tr("&AI"));
    m_aiMenu->addAction(tr("AI Assistant"), this, [this]{
        if (m_aiPanel) m_aiPanel->togglePanel();
    });
    m_aiMenu->addAction(tr("Summarize Page"), this, [this]{
        auto *view = m_tabWidget ? m_tabWidget->currentWebView() : nullptr;
        if (view) {
            view->page()->toPlainText([this, view](const QString &text) {
                AiManager::instance().summarizePage(text, view->title());
            });
            if (m_aiPanel && !m_aiPanel->isPanelVisible()) m_aiPanel->showPanel();
        }
    });
    m_aiMenu->addSeparator();
    m_aiMenu->addAction(tr("AI Settings..."), this, [this]{
        // Open settings dialog on AI tab
        SettingsDialog dlg(m_config, this);
        if (dlg.exec() == QDialog::Accepted)
            m_config = dlg.updatedConfig();
    });

    // ── Developer ────────────────────────────────────────────────────
    m_developerMenu = mb->addMenu(tr("&Developer"));
    {
        auto *a = m_developerMenu->addAction(tr("Developer Tools"), this, []{ /* placeholder */ });
        a->setShortcut(QKeySequence(QStringLiteral("F12")));
    }
    m_developerMenu->addAction(tr("View Source"),
                               this, []{ /* placeholder */ });

    // ── Help ─────────────────────────────────────────────────────────
    m_helpMenu = mb->addMenu(tr("&Help"));
    m_helpMenu->addAction(tr("About OyNIx"), this, &BrowserWindow::showAbout);
    m_helpMenu->addAction(tr("About Qt"),    this, [this]{
        QMessageBox::aboutQt(this, tr("About Qt"));
    });
}

// ── Navigation toolbar ───────────────────────────────────────────────

void BrowserWindow::createNavigationToolbar()
{
    m_navToolbar = addToolBar(tr("Navigation"));
    m_navToolbar->setMovable(false);
    m_navToolbar->setIconSize(QSize(18, 18));
    m_navToolbar->setContentsMargins(4, 2, 4, 2);

    const auto &c = ThemeEngine::instance().colors();

    // ── Navigation buttons ──────────────────────────────────────
    m_backAction = m_navToolbar->addAction(
        style()->standardIcon(QStyle::SP_ArrowBack), tr("Back"),
        this, &BrowserWindow::goBack);
    m_fwdAction = m_navToolbar->addAction(
        style()->standardIcon(QStyle::SP_ArrowForward), tr("Forward"),
        this, &BrowserWindow::goForward);

    // Reload / Stop — swap visibility based on loading state
    m_reloadAction = m_navToolbar->addAction(
        style()->standardIcon(QStyle::SP_BrowserReload), tr("Reload"),
        this, &BrowserWindow::reload);
    m_stopAction = m_navToolbar->addAction(
        style()->standardIcon(QStyle::SP_BrowserStop), tr("Stop"),
        this, [this]{
            auto *v = m_tabWidget ? m_tabWidget->currentWebView() : nullptr;
            if (v) v->stop();
        });
    m_stopAction->setVisible(false);

    m_homeAction = m_navToolbar->addAction(
        style()->standardIcon(QStyle::SP_DirHomeIcon), tr("Home"),
        this, &BrowserWindow::goHome);

    m_backAction->setShortcut(QKeySequence::Back);
    m_fwdAction->setShortcut(QKeySequence::Forward);
    m_reloadAction->setShortcut(QKeySequence::Refresh);
    m_stopAction->setShortcut(QKeySequence(QStringLiteral("Escape")));
    m_backAction->setToolTip(tr("Back (Alt+Left)"));
    m_fwdAction->setToolTip(tr("Forward (Alt+Right)"));
    m_reloadAction->setToolTip(tr("Reload (F5)"));
    m_homeAction->setToolTip(tr("Home (Alt+Home)"));

    // ── URL bar ─────────────────────────────────────────────────
    m_urlBar = new UrlBar(this);
    m_navToolbar->addWidget(m_urlBar);

    connect(m_urlBar, &UrlBar::urlEntered, this, &BrowserWindow::navigateTo);
    connect(m_urlBar, &UrlBar::searchRequested, this, &BrowserWindow::performSearch);

    // ── Bookmark star ───────────────────────────────────────────
    m_bookmarkStar = m_navToolbar->addAction(
        style()->standardIcon(QStyle::SP_DialogApplyButton),
        tr("Bookmark"), this, [this]{
            auto *v = m_tabWidget ? m_tabWidget->currentWebView() : nullptr;
            if (v && m_bookmarkManager)
                m_bookmarkManager->add(v->url().toString(), v->title());
        });
    m_bookmarkStar->setToolTip(tr("Bookmark this page (Ctrl+D)"));

    // ── Zoom indicator ──────────────────────────────────────────
    m_zoomLabel = new QLabel(this);
    m_zoomLabel->setFixedWidth(42);
    m_zoomLabel->setAlignment(Qt::AlignCenter);
    m_zoomLabel->setToolTip(tr("Zoom level — Ctrl+0 to reset"));
    m_zoomLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: 11px; padding: 0 2px; }")
        .arg(c.value(QStringLiteral("text-secondary"))));
    m_zoomLabel->hide(); // only show when zoom != 100%
    m_navToolbar->addWidget(m_zoomLabel);

    // ── Crabs button ────────────────────────────────────────────
    auto *crabsAction = m_navToolbar->addAction(
        style()->standardIcon(QStyle::SP_DriveNetIcon),
        tr("Crabs"), this, [this]{
            if (m_crabPanel) m_crabPanel->setVisible(!m_crabPanel->isVisible());
        });
    crabsAction->setToolTip(tr("Crabs Web Crawler (Ctrl+Shift+W)"));

    // ── Tab count badge ─────────────────────────────────────────
    m_tabCountLabel = new QLabel(this);
    m_tabCountLabel->setFixedSize(26, 20);
    m_tabCountLabel->setAlignment(Qt::AlignCenter);
    m_tabCountLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; background: %2; border: 1px solid %3; "
        "border-radius: 4px; font-size: 11px; font-weight: bold; }")
        .arg(c.value(QStringLiteral("text-primary")),
             c.value(QStringLiteral("bg-mid")),
             c.value(QStringLiteral("border"))));
    m_tabCountLabel->setToolTip(tr("Open tabs"));
    m_tabCountLabel->setText(QStringLiteral("1"));
    m_navToolbar->addWidget(m_tabCountLabel);

    connect(m_tabWidget, &TabWidget::tabCountChanged, this, [this](int count) {
        if (m_tabCountLabel) m_tabCountLabel->setText(QString::number(count));
    });

    // ── Menu button ─────────────────────────────────────────────
    m_menuButton = new QToolButton(this);
    m_menuButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMenuButton));
    m_menuButton->setPopupMode(QToolButton::InstantPopup);
    m_menuButton->setToolTip(tr("Menu"));

    auto *btnMenu = new QMenu(m_menuButton);
    btnMenu->addAction(tr("New Tab"),       this, [this]{ newTab(); });
    btnMenu->addAction(tr("Downloads"),     this, &BrowserWindow::showDownloads);
    btnMenu->addAction(tr("Bookmarks"),     this, &BrowserWindow::showBookmarks);
    btnMenu->addAction(tr("History"),       this, &BrowserWindow::showHistory);
    btnMenu->addSeparator();
    btnMenu->addAction(tr("Zoom In"),       this, &BrowserWindow::zoomIn);
    btnMenu->addAction(tr("Zoom Out"),      this, &BrowserWindow::zoomOut);
    btnMenu->addAction(tr("Reset Zoom"),    this, &BrowserWindow::resetZoom);
    btnMenu->addSeparator();
    btnMenu->addAction(tr("Settings"),      this, &BrowserWindow::showSettings);
    btnMenu->addAction(tr("Quit"),          this, &QWidget::close);
    m_menuButton->setMenu(btnMenu);

    m_navToolbar->addWidget(m_menuButton);

    // ── Loading progress bar (spans full toolbar width) ─────────
    m_loadProgress = new QProgressBar(this);
    m_loadProgress->setMaximumHeight(3);
    m_loadProgress->setTextVisible(false);
    m_loadProgress->setRange(0, 100);
    m_loadProgress->setValue(0);
    m_loadProgress->hide();
    m_loadProgress->setStyleSheet(QStringLiteral(
        "QProgressBar { background: transparent; border: none; }"
        "QProgressBar::chunk { background: %1; }")
        .arg(c.value(QStringLiteral("purple-mid"))));

    // Track page load progress from current tab
    auto updateLoadState = [this]() {
        auto *v = m_tabWidget ? m_tabWidget->currentWebView() : nullptr;
        if (!v) return;

        connect(v, &QWebEngineView::loadStarted, this, [this]{
            if (m_loadProgress) { m_loadProgress->setValue(0); m_loadProgress->show(); }
            if (m_reloadAction) m_reloadAction->setVisible(false);
            if (m_stopAction) m_stopAction->setVisible(true);
        }, Qt::UniqueConnection);

        connect(v, &QWebEngineView::loadProgress, this, [this](int p){
            if (m_loadProgress) m_loadProgress->setValue(p);
        }, Qt::UniqueConnection);

        connect(v, &QWebEngineView::loadFinished, this, [this]{
            if (m_loadProgress) m_loadProgress->hide();
            if (m_reloadAction) m_reloadAction->setVisible(true);
            if (m_stopAction) m_stopAction->setVisible(false);
            // Update zoom label
            auto *view = m_tabWidget ? m_tabWidget->currentWebView() : nullptr;
            if (view && m_zoomLabel) {
                int pct = qRound(view->zoomFactor() * 100);
                if (pct != 100) {
                    m_zoomLabel->setText(QStringLiteral("%1%").arg(pct));
                    m_zoomLabel->show();
                } else {
                    m_zoomLabel->hide();
                }
            }
        }, Qt::UniqueConnection);
    };

    connect(m_tabWidget, &QTabWidget::currentChanged, this, updateLoadState);
    updateLoadState();
}

// ── Status bar ───────────────────────────────────────────────────────

void BrowserWindow::createStatusBar()
{
    m_dbStatsLabel = new QLabel(tr("DB: ready"), this);
    statusBar()->addPermanentWidget(m_dbStatsLabel);

    auto dbStats = Database::instance()->getStats();
    const int siteCount = dbStats[QStringLiteral("site_count")].toInt();
    const int pageCount = dbStats[QStringLiteral("page_count")].toInt();
    statusBar()->showMessage(
        tr("OyNIx v3.1 ready — %1 sites indexed, %2 pages")
            .arg(siteCount).arg(pageCount),
        5000);
}

// ── Find bar ─────────────────────────────────────────────────────────

void BrowserWindow::createFindBar()
{
    m_findBar = new FindBar(this);
    m_findBar->hide();

    // The find bar is overlaid at the bottom of the central widget.
    // We parent it to `this` and reposition in event().
}

// ── Config I/O ───────────────────────────────────────────────────────

void BrowserWindow::loadConfig()
{
    const QString filePath = configPath() + QLatin1Char('/') + QLatin1String(kConfigFile);
    QFile f(filePath);
    if (f.open(QIODevice::ReadOnly)) {
        QJsonParseError err;
        QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
        if (err.error == QJsonParseError::NoError && doc.isObject())
            m_config = doc.object();
    }

    // Defaults
    if (!m_config.contains(QStringLiteral("homepage")))
        m_config[QStringLiteral("homepage")] = QStringLiteral("oyn://home");
    if (!m_config.contains(QStringLiteral("theme")))
        m_config[QStringLiteral("theme")] = QStringLiteral("obsidian");
    if (!m_config.contains(QStringLiteral("search_engine")))
        m_config[QStringLiteral("search_engine")] = QStringLiteral("DuckDuckGo");
    if (!m_config.contains(QStringLiteral("search_mode")))
        m_config[QStringLiteral("search_mode")] = QStringLiteral("hybrid");

    // Apply search mode
    const bool useWeb = m_config[QStringLiteral("search_mode")].toString() != QLatin1String("internal");
    NyxSearch::instance()->setUseWebSearch(useWeb);
}

void BrowserWindow::saveConfig()
{
    const QString dirPath = configPath();
    QDir().mkpath(dirPath);

    const QString filePath = dirPath + QLatin1Char('/') + QLatin1String(kConfigFile);
    QFile f(filePath);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        f.write(QJsonDocument(m_config).toJson(QJsonDocument::Indented));
    }
}

// ── Theme ────────────────────────────────────────────────────────────

void BrowserWindow::applyTheme()
{
    const QString themeName = m_config.value(QStringLiteral("theme"))
                                  .toString(QStringLiteral("obsidian"));
    ThemeEngine::instance().loadTheme(themeName);
    setStyleSheet(ThemeEngine::instance().getQtStylesheet());
}

void BrowserWindow::setTheme(const QString &themeName)
{
    m_config[QStringLiteral("theme")] = themeName;
    applyTheme();
    emit themeChanged(themeName);
}

// ── Internal URL handling ────────────────────────────────────────────

void BrowserWindow::handleInternalUrl(const QUrl &url)
{
    auto *view = m_tabWidget ? m_tabWidget->currentWebView() : nullptr;
    if (!view) return;

    const auto &colors = ThemeEngine::instance().colors();
    const QString host = url.host();

    // Helper: load internal HTML safely. We use about:blank as the base URL
    // to prevent WebPage::acceptNavigationRequest from intercepting the
    // setHtml navigation (which would cancel it and leave a blank page).
    // The URL bar is updated explicitly.
    auto loadInternal = [&](const QString &html, const QUrl &displayUrl) {
        view->setHtml(html, QUrl(QStringLiteral("about:blank")));
        if (m_urlBar) m_urlBar->setDisplayUrl(displayUrl);
    };

    if (host == QLatin1String("home")) {
        const auto stats = NyxSearch::instance()->getStats();
        const QJsonArray recent = m_historyManager ? m_historyManager->getRecent(8) : QJsonArray();
        const QJsonArray bookmarks = m_bookmarkManager ? m_bookmarkManager->getAll() : QJsonArray();
        loadInternal(InternalPages::homePage(colors, stats, recent, bookmarks),
                     QUrl(QStringLiteral("oyn://home")));
        return;
    }

    if (host == QLatin1String("search")) {
        QUrlQuery query(url);
        const QString q = query.queryItemValue(QStringLiteral("q"));
        if (q.isEmpty()) {
            handleInternalUrl(QUrl(QStringLiteral("oyn://home")));
            return;
        }
        auto searchResult = NyxSearch::instance()->search(q);
        QJsonArray localResults = searchResult[QStringLiteral("local")].toArray();
        QJsonArray dbResults = searchResult[QStringLiteral("database")].toArray();
        for (const auto &v : dbResults)
            localResults.append(v);

        const QUrl displayUrl(QStringLiteral("oyn://search?q=") + QUrl::toPercentEncoding(q));
        loadInternal(InternalPages::searchPage(q, localResults, QJsonArray(), colors), displayUrl);

        // Async web results — when they arrive, refresh the page
        auto *nyx = NyxSearch::instance();
        auto conn = std::make_shared<QMetaObject::Connection>();
        *conn = connect(nyx, &NyxSearch::webResultsReady, this,
                        [this, q, localResults, colors, conn, displayUrl](const QJsonArray &webResults) {
            disconnect(*conn);
            auto *v = m_tabWidget ? m_tabWidget->currentWebView() : nullptr;
            if (!v) return;
            const QString updatedHtml = InternalPages::searchPage(q, localResults, webResults, colors);
            v->setHtml(updatedHtml, QUrl(QStringLiteral("about:blank")));
        });
        return;
    }

    if (host == QLatin1String("bookmarks")) {
        const QJsonArray bookmarks = m_bookmarkManager ? m_bookmarkManager->getAll() : QJsonArray();
        loadInternal(InternalPages::bookmarksPage(bookmarks, colors),
                     QUrl(QStringLiteral("oyn://bookmarks")));
        return;
    }

    if (host == QLatin1String("history")) {
        const QJsonArray history = m_historyManager ? m_historyManager->getAll() : QJsonArray();
        loadInternal(InternalPages::historyPage(history, colors),
                     QUrl(QStringLiteral("oyn://history")));
        return;
    }

    if (host == QLatin1String("downloads")) {
        loadInternal(InternalPages::downloadsPage(QJsonArray(), colors),
                     QUrl(QStringLiteral("oyn://downloads")));
        return;
    }

    if (host == QLatin1String("settings")) {
        loadInternal(InternalPages::settingsPage(m_config, colors),
                     QUrl(QStringLiteral("oyn://settings")));
        return;
    }

    if (host == QLatin1String("profiles")) {
        QJsonArray profiles;
        QString activeProfile = QStringLiteral("Default");
        QJsonObject def;
        def[QStringLiteral("name")] = QStringLiteral("Default");
        profiles.append(def);
        loadInternal(InternalPages::profilesPage(profiles, activeProfile, colors),
                     QUrl(QStringLiteral("oyn://profiles")));
        return;
    }

    if (host == QLatin1String("setting")) {
        QUrlQuery query(url);
        const auto items = query.queryItems();
        for (const auto &item : items) {
            m_config[item.first] = (item.second == QLatin1String("true"));
        }
        saveConfig();
        handleInternalUrl(QUrl(QStringLiteral("oyn://settings")));
        return;
    }
}

// ── Tab management ───────────────────────────────────────────────────

void BrowserWindow::newTab(const QUrl &url)
{
    if (!m_tabWidget) return;

    const QUrl target = url.isEmpty() ? QUrl(m_config.value(QStringLiteral("homepage"))
                                                     .toString(QStringLiteral("oyn://home")))
                                      : url;

    m_tabWidget->addNewTab(target);
}

void BrowserWindow::closeTab(int index)
{
    if (!m_tabWidget) return;
    const int idx = (index < 0) ? m_tabWidget->currentIndex() : index;
    if (idx < 0) return;

    auto *w = m_tabWidget->widget(idx);
    m_tabWidget->removeTab(idx);
    if (w) w->deleteLater();

    if (m_tabWidget->count() == 0)
        close();
}

// ── Navigation slots ─────────────────────────────────────────────────

void BrowserWindow::navigateTo(const QUrl &url)
{
    if (url.scheme() == QLatin1String("oyn")) {
        handleInternalUrl(url);
        return;
    }

    auto *view = m_tabWidget ? m_tabWidget->currentWebView() : nullptr;
    if (view)
        view->load(url);
}

void BrowserWindow::goHome()
{
    navigateTo(QUrl(m_config.value(QStringLiteral("homepage"))
                           .toString(QStringLiteral("oyn://home"))));
}

void BrowserWindow::goBack()
{
    auto *v = m_tabWidget ? m_tabWidget->currentWebView() : nullptr;
    if (v) v->back();
}

void BrowserWindow::goForward()
{
    auto *v = m_tabWidget ? m_tabWidget->currentWebView() : nullptr;
    if (v) v->forward();
}

void BrowserWindow::reload()
{
    auto *v = m_tabWidget ? m_tabWidget->currentWebView() : nullptr;
    if (v) v->reload();
}

// ── Page slots ───────────────────────────────────────────────────────

void BrowserWindow::showBookmarks()
{
    handleInternalUrl(QUrl(QStringLiteral("oyn://bookmarks")));
}

void BrowserWindow::showHistory()
{
    handleInternalUrl(QUrl(QStringLiteral("oyn://history")));
}

void BrowserWindow::showDownloads()
{
    if (m_downloadManager)
        m_downloadManager->setVisible(!m_downloadManager->isVisible());
}

void BrowserWindow::showSettings()
{
    SettingsDialog dlg(m_config, this);
    connect(&dlg, &SettingsDialog::themeChangeRequested, this, &BrowserWindow::setTheme);
    if (dlg.exec() == QDialog::Accepted) {
        m_config = dlg.updatedConfig();
        saveConfig();
        applyTheme();
        if (m_treeTabSidebar)
            m_treeTabSidebar->setVisible(m_config[QStringLiteral("tree_tabs")].toBool());
        // Apply search mode
        const bool useWeb = m_config[QStringLiteral("search_mode")].toString() != QLatin1String("internal");
        NyxSearch::instance()->setUseWebSearch(useWeb);
    }
}

// ── Zoom ─────────────────────────────────────────────────────────────

void BrowserWindow::zoomIn()
{
    auto *v = m_tabWidget ? m_tabWidget->currentWebView() : nullptr;
    if (v) {
        v->setZoomFactor(v->zoomFactor() + 0.1);
        int pct = qRound(v->zoomFactor() * 100);
        if (m_zoomLabel) {
            m_zoomLabel->setText(QStringLiteral("%1%").arg(pct));
            m_zoomLabel->setVisible(pct != 100);
        }
    }
}

void BrowserWindow::zoomOut()
{
    auto *v = m_tabWidget ? m_tabWidget->currentWebView() : nullptr;
    if (v) {
        v->setZoomFactor(qMax(0.25, v->zoomFactor() - 0.1));
        int pct = qRound(v->zoomFactor() * 100);
        if (m_zoomLabel) {
            m_zoomLabel->setText(QStringLiteral("%1%").arg(pct));
            m_zoomLabel->setVisible(pct != 100);
        }
    }
}

void BrowserWindow::resetZoom()
{
    auto *v = m_tabWidget ? m_tabWidget->currentWebView() : nullptr;
    if (v) {
        v->setZoomFactor(1.0);
        if (m_zoomLabel) m_zoomLabel->hide();
    }
}

void BrowserWindow::toggleFullscreen()
{
    if (isFullScreen())
        showNormal();
    else
        showFullScreen();
}

// ── Search ───────────────────────────────────────────────────────────

void BrowserWindow::performSearch(const QString &query)
{
    // Route all searches through the built-in Nyx search engine
    QUrl searchUrl(QStringLiteral("oyn://search"));
    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("q"), query);
    searchUrl.setQuery(urlQuery);
    handleInternalUrl(searchUrl);
}

// ── About ────────────────────────────────────────────────────────────

void BrowserWindow::showAbout()
{
    QMessageBox::about(this, tr("About OyNIx Browser"),
                       tr("<h2>OyNIx Browser v3.1</h2>"
                          "<p>Chromium-based desktop browser built with "
                          "Qt6 WebEngine, C++17, and .NET 8 (C#) core.</p>"
                          "<p>&copy; 2026 OyNIx Project</p>"));
}

// ── Utility ──────────────────────────────────────────────────────────

void BrowserWindow::updateWindowTitle(const QString &title)
{
    setWindowTitle(title.isEmpty() ? QStringLiteral("OyNIx Browser")
                                   : title + QStringLiteral(" — OyNIx"));
}

void BrowserWindow::updateSecurityIndicator(const QUrl &url)
{
    if (!m_urlBar) return;

    if (url.scheme() == QLatin1String("https"))
        m_urlBar->setSecurityIcon(QStringLiteral("lock"));
    else if (url.scheme() == QLatin1String("oyn"))
        m_urlBar->setSecurityIcon(QStringLiteral("internal"));
    else
        m_urlBar->setSecurityIcon(QStringLiteral("warning"));
}

void BrowserWindow::updateDbStats()
{
    if (!m_dbStatsLabel) return;

    auto *db = Database::instance();
    auto stats = db->getStats();
    const int sites = stats[QStringLiteral("site_count")].toInt();
    const int pages = stats[QStringLiteral("page_count")].toInt();
    auto cacheStats = WebCache::instance().getStats();
    const int cached = cacheStats[QStringLiteral("cached_pages")].toInt();
    const double cacheMb = cacheStats[QStringLiteral("cache_size_mb")].toDouble();

    m_dbStatsLabel->setText(
        tr("DB: %1 sites, %2 pages | Cache: %3 pages (%4 MB)")
            .arg(sites).arg(pages).arg(cached).arg(cacheMb, 0, 'f', 1));
}

// ── Download handling ────────────────────────────────────────────
void BrowserWindow::handleDownload(QWebEngineDownloadRequest *download)
{
    if (!m_downloadManager) return;
    m_downloadManager->addDownload(download);
    m_downloadManager->show();
}

// ── Security check ──────────────────────────────────────────────────
void BrowserWindow::handleSecurityCheck(const QUrl &url)
{
    auto &sec = SecurityManager::instance();
    if (sec.shouldPrompt(url)) {
        const auto info = sec.securityInfo(url);
        const auto cat = info[QStringLiteral("category")].toString();
        const auto result = QMessageBox::warning(this,
            QStringLiteral("Security Alert"),
            QStringLiteral("You are navigating to a %1 login page:\n%2\n\n"
                "Make sure this is the genuine site before entering credentials.")
                .arg(cat, url.toString()),
            QMessageBox::Ok | QMessageBox::Cancel);
        if (result == QMessageBox::Cancel) {
            // Navigate back
            goBack();
        }
    }
}

// ── Command palette registration ─────────────────────────────────────
void BrowserWindow::registerCommands()
{
    if (!m_commandPalette) return;

    using Cmd = CommandPalette::Command;

    m_commandPalette->registerCommand({QStringLiteral("new_tab"),
        QStringLiteral("New Tab"), QStringLiteral("Open a new browser tab"),
        QStringLiteral("Ctrl+T"), QStringLiteral("Navigation"),
        [this]{ newTab(); }});

    m_commandPalette->registerCommand({QStringLiteral("close_tab"),
        QStringLiteral("Close Tab"), QStringLiteral("Close the current tab"),
        QStringLiteral("Ctrl+W"), QStringLiteral("Navigation"),
        [this]{ closeTab(); }});

    m_commandPalette->registerCommand({QStringLiteral("go_home"),
        QStringLiteral("Go Home"), QStringLiteral("Navigate to homepage"),
        QStringLiteral("Alt+Home"), QStringLiteral("Navigation"),
        [this]{ goHome(); }});

    m_commandPalette->registerCommand({QStringLiteral("go_back"),
        QStringLiteral("Go Back"), QStringLiteral("Navigate back"),
        QStringLiteral("Alt+Left"), QStringLiteral("Navigation"),
        [this]{ goBack(); }});

    m_commandPalette->registerCommand({QStringLiteral("go_forward"),
        QStringLiteral("Go Forward"), QStringLiteral("Navigate forward"),
        QStringLiteral("Alt+Right"), QStringLiteral("Navigation"),
        [this]{ goForward(); }});

    m_commandPalette->registerCommand({QStringLiteral("reload"),
        QStringLiteral("Reload Page"), QStringLiteral("Reload the current page"),
        QStringLiteral("F5"), QStringLiteral("Navigation"),
        [this]{ reload(); }});

    m_commandPalette->registerCommand({QStringLiteral("bookmarks"),
        QStringLiteral("Bookmarks"), QStringLiteral("Show bookmarks panel"),
        QStringLiteral("Ctrl+B"), QStringLiteral("Tools"),
        [this]{ showBookmarks(); }});

    m_commandPalette->registerCommand({QStringLiteral("history"),
        QStringLiteral("History"), QStringLiteral("Show browsing history"),
        QString(), QStringLiteral("Tools"),
        [this]{ showHistory(); }});

    m_commandPalette->registerCommand({QStringLiteral("downloads"),
        QStringLiteral("Downloads"), QStringLiteral("Show downloads"),
        QString(), QStringLiteral("Tools"),
        [this]{ showDownloads(); }});

    m_commandPalette->registerCommand({QStringLiteral("settings"),
        QStringLiteral("Settings"), QStringLiteral("Open browser settings"),
        QString(), QStringLiteral("Tools"),
        [this]{ showSettings(); }});

    m_commandPalette->registerCommand({QStringLiteral("ai_panel"),
        QStringLiteral("Toggle AI Assistant"), QStringLiteral("Show/hide Nyx AI panel"),
        QStringLiteral("Ctrl+Shift+A"), QStringLiteral("AI"),
        [this]{ if (m_aiPanel) m_aiPanel->togglePanel(); }});

    m_commandPalette->registerCommand({QStringLiteral("audio_player"),
        QStringLiteral("Audio Player"), QStringLiteral("Show/hide audio player"),
        QString(), QStringLiteral("Tools"),
        [this]{ if (m_audioPlayer) m_audioPlayer->setVisible(!m_audioPlayer->isVisible()); }});

    m_commandPalette->registerCommand({QStringLiteral("tree_tabs"),
        QStringLiteral("Toggle Tree Tabs"), QStringLiteral("Show/hide tree tab sidebar"),
        QStringLiteral("Ctrl+Shift+T"), QStringLiteral("View"),
        [this]{ if (m_treeTabSidebar) m_treeTabSidebar->setVisible(!m_treeTabSidebar->isVisible()); }});

    m_commandPalette->registerCommand({QStringLiteral("fullscreen"),
        QStringLiteral("Toggle Fullscreen"), QStringLiteral("Enter/exit fullscreen"),
        QStringLiteral("F11"), QStringLiteral("View"),
        [this]{ toggleFullscreen(); }});

    m_commandPalette->registerCommand({QStringLiteral("zoom_in"),
        QStringLiteral("Zoom In"), QStringLiteral("Increase page zoom"),
        QStringLiteral("Ctrl++"), QStringLiteral("View"),
        [this]{ zoomIn(); }});

    m_commandPalette->registerCommand({QStringLiteral("zoom_out"),
        QStringLiteral("Zoom Out"), QStringLiteral("Decrease page zoom"),
        QStringLiteral("Ctrl+-"), QStringLiteral("View"),
        [this]{ zoomOut(); }});

    m_commandPalette->registerCommand({QStringLiteral("find"),
        QStringLiteral("Find in Page"), QStringLiteral("Search text in page"),
        QStringLiteral("Ctrl+F"), QStringLiteral("Edit"),
        [this]{ if (m_findBar) m_findBar->showBar(); }});

    m_commandPalette->registerCommand({QStringLiteral("about"),
        QStringLiteral("About OyNIx"), QStringLiteral("Show about dialog"),
        QString(), QStringLiteral("Help"),
        [this]{ showAbout(); }});

    m_commandPalette->registerCommand({QStringLiteral("crabs"),
        QStringLiteral("Crabs"), QStringLiteral("Show/hide Crabs web crawler panel"),
        QStringLiteral("Ctrl+Shift+W"), QStringLiteral("Tools"),
        [this]{ if (m_crabPanel) m_crabPanel->setVisible(!m_crabPanel->isVisible()); }});

    m_commandPalette->registerCommand({QStringLiteral("extensions"),
        QStringLiteral("Extensions"), QStringLiteral("Show/hide extensions panel"),
        QString(), QStringLiteral("Tools"),
        [this]{ if (m_extensionPanel) m_extensionPanel->setVisible(!m_extensionPanel->isVisible()); }});

    m_commandPalette->registerCommand({QStringLiteral("profiles"),
        QStringLiteral("Profiles"), QStringLiteral("Show profiles page"),
        QString(), QStringLiteral("Tools"),
        [this]{ handleInternalUrl(QUrl(QStringLiteral("oyn://profiles"))); }});

    m_commandPalette->registerCommand({QStringLiteral("reset_zoom"),
        QStringLiteral("Reset Zoom"), QStringLiteral("Reset page zoom to 100%%"),
        QStringLiteral("Ctrl+0"), QStringLiteral("View"),
        [this]{ resetZoom(); }});

    m_commandPalette->registerCommand({QStringLiteral("focus_url"),
        QStringLiteral("Focus URL Bar"), QStringLiteral("Focus the address bar"),
        QStringLiteral("Ctrl+L"), QStringLiteral("Navigation"),
        [this]{ if (m_urlBar) { m_urlBar->setFocus(); m_urlBar->selectAll(); } }});

    m_commandPalette->registerCommand({QStringLiteral("bookmark_page"),
        QStringLiteral("Bookmark Page"), QStringLiteral("Add current page to bookmarks"),
        QStringLiteral("Ctrl+D"), QStringLiteral("Tools"),
        [this]{
            auto *v = m_tabWidget ? m_tabWidget->currentWebView() : nullptr;
            if (v && m_bookmarkManager)
                m_bookmarkManager->add(v->url().toString(), v->title());
        }});

    m_commandPalette->registerCommand({QStringLiteral("summarize"),
        QStringLiteral("Summarize Page"), QStringLiteral("AI summarize current page"),
        QString(), QStringLiteral("AI"),
        [this]{
            auto *view = m_tabWidget ? m_tabWidget->currentWebView() : nullptr;
            if (view) {
                view->page()->toPlainText([this, view](const QString &text) {
                    AiManager::instance().summarizePage(text, view->title());
                });
                if (m_aiPanel && !m_aiPanel->isPanelVisible()) m_aiPanel->showPanel();
            }
        }});

    m_commandPalette->registerCommand({QStringLiteral("clear_data"),
        QStringLiteral("Clear Browsing Data"), QStringLiteral("Clear history, cache, cookies"),
        QString(), QStringLiteral("Privacy"),
        [this]{
            if (m_historyManager) m_historyManager->clear();
            QWebEngineProfile::defaultProfile()->clearHttpCache();
            WebCache::instance().clear();
            updateDbStats();
        }});
}

// ── Events ───────────────────────────────────────────────────────────

void BrowserWindow::closeEvent(QCloseEvent *event)
{
    saveConfig();
    event->accept();
}

bool BrowserWindow::event(QEvent *event)
{
    if (event->type() == QEvent::Resize) {
        // Reposition loading progress bar below toolbar
        if (m_loadProgress && m_navToolbar) {
            const int y = m_navToolbar->geometry().bottom();
            m_loadProgress->setGeometry(0, y, width(), 3);
            m_loadProgress->raise();
        }
        // Reposition find bar at the bottom
        if (m_findBar && m_findBar->isVisible()) {
            const int barH = m_findBar->sizeHint().height();
            m_findBar->setGeometry(0, height() - barH - statusBar()->height(),
                                   width(), barH);
        }
    }
    return QMainWindow::event(event);
}
