#pragma once

#include <QLineEdit>
#include <QUrl>

class QCompleter;
class QStringListModel;
class QLabel;

class UrlBar : public QLineEdit
{
    Q_OBJECT

public:
    explicit UrlBar(QWidget *parent = nullptr);
    ~UrlBar() override;

    void setDisplayUrl(const QUrl &url);
    void setSecurityIcon(const QString &iconPath);
    void addHistoryEntry(const QString &url);

signals:
    void urlEntered(const QUrl &url);
    void searchRequested(const QString &query);

protected:
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void applyStyle();
    void processInput();
    [[nodiscard]] bool looksLikeUrl(const QString &input) const;

    QUrl             m_currentUrl;
    QCompleter      *m_completer    = nullptr;
    QStringListModel *m_historyModel = nullptr;
    QStringList      m_historyList;
    QAction         *m_securityAction = nullptr;
    bool             m_focused      = false;
};
