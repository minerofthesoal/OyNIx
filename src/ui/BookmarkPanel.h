#pragma once

#include <QWidget>
#include <QJsonArray>

class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QPushButton;
class BookmarkManager;

/**
 * BookmarkPanel — Sidebar panel for bookmark management.
 * Supports folders, search, drag-drop, import/export, and context menu operations.
 */
class BookmarkPanel : public QWidget
{
    Q_OBJECT

public:
    explicit BookmarkPanel(BookmarkManager *manager, QWidget *parent = nullptr);
    ~BookmarkPanel() override;

    void refresh();

signals:
    void bookmarkActivated(const QUrl &url);
    void addBookmarkRequested(const QUrl &url, const QString &title);

private:
    void setupUi();
    void setupStyles();

    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onSearchChanged(const QString &text);
    void showContextMenu(const QPoint &pos);
    void addFolder();
    void importBookmarks();
    void exportBookmarks();

    BookmarkManager *m_manager = nullptr;
    QTreeWidget     *m_treeWidget = nullptr;
    QLineEdit       *m_searchInput = nullptr;
    QPushButton     *m_addFolderBtn = nullptr;
    QPushButton     *m_importBtn = nullptr;
    QPushButton     *m_exportBtn = nullptr;
};
