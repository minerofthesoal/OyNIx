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
#include "search/NyxSearch.h"

#include <QSplitter>
#include <QWebEngineProfile>
#include <QWebEngineDownloadRequest>

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

    // Initialize data subsystems
    m_bookmarkManager  = new BookmarkManager(configPath(), this);
    m_sessionManager   = new SessionManager(this);
    m_extensionManager = new ExtensionManager(this);

    // Central layout: tree-tabs | tab-widget | ai-panel
    m_splitter = new QSplitter(Qt::Horizontal, this);

    // Tree tab sidebar (left)
    m_tabWidget = new TabWidget(this);
    m_treeTabSidebar = new TreeTabSidebar(m_tabWidget, this);
    m_treeTabSidebar->setVisible(m_config[QStringLiteral("tree_tabs")].toBool());

    // AI chat panel (right)
    m_aiPanel = new AiChatPanel(this);

    m_splitter->addWidget(m_treeTabSidebar);
    m_splitter->addWidget(m_tabWidget);
    m_splitter->addWidget(m_aiPanel);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setStretchFactor(2, 0);
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

    // Download handling
    connect(QWebEngineProfile::defaultProfile(), &QWebEngineProfile::downloadRequested,
            this, &BrowserWindow::handleDownload);

    // Tree tab sidebar connections
    connect(m_treeTabSidebar, &TreeTabSidebar::newTabRequested, this, [this]() { newTab(); });
    connect(m_treeTabSidebar, &TreeTabSidebar::closeTabRequested, this, &BrowserWindow::closeTab);

    // Bookmark panel connections
    connect(m_bookmarkPanel, &BookmarkPanel::bookmarkActivated, this, &BrowserWindow::navigateTo);

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
                NyxSearch::instance()->indexPage(
                    url.toString(), view->title(), QString());
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

    // Shortcuts
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+T")), this, [this]{ newTab(); });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+W")), this, [this]{ closeTab(); });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+L")), this, [this]{
        if (m_urlBar) { m_urlBar->setFocus(); m_urlBar->selectAll(); }
    });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+F")), this, [this]{
        if (m_findBar) m_findBar->showBar();
    });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+K")), this, [this]{
        if (m_commandPalette) m_commandPalette->toggle();
    });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+A")), this, [this]{
        if (m_aiPanel) m_aiPanel->togglePanel();
    });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+D")), this, [this]{
        auto *v = m_tabWidget ? m_tabWidget->currentWebView() : nullptr;
        if (v && m_bookmarkManager)
            m_bookmarkManager->add(v->url().toString(), v->title());
    });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+B")), this, [this]{
        if (m_bookmarkPanel) m_bookmarkPanel->setVisible(!m_bookmarkPanel->isVisible());
    });
    new QShortcut(QKeySequence(QStringLiteral("F11")), this, [this]{ toggleFullscreen(); });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl++")), this, [this]{ zoomIn(); });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+-")), this, [this]{ zoomOut(); });
    new QShortcut(QKeySequence(QStringLiteral("Ctrl+0")), this, [this]{ resetZoom(); });
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
        QStringLiteral("Nyx Dark"),
        QStringLiteral("Midnight"),
        QStringLiteral("Aether Light"),
        QStringLiteral("Solarized"),
        QStringLiteral("System"),
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
    m_toolsMenu->addAction(tr("Extensions"),      this, []{ /* placeholder */ });
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
    m_navToolbar->setIconSize(QSize(20, 20));

    m_backAction   = m_navToolbar->addAction(style()->standardIcon(QStyle::SP_ArrowBack),
                                              tr("Back"), this, &BrowserWindow::goBack);
    m_fwdAction    = m_navToolbar->addAction(style()->standardIcon(QStyle::SP_ArrowForward),
                                              tr("Forward"), this, &BrowserWindow::goForward);
    m_reloadAction = m_navToolbar->addAction(style()->standardIcon(QStyle::SP_BrowserReload),
                                              tr("Reload"), this, &BrowserWindow::reload);
    m_homeAction   = m_navToolbar->addAction(style()->standardIcon(QStyle::SP_DirHomeIcon),
                                              tr("Home"), this, &BrowserWindow::goHome);

    m_backAction->setShortcut(QKeySequence::Back);
    m_fwdAction->setShortcut(QKeySequence::Forward);
    m_reloadAction->setShortcut(QKeySequence::Refresh);

    // URL bar
    m_urlBar = new UrlBar(this);
    m_navToolbar->addWidget(m_urlBar);

    connect(m_urlBar, &UrlBar::urlEntered, this, &BrowserWindow::navigateTo);
    connect(m_urlBar, &UrlBar::searchRequested, this, &BrowserWindow::performSearch);

    // Bookmark star
    m_bookmarkStar = m_navToolbar->addAction(
        style()->standardIcon(QStyle::SP_DialogApplyButton),
        tr("Bookmark"), this, [this]{
            // placeholder: toggle bookmark for current page
        });

    // Extensions area (spacer + placeholder)
    auto *spacer = new QWidget(this);
    spacer->setFixedWidth(4);
    m_navToolbar->addWidget(spacer);

    // Menu button
    m_menuButton = new QToolButton(this);
    m_menuButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarMenuButton));
    m_menuButton->setPopupMode(QToolButton::InstantPopup);

    auto *btnMenu = new QMenu(m_menuButton);
    btnMenu->addAction(tr("New Tab"),   this, [this]{ newTab(); });
    btnMenu->addAction(tr("Settings"),  this, &BrowserWindow::showSettings);
    btnMenu->addSeparator();
    btnMenu->addAction(tr("Quit"),      this, &QWidget::close);
    m_menuButton->setMenu(btnMenu);

    m_navToolbar->addWidget(m_menuButton);
}

// ── Status bar ───────────────────────────────────────────────────────

void BrowserWindow::createStatusBar()
{
    m_dbStatsLabel = new QLabel(tr("DB: ready"), this);
    statusBar()->addPermanentWidget(m_dbStatsLabel);

    auto dbStats = Database::instance()->getStats();
    const int siteCount = dbStats[QStringLiteral("site_count")].toInt();
    statusBar()->showMessage(
        tr("OyNIx Browser is ready! Features: Tree Tabs | Nyx Search | Local AI | %1 Sites")
            .arg(siteCount > 0 ? QString::number(siteCount) : QStringLiteral("0")),
        6000);
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
        m_config[QStringLiteral("theme")] = QStringLiteral("Nyx Dark");
    if (!m_config.contains(QStringLiteral("search_engine")))
        m_config[QStringLiteral("search_engine")] = QStringLiteral("DuckDuckGo");
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
    // Nyx Dark default inline stylesheet (ThemeEngine can override later)
    const QString sheet = QStringLiteral(
        "QMainWindow { background-color: #08080d; }"
        "QMenuBar { background-color: #0e0e16; color: #E8E0F0; }"
        "QMenuBar::item:selected { background-color: #7B4FBF; }"
        "QMenu { background-color: #0e0e16; color: #E8E0F0; border: 1px solid #7B4FBF; }"
        "QMenu::item:selected { background-color: #7B4FBF; }"
        "QToolBar { background-color: #0e0e16; border: none; spacing: 4px; }"
        "QStatusBar { background-color: #0e0e16; color: #A8A0B8; }"
        "QTabWidget::pane { border: 1px solid #7B4FBF; }"
        "QTabBar::tab { background: #0e0e16; color: #E8E0F0; padding: 6px 14px;"
        "              border: 1px solid #7B4FBF; border-bottom: none; border-radius: 4px 4px 0 0; }"
        "QTabBar::tab:selected { background: #7B4FBF; }"
        "QTabBar::tab:hover { background: #9B6FDF; }"
    );
    setStyleSheet(sheet);
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
    const QString host = url.host();

    if (host == QLatin1String("home"))          { /* load home page */      return; }
    if (host == QLatin1String("bookmarks"))     { showBookmarks();          return; }
    if (host == QLatin1String("history"))       { showHistory();            return; }
    if (host == QLatin1String("downloads"))     { showDownloads();          return; }
    if (host == QLatin1String("settings"))      { showSettings();           return; }
    if (host == QLatin1String("search"))        { /* open search page */    return; }
    if (host == QLatin1String("profiles"))      { /* open profiles page */  return; }
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
    if (m_bookmarkPanel)
        m_bookmarkPanel->setVisible(!m_bookmarkPanel->isVisible());
}

void BrowserWindow::showHistory()    { navigateTo(QUrl(QStringLiteral("oyn://history")));   }

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
    }
}

// ── Zoom ─────────────────────────────────────────────────────────────

void BrowserWindow::zoomIn()
{
    auto *v = m_tabWidget ? m_tabWidget->currentWebView() : nullptr;
    if (v) v->setZoomFactor(v->zoomFactor() + 0.1);
}

void BrowserWindow::zoomOut()
{
    auto *v = m_tabWidget ? m_tabWidget->currentWebView() : nullptr;
    if (v) v->setZoomFactor(qMax(0.25, v->zoomFactor() - 0.1));
}

void BrowserWindow::resetZoom()
{
    auto *v = m_tabWidget ? m_tabWidget->currentWebView() : nullptr;
    if (v) v->setZoomFactor(1.0);
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
    const QString engine = m_config.value(QStringLiteral("search_engine"))
                                   .toString(QStringLiteral("DuckDuckGo"));

    QUrl searchUrl;
    if (engine == QLatin1String("Google")) {
        searchUrl = QUrl(QStringLiteral("https://www.google.com/search"));
    } else if (engine == QLatin1String("Bing")) {
        searchUrl = QUrl(QStringLiteral("https://www.bing.com/search"));
    } else {
        searchUrl = QUrl(QStringLiteral("https://duckduckgo.com/"));
    }

    QUrlQuery urlQuery;
    urlQuery.addQueryItem(QStringLiteral("q"), query);
    searchUrl.setQuery(urlQuery);

    navigateTo(searchUrl);
}

// ── About ────────────────────────────────────────────────────────────

void BrowserWindow::showAbout()
{
    QMessageBox::about(this, tr("About OyNIx Browser"),
                       tr("<h2>OyNIx Browser v3.0</h2>"
                          "<p>A Chromium-based desktop browser built with "
                          "Qt6 WebEngine and C++17.</p>"
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
    m_dbStatsLabel->setText(tr("DB: %1 sites, %2 pages").arg(sites).arg(pages));
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
}

// ── Events ───────────────────────────────────────────────────────────

void BrowserWindow::closeEvent(QCloseEvent *event)
{
    saveConfig();
    event->accept();
}

bool BrowserWindow::event(QEvent *event)
{
    // Reposition find bar when the window is resized
    if (event->type() == QEvent::Resize && m_findBar && m_findBar->isVisible()) {
        const int barH = m_findBar->sizeHint().height();
        m_findBar->setGeometry(0, height() - barH - statusBar()->height(),
                               width(), barH);
    }
    return QMainWindow::event(event);
}
