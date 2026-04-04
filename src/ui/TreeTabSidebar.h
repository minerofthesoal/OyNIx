#pragma once

#include <QWidget>
#include <QUrl>

class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QPushButton;
class QLabel;
class TabWidget;

/**
 * TreeTabSidebar — Tree-style tab sidebar (like Vivaldi/Firefox Sidebery).
 * Shows tabs in a hierarchical tree with parent-child relationships,
 * search, drag-drop reorder, and context menu.
 */
class TreeTabSidebar : public QWidget
{
    Q_OBJECT

public:
    explicit TreeTabSidebar(TabWidget *tabWidget, QWidget *parent = nullptr);
    ~TreeTabSidebar() override;

    void refreshTabs();
    void setVisible(bool visible) override;

signals:
    void tabActivated(int index);
    void newTabRequested();
    void closeTabRequested(int index);

private:
    void setupUi();
    void setupStyles();

    void onTabChanged();
    void onItemClicked(QTreeWidgetItem *item, int column);
    void onSearchChanged(const QString &text);
    void showContextMenu(const QPoint &pos);

    TabWidget    *m_tabWidget    = nullptr;
    QTreeWidget  *m_treeWidget   = nullptr;
    QLineEdit    *m_searchInput  = nullptr;
    QPushButton  *m_newTabBtn    = nullptr;
    QLabel       *m_tabCountLabel = nullptr;
};
