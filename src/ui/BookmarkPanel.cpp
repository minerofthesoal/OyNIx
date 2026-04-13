#include "BookmarkPanel.h"
#include "data/BookmarkManager.h"
#include "theme/ThemeEngine.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QMenu>
#include <QHeaderView>
#include <QFileDialog>
#include <QInputDialog>
#include <QApplication>
#include <QClipboard>
#include <QUrl>
#include <QJsonObject>

// ── Constructor / Destructor ─────────────────────────────────────────
BookmarkPanel::BookmarkPanel(BookmarkManager *manager, QWidget *parent)
    : QWidget(parent)
    , m_manager(manager)
{
    setupUi();
    setupStyles();
    refresh();

    if (m_manager) {
        connect(m_manager, &BookmarkManager::bookmarksChanged,
                this, &BookmarkPanel::refresh);
    }
}

BookmarkPanel::~BookmarkPanel() = default;

// ── UI Setup ─────────────────────────────────────────────────────────
void BookmarkPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Header
    auto *header = new QWidget(this);
    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(8, 6, 8, 6);
    auto *title = new QLabel(QStringLiteral("Bookmarks"), this);
    title->setObjectName(QStringLiteral("panelTitle"));
    headerLayout->addWidget(title);
    headerLayout->addStretch();

    m_addFolderBtn = new QPushButton(QStringLiteral("+Folder"), this);
    m_addFolderBtn->setFixedHeight(24);
    connect(m_addFolderBtn, &QPushButton::clicked, this, &BookmarkPanel::addFolder);
    headerLayout->addWidget(m_addFolderBtn);
    layout->addWidget(header);

    // Search
    m_searchInput = new QLineEdit(this);
    m_searchInput->setPlaceholderText(QStringLiteral("Search bookmarks..."));
    connect(m_searchInput, &QLineEdit::textChanged, this, &BookmarkPanel::onSearchChanged);
    layout->addWidget(m_searchInput);

    // Tree
    m_treeWidget = new QTreeWidget(this);
    m_treeWidget->setHeaderHidden(true);
    m_treeWidget->setDragDropMode(QTreeWidget::InternalMove);
    m_treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
    m_treeWidget->setIndentation(16);

    connect(m_treeWidget, &QTreeWidget::itemDoubleClicked,
            this, &BookmarkPanel::onItemDoubleClicked);
    connect(m_treeWidget, &QTreeWidget::customContextMenuRequested,
            this, &BookmarkPanel::showContextMenu);
    layout->addWidget(m_treeWidget, 1);

    // Bottom buttons
    auto *bottomRow = new QHBoxLayout;
    bottomRow->setContentsMargins(8, 4, 8, 6);
    m_importBtn = new QPushButton(QStringLiteral("Import"), this);
    connect(m_importBtn, &QPushButton::clicked, this, &BookmarkPanel::importBookmarks);
    m_exportBtn = new QPushButton(QStringLiteral("Export"), this);
    connect(m_exportBtn, &QPushButton::clicked, this, &BookmarkPanel::exportBookmarks);
    bottomRow->addWidget(m_importBtn);
    bottomRow->addWidget(m_exportBtn);
    bottomRow->addStretch();
    layout->addLayout(bottomRow);
}

void BookmarkPanel::setupStyles()
{
    const auto &c = ThemeEngine::instance().colors();

    QString ss;
    ss += QStringLiteral("BookmarkPanel { background: ") + c["bg-darkest"] + QStringLiteral("; }\n");
    ss += QStringLiteral("#panelTitle { color: ") + c["purple-light"]
       + QStringLiteral("; font-size: 13px; font-weight: 700;"
                        " letter-spacing: 0.05em; text-transform: uppercase; }\n");
    ss += QStringLiteral("QLineEdit { background: ") + c["bg-mid"]
       + QStringLiteral("; color: ") + c["text-primary"]
       + QStringLiteral("; border: 1px solid ") + c["border"]
       + QStringLiteral("; border-radius: 6px; padding: 5px 10px;"
                        " margin: 0 8px 4px 8px; font-size: 12px; }\n");
    ss += QStringLiteral("QLineEdit:focus { border-color: ") + c["purple-mid"] + QStringLiteral("; }\n");
    ss += QStringLiteral("QLineEdit::placeholder { color: ") + c["text-muted"] + QStringLiteral("; }\n");
    ss += QStringLiteral("QPushButton { background: rgba(110,106,179,0.15); color: ")
       + c["purple-light"] + QStringLiteral("; border: 1px solid rgba(110,106,179,0.3);"
                                             " border-radius: 6px; padding: 4px 10px;"
                                             " font-size: 11px; }\n");
    ss += QStringLiteral("QPushButton:hover { background: rgba(110,106,179,0.3); }\n");
    ss += QStringLiteral("QTreeWidget { background: transparent; color: ") + c["text-primary"]
       + QStringLiteral("; border: none; font-size: 12px; outline: none; }\n");
    ss += QStringLiteral("QTreeWidget::item { padding: 5px 8px; border-radius: 6px;"
                         " margin: 1px 4px; }\n");
    ss += QStringLiteral("QTreeWidget::item:selected { background: ") + c["selection"]
       + QStringLiteral("; color: ") + c["purple-pale"] + QStringLiteral("; }\n");
    ss += QStringLiteral("QTreeWidget::item:hover:!selected { background: rgba(110,106,179,0.12); }\n");
    ss += QStringLiteral("QScrollBar:vertical { background: transparent; width: 5px; }\n");
    ss += QStringLiteral("QScrollBar::handle:vertical { background: ") + c["scrollbar"]
       + QStringLiteral("; border-radius: 2px; }\n");
    ss += QStringLiteral("QScrollBar::handle:vertical:hover { background: ") + c["scrollbar-hover"]
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }\n");
    setStyleSheet(ss);
}

// ── Refresh from manager ─────────────────────────────────────────────
void BookmarkPanel::refresh()
{
    if (!m_manager || !m_treeWidget) return;
    m_treeWidget->clear();

    // Get folders
    const QStringList folders = m_manager->folders();
    for (const auto &folder : folders) {
        auto *folderItem = new QTreeWidgetItem(m_treeWidget);
        folderItem->setText(0, folder);
        folderItem->setData(0, Qt::UserRole, QStringLiteral("folder"));
        folderItem->setFlags(folderItem->flags() | Qt::ItemIsDropEnabled);

        // Get bookmarks in this folder
        const QJsonArray bookmarks = m_manager->bookmarksInFolder(folder);
        for (const auto &val : bookmarks) {
            auto obj = val.toObject();
            auto *item = new QTreeWidgetItem(folderItem);
            item->setText(0, obj[QStringLiteral("title")].toString());
            item->setData(0, Qt::UserRole, obj[QStringLiteral("url")].toString());
            item->setToolTip(0, obj[QStringLiteral("url")].toString());
        }
    }

    // Unsorted bookmarks
    const QJsonArray unsorted = m_manager->unsortedBookmarks();
    if (!unsorted.isEmpty()) {
        auto *unsortedItem = new QTreeWidgetItem(m_treeWidget);
        unsortedItem->setText(0, QStringLiteral("Unsorted"));
        unsortedItem->setData(0, Qt::UserRole, QStringLiteral("folder"));

        for (const auto &val : unsorted) {
            auto obj = val.toObject();
            auto *item = new QTreeWidgetItem(unsortedItem);
            item->setText(0, obj[QStringLiteral("title")].toString());
            item->setData(0, Qt::UserRole, obj[QStringLiteral("url")].toString());
            item->setToolTip(0, obj[QStringLiteral("url")].toString());
        }
    }

    m_treeWidget->expandAll();
}

// ── Actions ──────────────────────────────────────────────────────────
void BookmarkPanel::onItemDoubleClicked(QTreeWidgetItem *item, int /*column*/)
{
    if (!item) return;
    const QString data = item->data(0, Qt::UserRole).toString();
    if (data != QStringLiteral("folder") && !data.isEmpty()) {
        emit bookmarkActivated(QUrl(data));
    }
}

void BookmarkPanel::onSearchChanged(const QString &text)
{
    const QString query = text.toLower();
    for (int i = 0; i < m_treeWidget->topLevelItemCount(); ++i) {
        auto *folder = m_treeWidget->topLevelItem(i);
        bool anyVisible = false;
        for (int j = 0; j < folder->childCount(); ++j) {
            auto *child = folder->child(j);
            const bool match = query.isEmpty() ||
                child->text(0).toLower().contains(query) ||
                child->toolTip(0).toLower().contains(query);
            child->setHidden(!match);
            if (match) anyVisible = true;
        }
        folder->setHidden(!anyVisible && !query.isEmpty());
    }
}

void BookmarkPanel::showContextMenu(const QPoint &pos)
{
    auto *item = m_treeWidget->itemAt(pos);
    if (!item) return;

    auto *menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);
    const auto &c = ThemeEngine::instance().colors();
    menu->setStyleSheet(
        QStringLiteral("QMenu { background: ") + c["bg-darkest"]
        + QStringLiteral("; color: ") + c["text-primary"]
        + QStringLiteral("; border: 1px solid ") + c["purple-mid"]
        + QStringLiteral("; border-radius: 8px; }"
                         "QMenu::item { padding: 6px 16px; border-radius: 4px; }"
                         "QMenu::item:selected { background: ") + c["purple-dark"]
        + QStringLiteral("; }"));

    const QString data = item->data(0, Qt::UserRole).toString();
    if (data != QStringLiteral("folder")) {
        menu->addAction(QStringLiteral("Open"), this, [this, data]() {
            emit bookmarkActivated(QUrl(data));
        });
        menu->addAction(QStringLiteral("Copy URL"), this, [data]() {
            QApplication::clipboard()->setText(data);
        });
        menu->addSeparator();
        menu->addAction(QStringLiteral("Delete"), this, [this, data]() {
            if (m_manager) m_manager->removeBookmark(data);
        });
    } else {
        menu->addAction(QStringLiteral("Rename Folder"), this, [this, item]() {
            bool ok;
            const QString name = QInputDialog::getText(this,
                QStringLiteral("Rename Folder"), QStringLiteral("New name:"),
                QLineEdit::Normal, item->text(0), &ok);
            if (ok && !name.isEmpty()) {
                if (m_manager) m_manager->renameFolder(item->text(0), name);
            }
        });
    }

    menu->popup(m_treeWidget->viewport()->mapToGlobal(pos));
}

void BookmarkPanel::addFolder()
{
    bool ok;
    const QString name = QInputDialog::getText(this,
        QStringLiteral("New Folder"), QStringLiteral("Folder name:"),
        QLineEdit::Normal, QString(), &ok);
    if (ok && !name.isEmpty() && m_manager) {
        m_manager->addFolder(name);
    }
}

void BookmarkPanel::importBookmarks()
{
    const QString path = QFileDialog::getOpenFileName(this,
        QStringLiteral("Import Bookmarks"), QString(),
        QStringLiteral("JSON (*.json)"));
    if (!path.isEmpty() && m_manager)
        m_manager->importFromFile(path);
}

void BookmarkPanel::exportBookmarks()
{
    const QString path = QFileDialog::getSaveFileName(this,
        QStringLiteral("Export Bookmarks"), QStringLiteral("bookmarks.json"),
        QStringLiteral("JSON (*.json)"));
    if (!path.isEmpty() && m_manager)
        m_manager->exportToFile(path);
}
