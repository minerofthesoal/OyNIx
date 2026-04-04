#pragma once

#include <QMainWindow>
#include <QUrl>
#include <QJsonObject>

class QAction;
class QActionGroup;
class QLabel;
class QMenu;
class QToolBar;
class QToolButton;
class QLineEdit;
class QComboBox;

// OyNIx forward declarations
class TabWidget;
class UrlBar;
class FindBar;
class ThemeEngine;
class Database;
class BookmarkManager;
class HistoryManager;
class NyxSearch;
class ExtensionManager;
class ProfileManager;
class GitHubSync;
class AiChatPanel;
class CommandPalette;
class DownloadManager;
class AudioPlayer;
class TreeTabSidebar;
class BookmarkPanel;
class SessionManager;
class SecurityManager;
class ExtensionPanel;
class QSplitter;
class QWebEngineDownloadRequest;

class BrowserWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit BrowserWindow(QWidget *parent = nullptr);
    ~BrowserWindow() override;

    [[nodiscard]] QString configPath() const;
    [[nodiscard]] QJsonObject config() const { return m_config; }

public slots:
    void newTab(const QUrl &url = QUrl());
    void closeTab(int index = -1);
    void navigateTo(const QUrl &url);
    void goHome();
    void goBack();
    void goForward();
    void reload();

    void showBookmarks();
    void showHistory();
    void showDownloads();
    void showSettings();

    void zoomIn();
    void zoomOut();
    void resetZoom();
    void toggleFullscreen();

    void setTheme(const QString &themeName);
    void showAbout();
    void performSearch(const QString &query);

signals:
    void themeChanged(const QString &themeName);

protected:
    void closeEvent(QCloseEvent *event) override;
    bool event(QEvent *event) override;

private:
    void createMenuBar();
    void createNavigationToolbar();
    void createStatusBar();
    void createFindBar();
    void createSidePanels();
    void registerCommands();

    void loadConfig();
    void saveConfig();
    void applyTheme();
    void handleInternalUrl(const QUrl &url);
    void handleDownload(QWebEngineDownloadRequest *download);
    void handleSecurityCheck(const QUrl &url);
    void updateWindowTitle(const QString &title);
    void updateSecurityIndicator(const QUrl &url);
    void updateDbStats();

    // Menus
    QMenu *m_fileMenu       = nullptr;
    QMenu *m_editMenu       = nullptr;
    QMenu *m_viewMenu       = nullptr;
    QMenu *m_searchMenu     = nullptr;
    QMenu *m_toolsMenu      = nullptr;
    QMenu *m_aiMenu         = nullptr;
    QMenu *m_developerMenu  = nullptr;
    QMenu *m_helpMenu       = nullptr;

    // Navigation
    QToolBar  *m_navToolbar  = nullptr;
    QAction   *m_backAction  = nullptr;
    QAction   *m_fwdAction   = nullptr;
    QAction   *m_reloadAction = nullptr;
    QAction   *m_homeAction  = nullptr;
    UrlBar    *m_urlBar      = nullptr;
    QAction   *m_bookmarkStar = nullptr;
    QToolButton *m_menuButton = nullptr;

    // Central
    TabWidget *m_tabWidget   = nullptr;
    FindBar   *m_findBar     = nullptr;

    // Status
    QLabel *m_dbStatsLabel   = nullptr;

    // Config
    QJsonObject m_config;

    // Side panels
    QSplitter        *m_splitter         = nullptr;
    AiChatPanel      *m_aiPanel          = nullptr;
    TreeTabSidebar   *m_treeTabSidebar   = nullptr;
    BookmarkPanel    *m_bookmarkPanel    = nullptr;
    ExtensionPanel   *m_extensionPanel   = nullptr;
    DownloadManager  *m_downloadManager  = nullptr;
    AudioPlayer      *m_audioPlayer      = nullptr;
    CommandPalette   *m_commandPalette   = nullptr;
    SessionManager   *m_sessionManager   = nullptr;

    // Sub-systems (owned, lazily created)
    ThemeEngine      *m_themeEngine      = nullptr;
    Database         *m_database         = nullptr;
    BookmarkManager  *m_bookmarkManager  = nullptr;
    HistoryManager   *m_historyManager   = nullptr;
    NyxSearch        *m_nyxSearch        = nullptr;
    ExtensionManager *m_extensionManager = nullptr;
    ProfileManager   *m_profileManager   = nullptr;
    GitHubSync       *m_githubSync       = nullptr;
};
