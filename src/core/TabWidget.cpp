#include "core/TabWidget.h"
#include "core/WebView.h"
#include "core/WebPage.h"

#include <QTabBar>
#include <QToolButton>
#include <QMenu>
#include <QMouseEvent>
#include <QApplication>
#include <QStyle>

// ── Constructor / Destructor ──────────────────────────────────────────
TabWidget::TabWidget(QWidget *parent)
    : QTabWidget(parent)
{
    setupTabBar();
    setupNewTabButton();
    setupConnections();

    setDocumentMode(true);
    setElideMode(Qt::ElideRight);
    setMovable(true);
    setTabsClosable(true);

    // Apply OyNIx dark theme to tab bar
    const QString bg     = QStringLiteral("#08080d");
    const QString purple = QStringLiteral("#7B4FBF");
    const QString text   = QStringLiteral("#E8E0F0");

    setStyleSheet(QStringLiteral(
        "QTabWidget::pane { border: none; background: %1; }"
        "QTabBar { background: %1; }"
        "QTabBar::tab {"
        "  background: %1; color: %3; padding: 6px 16px;"
        "  border: 1px solid transparent; border-bottom: none;"
        "  min-width: 120px; max-width: 240px;"
        "}"
        "QTabBar::tab:selected {"
        "  background: %2; border-color: %2;"
        "}"
        "QTabBar::tab:hover:!selected {"
        "  background: rgba(123,79,191,0.3);"
        "}"
        "QTabBar::close-button {"
        "  image: none; /* custom paint handled by style */ "
        "}"
    ).arg(bg, purple, text));
}

TabWidget::~TabWidget() = default;

// ── Setup ─────────────────────────────────────────────────────────────
void TabWidget::setupTabBar()
{
    QTabBar *bar = tabBar();
    bar->setContextMenuPolicy(Qt::CustomContextMenu);
    bar->installEventFilter(this);  // for middle-click
    connect(bar, &QTabBar::customContextMenuRequested,
            this, &TabWidget::showTabContextMenu);
}

void TabWidget::setupNewTabButton()
{
    m_newTabButton = new QToolButton(this);
    m_newTabButton->setText(QStringLiteral("+"));
    m_newTabButton->setAutoRaise(true);
    m_newTabButton->setToolTip(tr("New Tab"));

    const QString purple = QStringLiteral("#7B4FBF");
    const QString text   = QStringLiteral("#E8E0F0");
    m_newTabButton->setStyleSheet(QStringLiteral(
        "QToolButton { color: %1; font-size: 18px; font-weight: bold;"
        "  border: none; padding: 4px 10px; }"
        "QToolButton:hover { background: %2; border-radius: 4px; }"
    ).arg(text, purple));

    setCornerWidget(m_newTabButton, Qt::TopRightCorner);

    connect(m_newTabButton, &QToolButton::clicked, this, [this]() {
        addNewTab();
    });
}

void TabWidget::setupConnections()
{
    connect(this, &QTabWidget::currentChanged,
            this, &TabWidget::onCurrentChanged);
    connect(this, &QTabWidget::tabCloseRequested,
            this, &TabWidget::onTabCloseRequested);
}

// ── Add / Get Tabs ────────────────────────────────────────────────────
WebView *TabWidget::addNewTab(const QUrl &url)
{
    if (count() >= MaxTabs)
        return currentWebView();

    auto *view = new WebView(this);
    if (m_extensionManager)
        view->setExtensionManager(m_extensionManager);

    // Connect WebView signals to update tab state
    connect(view, &QWebEngineView::titleChanged, this, [this, view](const QString &title) {
        const int idx = indexOf(view);
        if (idx < 0) return;
        const QString display = title.isEmpty() ? tr("New Tab") : title;
        setTabText(idx, display);
        updateTabTooltip(idx);
        if (idx == currentIndex())
            emit currentTitleChanged(display);
    });

    connect(view, &WebView::iconChanged, this, [this, view](const QIcon &icon) {
        const int idx = indexOf(view);
        if (idx >= 0)
            setTabIcon(idx, icon);
    });

    connect(view, &QWebEngineView::urlChanged, this, [this, view](const QUrl &u) {
        const int idx = indexOf(view);
        if (idx >= 0)
            updateTabTooltip(idx);
        if (indexOf(view) == currentIndex())
            emit currentUrlChanged(u);
    });

    // New tab requests from the view (target=_blank etc.)
    connect(view, &WebView::newTabRequested, this, [this](const QUrl &u) {
        addNewTab(u);
    });

    // Forward oyn:// interception
    connect(view->webPage(), &WebPage::oynUrlRequested, this, [this](const QString &path) {
        // Re-emit as a URL so BrowserWindow can handle it
        emit currentUrlChanged(QUrl(QStringLiteral("oyn://") + path));
    });

    // Audio state — update tab icon/text hint
    connect(view, &WebView::audioStateChanged, this, [this, view](bool playing) {
        const int idx = indexOf(view);
        if (idx < 0) return;
        // Prepend a speaker indicator to the tab text
        QString text = tabText(idx);
        const QString speaker = QStringLiteral("\xF0\x9F\x94\x8A ");  // 🔊
        if (playing && !text.startsWith(speaker))
            setTabText(idx, speaker + text);
        else if (!playing && text.startsWith(speaker))
            setTabText(idx, text.mid(speaker.size()));
    });

    const int index = insertTab(count(), view, tr("New Tab"));
    setCurrentIndex(index);
    updateTabTooltip(index);
    emitTabCountChanged();

    if (!url.isEmpty())
        view->load(url);

    return view;
}

WebView *TabWidget::currentWebView() const
{
    return webView(currentIndex());
}

WebView *TabWidget::webView(int index) const
{
    return qobject_cast<WebView *>(widget(index));
}

// ── Pinning ───────────────────────────────────────────────────────────
void TabWidget::pinTab(int index)
{
    auto *view = webView(index);
    if (!view || m_pinnedTabs.contains(view))
        return;

    m_pinnedTabs.insert(view);

    // Move to front (after other pinned tabs)
    const int target = pinnedTabCount() - 1;  // we already inserted
    if (index != target)
        tabBar()->moveTab(index, target);

    // Visual: remove close button, shrink width
    tabBar()->setTabButton(target, QTabBar::RightSide, nullptr);
    tabBar()->setTabButton(target, QTabBar::LeftSide, nullptr);
}

void TabWidget::unpinTab(int index)
{
    auto *view = webView(index);
    if (!view || !m_pinnedTabs.contains(view))
        return;

    m_pinnedTabs.remove(view);

    // Move after pinned section
    const int target = pinnedTabCount();  // first non-pinned position
    if (index != target)
        tabBar()->moveTab(index, target);
}

bool TabWidget::isTabPinned(int index) const
{
    auto *view = webView(index);
    return view && m_pinnedTabs.contains(view);
}

int TabWidget::pinnedTabCount() const
{
    return static_cast<int>(m_pinnedTabs.size());
}

// ── Tab search ────────────────────────────────────────────────────────
QList<int> TabWidget::findTab(const QString &query) const
{
    QList<int> results;
    const QString q = query.toLower();
    for (int i = 0; i < count(); ++i) {
        const auto *view = webView(i);
        if (!view) continue;
        if (tabText(i).toLower().contains(q) ||
            view->url().toString().toLower().contains(q)) {
            results.append(i);
        }
    }
    return results;
}

// ── Event filter (middle-click) ───────────────────────────────────────
bool TabWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == tabBar() && event->type() == QEvent::MouseButtonRelease) {
        auto *me = static_cast<QMouseEvent *>(event);
        if (me->button() == Qt::MiddleButton) {
            const int idx = tabBar()->tabAt(me->pos());
            if (idx >= 0) {
                // Middle-click on a tab — close it
                closeTab(idx);
            } else {
                // Middle-click on empty tab bar — new tab
                addNewTab();
            }
            return true;
        }
    }
    return QTabWidget::eventFilter(watched, event);
}

// ── Slots ─────────────────────────────────────────────────────────────
void TabWidget::onCurrentChanged(int index)
{
    auto *view = webView(index);
    if (!view) return;

    emit currentUrlChanged(view->url());
    emit currentTitleChanged(view->title().isEmpty() ? tr("New Tab") : view->title());
}

void TabWidget::onTabCloseRequested(int index)
{
    closeTab(index);
}

void TabWidget::closeTab(int index)
{
    if (isTabPinned(index))
        return; // pinned tabs can't be closed

    auto *view = webView(index);
    if (!view) return;

    // Don't close the last tab — open a new one instead
    if (count() <= 1) {
        addNewTab();
    }

    removeTab(index);
    view->deleteLater();
    emitTabCountChanged();
}

// ── Context menu ──────────────────────────────────────────────────────
void TabWidget::showTabContextMenu(const QPoint &pos)
{
    const int index = tabBar()->tabAt(pos);
    if (index < 0) return;

    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const QString bg     = QStringLiteral("#08080d");
    const QString purple = QStringLiteral("#7B4FBF");
    const QString text   = QStringLiteral("#E8E0F0");
    menu->setStyleSheet(QStringLiteral(
        "QMenu { background: %1; color: %3; border: 1px solid %2; }"
        "QMenu::item:selected { background: %2; }"
        "QMenu::separator { background: %2; height: 1px; margin: 4px 8px; }")
        .arg(bg, purple, text));

    // Pin / Unpin
    if (isTabPinned(index)) {
        QAction *unpin = menu->addAction(tr("Unpin Tab"));
        connect(unpin, &QAction::triggered, this, [this, index]() {
            unpinTab(index);
        });
    } else {
        QAction *pin = menu->addAction(tr("Pin Tab"));
        connect(pin, &QAction::triggered, this, [this, index]() {
            pinTab(index);
        });
    }

    // Duplicate
    QAction *dup = menu->addAction(tr("Duplicate Tab"));
    connect(dup, &QAction::triggered, this, [this, index]() {
        duplicateTab(index);
    });

    menu->addSeparator();

    // Close others
    QAction *closeOthers = menu->addAction(tr("Close Other Tabs"));
    closeOthers->setEnabled(count() > 1);
    connect(closeOthers, &QAction::triggered, this, [this, index]() {
        closeOtherTabs(index);
    });

    // Close to right
    QAction *closeRight = menu->addAction(tr("Close Tabs to the Right"));
    closeRight->setEnabled(index < count() - 1);
    connect(closeRight, &QAction::triggered, this, [this, index]() {
        closeTabsToRight(index);
    });

    menu->addSeparator();

    // Close this tab (disabled if pinned)
    QAction *closeThis = menu->addAction(tr("Close Tab"));
    closeThis->setEnabled(!isTabPinned(index));
    connect(closeThis, &QAction::triggered, this, [this, index]() {
        closeTab(index);
    });

    menu->popup(tabBar()->mapToGlobal(pos));
}

// ── Context menu actions ──────────────────────────────────────────────
void TabWidget::duplicateTab(int index)
{
    auto *view = webView(index);
    if (!view) return;
    addNewTab(view->url());
}

void TabWidget::closeOtherTabs(int index)
{
    // Close from end to start, skipping pinned and the target
    for (int i = count() - 1; i >= 0; --i) {
        if (i != index && !isTabPinned(i))
            closeTab(i);
    }
}

void TabWidget::closeTabsToRight(int index)
{
    for (int i = count() - 1; i > index; --i) {
        if (!isTabPinned(i))
            closeTab(i);
    }
}

// ── Helpers ───────────────────────────────────────────────────────────
void TabWidget::updateTabTooltip(int index)
{
    auto *view = webView(index);
    if (!view) return;

    const QString title = view->title().isEmpty() ? tr("New Tab") : view->title();
    const QString url   = view->url().toString();
    setTabToolTip(index, QStringLiteral("%1\n%2").arg(title, url));
}

void TabWidget::emitTabCountChanged()
{
    emit tabCountChanged(count());
}
