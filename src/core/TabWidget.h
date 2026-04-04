#pragma once

#include <QTabWidget>
#include <QUrl>
#include <QSet>

class WebView;
class QMenu;
class QToolButton;

class TabWidget : public QTabWidget
{
    Q_OBJECT

public:
    static constexpr int MaxTabs = 50;

    explicit TabWidget(QWidget *parent = nullptr);
    ~TabWidget() override;

    WebView *addNewTab(const QUrl &url = QUrl(QStringLiteral("oyn://home")));
    [[nodiscard]] WebView *currentWebView() const;
    [[nodiscard]] WebView *webView(int index) const;

    void pinTab(int index);
    void unpinTab(int index);
    [[nodiscard]] bool isTabPinned(int index) const;

    QList<int> findTab(const QString &query) const;

signals:
    void currentUrlChanged(const QUrl &url);
    void currentTitleChanged(const QString &title);
    void tabCountChanged(int count);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupTabBar();
    void setupNewTabButton();
    void setupConnections();

    void onCurrentChanged(int index);
    void onTabCloseRequested(int index);
    void closeTab(int index);
    void showTabContextMenu(const QPoint &pos);

    // Context menu actions
    void duplicateTab(int index);
    void closeOtherTabs(int index);
    void closeTabsToRight(int index);

    void updateTabTooltip(int index);
    void emitTabCountChanged();

    int pinnedTabCount() const;

    QToolButton *m_newTabButton = nullptr;
    QSet<int>    m_pinnedIndices;  // track by widget pointer instead — see cpp
    QSet<WebView *> m_pinnedTabs;
};
