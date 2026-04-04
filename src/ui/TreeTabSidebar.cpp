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

// ── Constructor / Destructor ─────────────────────────────────────────
TreeTabSidebar::TreeTabSidebar(TabWidget *tabWidget, QWidget *parent)
    : QWidget(parent)
    , m_tabWidget(tabWidget)
{
    setupUi();
    setupStyles();

    // Connect to tab widget changes
    if (m_tabWidget) {
        connect(m_tabWidget, &TabWidget::tabCountChanged,
                this, [this](int) { refreshTabs(); });
        connect(m_tabWidget, &QTabWidget::currentChanged,
                this, [this](int) { onTabChanged(); });
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
    headerLayout->setContentsMargins(8, 6, 8, 6);

    auto *titleLabel = new QLabel(QStringLiteral("Tabs"), this);
    titleLabel->setStyleSheet(QStringLiteral("color: #E8E0F0; font-size: 13px; font-weight: bold;"));
    headerLayout->addWidget(titleLabel);

    m_tabCountLabel = new QLabel(QStringLiteral("0"), this);
    m_tabCountLabel->setStyleSheet(QStringLiteral(
        "background: #7B4FBF; color: white; border-radius: 8px;"
        " padding: 1px 6px; font-size: 10px; font-weight: bold;"));
    headerLayout->addWidget(m_tabCountLabel);
    headerLayout->addStretch();

    m_newTabBtn = new QPushButton(QStringLiteral("+"), this);
    m_newTabBtn->setFixedSize(24, 24);
    m_newTabBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; color: #E8E0F0; border: none;"
        "  font-size: 16px; font-weight: bold; }"
        "QPushButton:hover { background: rgba(123,79,191,0.3); border-radius: 4px; }"));
    connect(m_newTabBtn, &QPushButton::clicked, this, &TreeTabSidebar::newTabRequested);
    headerLayout->addWidget(m_newTabBtn);

    layout->addWidget(header);

    // Search
    m_searchInput = new QLineEdit(this);
    m_searchInput->setPlaceholderText(QStringLiteral("Search tabs..."));
    m_searchInput->setStyleSheet(QStringLiteral(
        "QLineEdit { background: #12121e; color: #E8E0F0; border: 1px solid #2a2a40;"
        "  border-radius: 4px; padding: 4px 8px; margin: 0 8px 4px 8px; font-size: 12px; }"
        "QLineEdit:focus { border-color: #7B4FBF; }"));
    connect(m_searchInput, &QLineEdit::textChanged, this, &TreeTabSidebar::onSearchChanged);
    layout->addWidget(m_searchInput);

    // Tree widget
    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setHeaderHidden(true);
    m_treeWidget->setRootIsDecorated(true);
    m_treeWidget->setDragDropMode(QTreeWidget::InternalMove);
    m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeWidget->setIndentation(16);

    connect(m_treeWidget, &QTreeWidget::itemClicked, this, &TreeTabSidebar::onItemClicked);
    connect(m_treeWidget, &QTreeWidget::customContextMenuRequested,
            this, &TreeTabSidebar::showContextMenu);

    layout->addWidget(m_treeWidget, 1);

    setFixedWidth(220);
}

void TreeTabSidebar::setupStyles()
{
    setStyleSheet(QStringLiteral(
        "TreeTabSidebar { background: #0a0a12; border-right: 1px solid #2a2a40; }"));

    m_treeWidget->setStyleSheet(QStringLiteral(
        "QTreeWidget { background: #0a0a12; color: #E8E0F0; border: none;"
        "  font-size: 12px; outline: none; }"
        "QTreeWidget::item { padding: 4px 6px; border-radius: 4px; margin: 1px 4px; }"
        "QTreeWidget::item:selected { background: rgba(123,79,191,0.4); }"
        "QTreeWidget::item:hover { background: rgba(123,79,191,0.2); }"
        "QTreeWidget::branch { background: transparent; }"
        "QScrollBar:vertical { background: #0a0a12; width: 6px; }"
        "QScrollBar::handle:vertical { background: #7B4FBF; border-radius: 3px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    ));
}

// ── Refresh tab list ─────────────────────────────────────────────────
void TreeTabSidebar::refreshTabs()
{
    if (!m_tabWidget) return;

    m_treeWidget->clear();

    for (int i = 0; i < m_tabWidget->count(); ++i) {
        auto *view = m_tabWidget->webView(i);
        if (!view) continue;

        QString title = m_tabWidget->tabText(i);
        if (title.length() > 35)
            title = title.left(32) + QStringLiteral("...");

        auto *item = new QTreeWidgetItem(m_treeWidget);
        item->setText(0, title);
        item->setData(0, Qt::UserRole, i);
        item->setToolTip(0, m_tabWidget->tabToolTip(i));
        item->setIcon(0, m_tabWidget->tabIcon(i));

        if (m_tabWidget->isTabPinned(i)) {
            item->setText(0, QStringLiteral("[Pin] ") + title);
        }
    }

    // Highlight current
    onTabChanged();

    // Auto-expand all top-level items
    m_treeWidget->expandAll();

    m_tabCountLabel->setText(QString::number(m_tabWidget->count()));
}

void TreeTabSidebar::onTabChanged()
{
    if (!m_tabWidget) return;
    const int current = m_tabWidget->currentIndex();

    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        auto *item = m_treeWidget->topLevelItem(i);
        if (item->data(0, Qt::UserRole).toInt() == current) {
            m_treeWidget->setCurrentItem(item);
            break;
        }
    }
}

void TreeTabSidebar::onItemClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (!item || !m_tabWidget) return;
    const int index = item->data(0, Qt::UserRole).toInt();
    m_tabWidget->setCurrentIndex(index);
    emit tabActivated(index);
}

// ── Search filter ────────────────────────────────────────────────────
void TreeTabSidebar::onSearchChanged(const QString &text)
{
    const QString query = text.toLower();
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        auto *item = m_treeWidget->topLevelItem(i);
        const bool match = query.isEmpty() ||
            item->text(0).toLower().contains(query) ||
            item->toolTip(0).toLower().contains(query);
        item->setHidden(!match);
    }
}

// ── Context menu ─────────────────────────────────────────────────────
void TreeTabSidebar::showContextMenu(const QPoint &pos)
{
    auto *item = m_treeWidget->itemAt(pos);
    if (!item) return;

    const int index = item->data(0, Qt::UserRole).toInt();

    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    menu->setStyleSheet(QStringLiteral(
        "QMenu { background: #0e0e16; color: #E8E0F0; border: 1px solid #7B4FBF; }"
        "QMenu::item:selected { background: #7B4FBF; }"));

    menu->addAction(QStringLiteral("Close Tab"), this, [this, index]() {
        emit closeTabRequested(index);
    });
    menu->addAction(QStringLiteral("Close Other Tabs"), this, [this, index]() {
        if (!m_tabWidget) return;
        for (int i = m_tabWidget->count() - 1; i >= 0; --i) {
            if (i != index && !m_tabWidget->isTabPinned(i))
                emit closeTabRequested(i);
        }
    });
    menu->addSeparator();
    menu->addAction(QStringLiteral("Duplicate Tab"), this, [this, index]() {
        if (!m_tabWidget) return;
        auto *view = m_tabWidget->webView(index);
        if (view) m_tabWidget->addNewTab(view->url());
    });

    menu->popup(m_treeWidget->viewport()->mapToGlobal(pos));
}

void TreeTabSidebar::setVisible(bool visible)
{
    QWidget::setVisible(visible);
}
