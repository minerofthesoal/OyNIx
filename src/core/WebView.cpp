#include "core/WebView.h"
#include "core/WebPage.h"
#include "extensions/ExtensionManager.h"

#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QWebEngineHistory>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QFileDialog>
#include <QStandardPaths>

// ── Static per-domain zoom storage ────────────────────────────────────
QHash<QString, qreal> WebView::s_domainZoomLevels;

// ── Constructor / Destructor ──────────────────────────────────────────
WebView::WebView(QWidget *parent)
    : QWebEngineView(parent)
{
    setupPage();

    connect(this, &QWebEngineView::loadStarted,
            this, &WebView::onLoadStarted);
    connect(this, &QWebEngineView::loadProgress,
            this, &WebView::onLoadProgress);
    connect(this, &QWebEngineView::loadFinished,
            this, &WebView::onLoadFinished);
    connect(this, &QWebEngineView::urlChanged,
            this, &WebView::onUrlChanged);
    connect(this, &QWebEngineView::iconChanged,
            this, &WebView::onIconChanged);
}

WebView::~WebView() = default;

// ── Page setup ────────────────────────────────────────────────────────
void WebView::setupPage()
{
    m_page = new WebPage(QWebEngineProfile::defaultProfile(), this);
    setPage(m_page);

    // Audible state tracking (recentlyAudibleChanged in Qt 6.8+)
    connect(m_page, &QWebEnginePage::recentlyAudibleChanged,
            this, &WebView::onAudibleChanged);
}

// ── Audio ─────────────────────────────────────────────────────────────
bool WebView::isAudioMuted() const
{
    return page()->isAudioMuted();
}

void WebView::toggleAudioMute()
{
    page()->setAudioMuted(!page()->isAudioMuted());
}

// ── Zoom per domain ───────────────────────────────────────────────────
void WebView::setZoomForDomain(const QString &domain, qreal zoom)
{
    s_domainZoomLevels[domain] = zoom;
    setZoomFactor(zoom);
}

qreal WebView::zoomForDomain(const QString &domain) const
{
    return s_domainZoomLevels.value(domain, 1.0);
}

void WebView::applyDomainZoom(const QUrl &url)
{
    const QString domain = url.host();
    if (domain.isEmpty())
        return;
    const qreal zoom = s_domainZoomLevels.value(domain, 1.0);
    setZoomFactor(zoom);
}

// ── Loading slots ─────────────────────────────────────────────────────
void WebView::onLoadStarted()
{
    m_loadProgress = 0;
}

void WebView::onLoadProgress(int progress)
{
    m_loadProgress = progress;
}

void WebView::onLoadFinished(bool ok)
{
    m_loadProgress = 100;
    if (ok)
        injectContentScripts(url());
}

void WebView::onUrlChanged(const QUrl &url)
{
    applyDomainZoom(url);
}

void WebView::onIconChanged(const QIcon &icon)
{
    emit iconChanged(icon);
}

void WebView::onAudibleChanged(bool audible)
{
    emit audioStateChanged(audible);
}

// ── createWindow — open in new tab via signal ─────────────────────────
QWebEngineView *WebView::createWindow(QWebEnginePage::WebWindowType /*type*/)
{
    auto *newView = new WebView(parentWidget());

    connect(newView, &QWebEngineView::urlChanged, this,
            [this, newView](const QUrl &url) {
                emit newTabRequested(url);
                newView->deleteLater();
            });

    return newView;
}

// ── Content script injection ─────────────────────────────────────────
void WebView::injectContentScripts(const QUrl &pageUrl)
{
    if (!m_extensionManager || !m_page) return;

    auto scripts = m_extensionManager->getContentScriptsForUrl(pageUrl);

    // Inject CSS
    for (const auto &css : scripts.cssContents) {
        const QString safeCSS = QString::fromUtf8(QUrl::toPercentEncoding(css));
        const QString js = QStringLiteral(
            "(function() {"
            "  var s = document.createElement('style');"
            "  s.setAttribute('data-oynix-ext', 'true');"
            "  s.textContent = decodeURIComponent('%1');"
            "  (document.head || document.documentElement).appendChild(s);"
            "})();"
        ).arg(safeCSS);
        m_page->runJavaScript(js);
    }

    // Inject JS
    for (const auto &jsCode : scripts.jsContents) {
        m_page->runJavaScript(jsCode);
    }
}

// ── Context menu ──────────────────────────────────────────────────────
void WebView::contextMenuEvent(QContextMenuEvent *event)
{
    // Use Qt's built-in context menu with Nyx theming
    QMenu *menu = createStandardContextMenu();
    if (!menu)
        return;

    menu->setAttribute(Qt::WA_DeleteOnClose);

    // Context menu inherits from ThemeEngine global stylesheet
    // No manual styling needed

    // Add "Open Link in New Tab" if a link was right-clicked
    const QUrl pageUrl = page()->url();
    menu->addSeparator();

    QAction *newTabAction = menu->addAction(tr("Open Link in New &Tab"));
    connect(newTabAction, &QAction::triggered, this, [this]() {
        // Trigger "open in new window" which our createWindow() intercepts as a new tab
        page()->triggerAction(QWebEnginePage::OpenLinkInNewWindow);
    });

    // Save page
    QAction *saveAction = menu->addAction(tr("&Save Page As..."));
    connect(saveAction, &QAction::triggered, this, [this]() {
        const QString defaultPath =
            QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)
            + QLatin1Char('/') + page()->title() + QStringLiteral(".mhtml");
        const QString path = QFileDialog::getSaveFileName(
            this, tr("Save Page"), defaultPath,
            tr("MHTML (*.mhtml);;HTML (*.html)"));
        if (!path.isEmpty())
            page()->save(path);
    });

    menu->popup(event->globalPos());
}
