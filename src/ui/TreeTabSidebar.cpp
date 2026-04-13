#include "TreeTabSidebar.h"
#include "core/TabWidget.h"
#include "core/WebView.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
#include <QHeaderView>
#include <QUrl>
#include <QScrollBar>

// ── Constructor / Destructor ─────────────────────────────────────────
TreeTabSidebar::TreeTabSidebar(TabWidget *tabWidget, QWidget *parent)
    : QWidget(parent)
    , m_tabWidget(tabWidget)
{
    setupUi();
    setupStyles();

    if (m_tabWidget) {
        connect(m_tabWidget, &TabWidget::tabCountChanged,
                this, [this](int) { refreshTabs(); });
        connect(m_tabWidget, &QTabWidget::currentChanged,
                this, [this](int) { onTabChanged(); });
        // Also refresh when tab title changes
        connect(m_tabWidget, &TabWidget::currentTitleChanged,
                this, [this](const QString &) { refreshTabs(); });
    }

    refreshTabs();
}

TreeTabSidebar::~TreeTabSidebar() = default;

// ── UI Setup ─────────────────────────────────────────────────────────
void TreeTabSidebar::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header
    auto *header = new QWidget(this);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(12, 8, 12, 8);

    auto *titleLabel = new QLabel(QStringLiteral("Tabs"), this);
    titleLabel->setObjectName(QStringLiteral("panelTitle"));
    headerLayout->addWidget(titleLabel);

    m_tabCountLabel = new QLabel(QStringLiteral("0"), this);
    m_tabCountLabel->setObjectName(QStringLiteral("badge"));
    headerLayout->addWidget(m_tabCountLabel);
    headerLayout->addStretch();

    m_newTabBtn = new QPushButton(QStringLiteral("+"), this);
    m_newTabBtn->setFixedSize(28, 28);
    m_newTabBtn->setObjectName(QStringLiteral("navBtn"));
    m_newTabBtn->setToolTip(tr("New Tab (Ctrl+T)"));
    connect(m_newTabBtn, &QPushButton::clicked, this, &TreeTabSidebar::newTabRequested);
    headerLayout->addWidget(m_newTabBtn);

    layout->addWidget(header);

    // Search
    m_searchInput = new QLineEdit(this);
    m_searchInput->setPlaceholderText(QStringLiteral("Filter tabs..."));
    connect(m_searchInput, &QLineEdit::textChanged, this, &TreeTabSidebar::onSearchChanged);
    layout->addWidget(m_searchInput);

    // Tree widget
    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setHeaderHidden(true);
    m_treeWidget->setRootIsDecorated(true);
    m_treeWidget->setDragDropMode(QTreeWidget::InternalMove);
    m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeWidget->setIndentation(16);
    m_treeWidget->setAnimated(true);
    m_treeWidget->setExpandsOnDoubleClick(false);

    connect(m_treeWidget, &QTreeWidget::itemClicked, this, &TreeTabSidebar::onItemClicked);
    connect(m_treeWidget, &QTreeWidget::customContextMenuRequested,
            this, &TreeTabSidebar::showContextMenu);

    layout->addWidget(m_treeWidget, 1);

    setFixedWidth(240);
}

void TreeTabSidebar::setupStyles()
{
    setStyleSheet(QStringLiteral(
        "TreeTabSidebar { background: #1a1b26; border-right: 1px solid #383b52; }"
        "#panelTitle { color: #8884c7; font-size: 12px; font-weight: 700;"
        "  letter-spacing: 0.08em; text-transform: uppercase; background: transparent; }"
        "#badge { color: #1a1b26; background: #6e6ab3; font-size: 10px; font-weight: 700;"
        "  padding: 1px 7px; border-radius: 8px; }"
        "#navBtn { color: #8884c7; font-size: 16px; font-weight: bold;"
        "  background: transparent; border: 1px solid #383b52; border-radius: 6px; }"
        "#navBtn:hover { background: #6e6ab3; color: #1a1b26; border-color: #6e6ab3; }"
    ));

    m_searchInput->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #24263a; color: #c8cad8; border: 1px solid #383b52;"
        "  border-radius: 6px; padding: 5px 10px; margin: 4px 10px 6px 10px;"
        "  font-size: 12px; }"
        "QLineEdit:focus { border-color: #6e6ab3; background: #2a2d42; }"
        "QLineEdit::placeholder { color: #565b7e; }"
    ));

    m_treeWidget->setStyleSheet(QStringLiteral(
        "QTreeWidget { background: transparent; border: none; outline: none; }"
        "QTreeWidget::item { padding: 6px 10px; margin: 1px 6px;"
        "  border-radius: 6px; color: #c8cad8; }"
        "QTreeWidget::item:selected { background: rgba(110,106,179,0.25);"
        "  color: #d4d2ee; }"
        "QTreeWidget::item:hover:!selected { background: rgba(110,106,179,0.12); }"
        "QTreeWidget::branch { background: transparent; }"
        "QTreeWidget::branch:has-children:closed { image: none; }"
        "QTreeWidget::branch:has-children:open { image: none; }"
        "QScrollBar:vertical { background: transparent; width: 5px; margin: 2px; }"
        "QScrollBar::handle:vertical { background: #383b52; border-radius: 2px; min-height: 20px; }"
        "QScrollBar::handle:vertical:hover { background: #565b7e; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    ));
}

// ── Refresh tab list ─────────────────────────────────────────────────
void TreeTabSidebar::refreshTabs()
{
    if (!m_tabWidget) return;

    // Remember scroll position
    const int scrollPos = m_treeWidget->verticalScrollBar()
                              ? m_treeWidget->verticalScrollBar()->value() : 0;

    m_treeWidget->clear();

    // Group by domain for a tree structure
    QMap<QString, QTreeWidgetItem *> domainNodes;

    // First pass: pinned tabs at top level
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        auto *view = m_tabWidget->webView(i);
        if (!view) continue;

        if (m_tabWidget->isTabPinned(i)) {
            QString title = m_tabWidget->tabText(i);
            if (title.length() > 32)
                title = title.left(29) + QStringLiteral("...");

            auto *item = new QTreeWidgetItem(m_treeWidget);
            item->setText(0, QStringLiteral("\xF0\x9F\x93\x8C ") + title); // 📌
            item->setData(0, Qt::UserRole, i);
            item->setToolTip(0, m_tabWidget->tabToolTip(i));
            item->setIcon(0, m_tabWidget->tabIcon(i));
        }
    }

    // Second pass: regular tabs grouped by domain
    for (int i = 0; i < m_tabWidget->count(); ++i) {
        auto *view = m_tabWidget->webView(i);
        if (!view || m_tabWidget->isTabPinned(i)) continue;

        const QUrl url = view->url();
        const QString domain = url.host().isEmpty()
            ? QStringLiteral("Internal") : url.host();
        QString title = m_tabWidget->tabText(i);
        if (title.length() > 32)
            title = title.left(29) + QStringLiteral("...");

        // Get or create domain group
        QTreeWidgetItem *parent = nullptr;
        if (domainNodes.contains(domain)) {
            parent = domainNodes[domain];
        } else {
            parent = new QTreeWidgetItem(m_treeWidget);
            parent->setText(0, domain);
            parent->setData(0, Qt::UserRole, -1); // -1 = group header
            parent->setFlags(parent->flags() & ~Qt::ItemIsSelectable);
            domainNodes[domain] = parent;
        }

        auto *item = new QTreeWidgetItem(parent);
        item->setText(0, title);
        item->setData(0, Qt::UserRole, i);
        item->setToolTip(0, m_tabWidget->tabToolTip(i));
        item->setIcon(0, m_tabWidget->tabIcon(i));
    }

    // Collapse single-item groups: if a domain has only 1 tab, flatten it
    QList<QTreeWidgetItem *> toFlatten;
    for (auto it = domainNodes.begin(); it != domainNodes.end(); ++it) {
        if (it.value()->childCount() == 1) {
            toFlatten.append(it.value());
        }
    }
    for (auto *group : toFlatten) {
        auto *child = group->takeChild(0);
        const int groupIdx = m_treeWidget->indexOfTopLevelItem(group);
        m_treeWidget->takeTopLevelItem(groupIdx);
        m_treeWidget->insertTopLevelItem(groupIdx, child);
        delete group;
    }

    // Expand all and highlight current
    m_treeWidget->expandAll();
    onTabChanged();

    m_tabCountLabel->setText(QString::number(m_tabWidget->count()));

    // Restore scroll position
    if (m_treeWidget->verticalScrollBar())
        m_treeWidget->verticalScrollBar()->setValue(scrollPos);
}

void TreeTabSidebar::onTabChanged()
{
    if (!m_tabWidget) return;
    const int current = m_tabWidget->currentIndex();

    // Walk entire tree to find the item
    std::function<void(QTreeWidgetItem *)> findAndSelect;
    findAndSelect = [&](QTreeWidgetItem *item) {
        if (item->data(0, Qt::UserRole).toInt() == current) {
            m_treeWidget->setCurrentItem(item);
            m_treeWidget->scrollToItem(item);
            return;
        }
        for (int c = 0; c < item->childCount(); ++c)
            findAndSelect(item->child(c));
    };

    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i)
        findAndSelect(m_treeWidget->topLevelItem(i));
}

void TreeTabSidebar::onItemClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (!item || !m_tabWidget) return;
    const int index = item->data(0, Qt::UserRole).toInt();
    if (index < 0) return; // Group header
    m_tabWidget->setCurrentIndex(index);
    emit tabActivated(index);
}

// ── Search filter ────────────────────────────────────────────────────
void TreeTabSidebar::onSearchChanged(const QString &text)
{
    const QString query = text.toLower();

    std::function<bool(QTreeWidgetItem *)> filterItem;
    filterItem = [&](QTreeWidgetItem *item) -> bool {
        bool anyChildVisible = false;
        for (int c = 0; c < item->childCount(); ++c) {
            if (filterItem(item->child(c)))
                anyChildVisible = true;
        }

        if (item->childCount() > 0) {
            // Group node — visible if any child is visible
            item->setHidden(!anyChildVisible && !query.isEmpty());
            return anyChildVisible;
        }

        // Leaf node
        const bool match = query.isEmpty() ||
            item->text(0).toLower().contains(query) ||
            item->toolTip(0).toLower().contains(query);
        item->setHidden(!match);
        return match;
    };

    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i)
        filterItem(m_treeWidget->topLevelItem(i));
}

// ── Context menu ─────────────────────────────────────────────────────
void TreeTabSidebar::showContextMenu(const QPoint &pos)
{
    auto *item = m_treeWidget->itemAt(pos);
    if (!item) return;

    const int index = item->data(0, Qt::UserRole).toInt();
    if (index < 0) return; // Can't act on group headers

    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    // Pin / Unpin
    if (m_tabWidget->isTabPinned(index)) {
        menu->addAction(tr("Unpin Tab"), this, [this, index]() {
            m_tabWidget->unpinTab(index);
            refreshTabs();
        });
    } else {
        menu->addAction(tr("Pin Tab"), this, [this, index]() {
            m_tabWidget->pinTab(index);
            refreshTabs();
        });
    }

    menu->addSeparator();

    // Duplicate
    menu->addAction(tr("Duplicate Tab"), this, [this, index]() {
        if (!m_tabWidget) return;
        auto *view = m_tabWidget->webView(index);
        if (view) m_tabWidget->addNewTab(view->url());
    });

    menu->addSeparator();

    // Close
    auto *closeAction = menu->addAction(tr("Close Tab"), this, [this, index]() {
        emit closeTabRequested(index);
    });
    closeAction->setEnabled(!m_tabWidget->isTabPinned(index));

    menu->addAction(tr("Close Other Tabs"), this, [this, index]() {
        if (!m_tabWidget) return;
        for (int i = m_tabWidget->count() - 1; i >= 0; --i) {
            if (i != index && !m_tabWidget->isTabPinned(i))
                emit closeTabRequested(i);
        }
    });

    menu->addAction(tr("Close Tabs Below"), this, [this, index]() {
        if (!m_tabWidget) return;
        for (int i = m_tabWidget->count() - 1; i > index; --i) {
            if (!m_tabWidget->isTabPinned(i))
                emit closeTabRequested(i);
        }
    });

    menu->popup(m_treeWidget->viewport()->mapToGlobal(pos));
}

void TreeTabSidebar::setVisible(bool visible)
{
    QWidget::setVisible(visible);
    if (visible) refreshTabs();
}
