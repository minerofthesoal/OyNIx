#pragma once

#include <QWidget>

class QLineEdit;
class QPushButton;
class QLabel;
class QPropertyAnimation;
class QWebEngineView;

class FindBar : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int slideOffset READ slideOffset WRITE setSlideOffset)

public:
    explicit FindBar(QWidget *parent = nullptr);
    ~FindBar() override;

    void setWebView(QWebEngineView *view);
    void showBar();
    void hideBar();

    [[nodiscard]] int slideOffset() const { return m_slideOffset; }
    void setSlideOffset(int offset);

signals:
    void hidden();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private:
    void findNext();
    void findPrev();
    void onTextChanged(const QString &text);
    void updateMatchCount(int current, int total);
    void applyStyle();

    QLineEdit          *m_searchInput   = nullptr;
    QPushButton        *m_nextButton    = nullptr;
    QPushButton        *m_prevButton    = nullptr;
    QPushButton        *m_closeButton   = nullptr;
    QLabel             *m_matchLabel    = nullptr;
    QPropertyAnimation *m_animation     = nullptr;
    QWebEngineView     *m_webView       = nullptr;

    int  m_slideOffset = 0;
    bool m_visible     = false;
};
