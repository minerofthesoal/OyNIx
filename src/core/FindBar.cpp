/*
 * OyNIx Browser v3.1 — FindBar implementation
 * Find-in-page bar with slide-in animation, match counting, and
 * theme-consistent glass styling.
 */

#include "FindBar.h"
#include "theme/ThemeEngine.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QStyle>
#include <QWebEngineFindTextResult>
#include <QWebEnginePage>
#include <QWebEngineView>

// ── Construction ─────────────────────────────────────────────────────

FindBar::FindBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(40);
    setVisible(false);

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 2, 8, 2);
    layout->setSpacing(4);

    // Search input
    m_searchInput = new QLineEdit(this);
    m_searchInput->setPlaceholderText(tr("Find in page…"));
    m_searchInput->setMinimumWidth(200);
    m_searchInput->setClearButtonEnabled(true);
    layout->addWidget(m_searchInput);

    // Match count label
    m_matchLabel = new QLabel(this);
    m_matchLabel->setMinimumWidth(60);
    m_matchLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_matchLabel);

    // Previous button
    m_prevButton = new QPushButton(this);
    m_prevButton->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    m_prevButton->setToolTip(tr("Previous match"));
    m_prevButton->setFixedSize(28, 28);
    layout->addWidget(m_prevButton);

    // Next button
    m_nextButton = new QPushButton(this);
    m_nextButton->setIcon(style()->standardIcon(QStyle::SP_ArrowDown));
    m_nextButton->setToolTip(tr("Next match"));
    m_nextButton->setFixedSize(28, 28);
    layout->addWidget(m_nextButton);

    // Close button
    m_closeButton = new QPushButton(this);
    m_closeButton->setIcon(style()->standardIcon(QStyle::SP_TitleBarCloseButton));
    m_closeButton->setToolTip(tr("Close find bar"));
    m_closeButton->setFixedSize(28, 28);
    layout->addWidget(m_closeButton);

    // Slide animation on the custom slideOffset property
    m_animation = new QPropertyAnimation(this, "slideOffset", this);
    m_animation->setDuration(200);
    m_animation->setEasingCurve(QEasingCurve::OutCubic);

    // Connections
    connect(m_searchInput, &QLineEdit::textChanged,
            this, &FindBar::onTextChanged);
    connect(m_nextButton,  &QPushButton::clicked,
            this, &FindBar::findNext);
    connect(m_prevButton,  &QPushButton::clicked,
            this, &FindBar::findPrev);
    connect(m_closeButton, &QPushButton::clicked,
            this, &FindBar::hideBar);

    applyStyle();
}

FindBar::~FindBar() = default;

// ── Public API ───────────────────────────────────────────────────────

void FindBar::setWebView(QWebEngineView *view)
{
    m_webView = view;
}

void FindBar::showBar()
{
    if (m_visible) {
        m_searchInput->setFocus();
        m_searchInput->selectAll();
        return;
    }

    m_visible = true;
    setVisible(true);

    // Animate from below
    m_animation->stop();
    m_animation->setStartValue(height());
    m_animation->setEndValue(0);
    m_animation->start();

    m_searchInput->setFocus();
    m_searchInput->selectAll();
}

void FindBar::hideBar()
{
    if (!m_visible) return;

    m_visible = false;

    // Clear any active find highlighting
    if (m_webView)
        m_webView->findText(QString());

    // Animate slide-out
    m_animation->stop();
    m_animation->setStartValue(0);
    m_animation->setEndValue(height());
    m_animation->start();

    connect(m_animation, &QPropertyAnimation::finished, this, [this]{
        if (!m_visible)
            setVisible(false);
    }, Qt::SingleShotConnection);

    m_matchLabel->clear();
    emit hidden();
}

void FindBar::setSlideOffset(int offset)
{
    if (m_slideOffset == offset) return;
    m_slideOffset = offset;

    // Translate the widget vertically
    QWidget *p = parentWidget();
    if (p) {
        const int baseY = p->height() - height();
        move(x(), baseY + offset);
    }
    update();
}

// ── Key handling ─────────────────────────────────────────────────────

void FindBar::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        hideBar();
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (event->modifiers() & Qt::ShiftModifier)
            findPrev();
        else
            findNext();
        return;
    }
    QWidget::keyPressEvent(event);
}

// ── Find logic ───────────────────────────────────────────────────────

void FindBar::findNext()
{
    if (!m_webView || !m_searchInput) return;
    const QString text = m_searchInput->text();
    if (text.isEmpty()) return;

    m_webView->findText(text, {}, [this](const QWebEngineFindTextResult &result){
        updateMatchCount(result.activeMatch(), result.numberOfMatches());
    });
}

void FindBar::findPrev()
{
    if (!m_webView || !m_searchInput) return;
    const QString text = m_searchInput->text();
    if (text.isEmpty()) return;

    m_webView->findText(text, QWebEnginePage::FindBackward,
                        [this](const QWebEngineFindTextResult &result){
        updateMatchCount(result.activeMatch(), result.numberOfMatches());
    });
}

void FindBar::onTextChanged(const QString &text)
{
    if (!m_webView) return;

    if (text.isEmpty()) {
        m_webView->findText(QString());
        m_matchLabel->clear();
        return;
    }

    m_webView->findText(text, {}, [this](const QWebEngineFindTextResult &result){
        updateMatchCount(result.activeMatch(), result.numberOfMatches());
    });
}

void FindBar::updateMatchCount(int current, int total)
{
    if (!m_matchLabel) return;

    if (total == 0 && !m_searchInput->text().isEmpty()) {
        m_matchLabel->setText(tr("No matches"));
        m_matchLabel->setStyleSheet(QStringLiteral("color: ") +
            ThemeEngine::instance().getColor(QStringLiteral("error")) + QStringLiteral(";"));
    } else if (total > 0) {
        m_matchLabel->setText(tr("%1 of %2").arg(current).arg(total));
        m_matchLabel->setStyleSheet(QStringLiteral("color: ") +
            ThemeEngine::instance().getColor(QStringLiteral("text-secondary")) + QStringLiteral(";"));
    } else {
        m_matchLabel->clear();
    }
}

// ── Styling ──────────────────────────────────────────────────────────

void FindBar::applyStyle()
{
    const auto &c = ThemeEngine::instance().colors();
    QString ss;

    ss += QStringLiteral("FindBar { background: ") + c["bg-darkest"]
       + QStringLiteral("; border-top: 2px solid ") + c["purple-mid"]
       + QStringLiteral("; }\n");

    ss += QStringLiteral("QLineEdit { background: ") + c["bg-mid"]
       + QStringLiteral("; color: ") + c["text-primary"]
       + QStringLiteral("; border: 1px solid ") + c["border"]
       + QStringLiteral("; border-radius: 8px; padding: 5px 10px;"
                        " selection-background-color: ") + c["selection"]
       + QStringLiteral("; font-size: 13px; }\n");
    ss += QStringLiteral("QLineEdit:focus { border-color: ") + c["purple-mid"]
       + QStringLiteral("; background: ") + c["bg-light"]
       + QStringLiteral("; }\n");

    ss += QStringLiteral("QPushButton { background: ") + c["bg-mid"]
       + QStringLiteral("; color: ") + c["text-primary"]
       + QStringLiteral("; border: 1px solid ") + c["border"]
       + QStringLiteral("; border-radius: 6px; }\n");
    ss += QStringLiteral("QPushButton:hover { background: ") + c["purple-dark"]
       + QStringLiteral("; border-color: ") + c["purple-mid"]
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QPushButton:pressed { background: ") + c["purple-mid"]
       + QStringLiteral("; color: ") + c["bg-darkest"]
       + QStringLiteral("; }\n");

    ss += QStringLiteral("QLabel { color: ") + c["text-secondary"]
       + QStringLiteral("; font-size: 12px; background: transparent; }\n");

    setStyleSheet(ss);
}
