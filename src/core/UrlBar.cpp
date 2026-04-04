/*
 * OyNIx Browser v3.0 — UrlBar implementation
 * Custom URL bar with security indicator, autocomplete, and smart input detection.
 */

#include "UrlBar.h"

#include <QAction>
#include <QCompleter>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QRegularExpression>
#include <QStringListModel>
#include <QStyle>

// ── Construction ─────────────────────────────────────────────────────

UrlBar::UrlBar(QWidget *parent)
    : QLineEdit(parent)
{
    setPlaceholderText(tr("Search or enter URL"));
    setClearButtonEnabled(true);
    setMinimumWidth(300);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // Security indicator (leading action)
    m_securityAction = addAction(style()->standardIcon(QStyle::SP_DriveNetIcon),
                                 QLineEdit::LeadingPosition);
    m_securityAction->setToolTip(tr("Connection security"));

    // History-based completer
    m_historyModel = new QStringListModel(this);
    m_completer = new QCompleter(m_historyModel, this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setFilterMode(Qt::MatchContains);
    m_completer->setMaxVisibleItems(10);
    setCompleter(m_completer);

    // Process input on Return
    connect(this, &QLineEdit::returnPressed, this, &UrlBar::processInput);

    applyStyle();
}

UrlBar::~UrlBar() = default;

// ── Public API ───────────────────────────────────────────────────────

void UrlBar::setDisplayUrl(const QUrl &url)
{
    m_currentUrl = url;
    if (!m_focused) {
        // Show domain only when unfocused
        const QString host = url.host();
        setText(host.isEmpty() ? url.toString() : host);
    } else {
        setText(url.toString());
    }
}

void UrlBar::setSecurityIcon(const QString &iconPath)
{
    if (!m_securityAction) return;

    if (iconPath == QLatin1String("lock")) {
        m_securityAction->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
        m_securityAction->setToolTip(tr("Secure connection (HTTPS)"));
    } else if (iconPath == QLatin1String("internal")) {
        m_securityAction->setIcon(style()->standardIcon(QStyle::SP_DriveNetIcon));
        m_securityAction->setToolTip(tr("Internal page"));
    } else {
        m_securityAction->setIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
        m_securityAction->setToolTip(tr("Connection is not secure"));
    }
}

void UrlBar::addHistoryEntry(const QString &url)
{
    if (!m_historyList.contains(url)) {
        m_historyList.prepend(url);
        // Cap at 500 entries for performance
        if (m_historyList.size() > 500)
            m_historyList.removeLast();
        m_historyModel->setStringList(m_historyList);
    }
}

// ── Focus events ─────────────────────────────────────────────────────

void UrlBar::focusInEvent(QFocusEvent *event)
{
    m_focused = true;
    // Show full URL when focused
    setText(m_currentUrl.toString());
    selectAll();
    QLineEdit::focusInEvent(event);
}

void UrlBar::focusOutEvent(QFocusEvent *event)
{
    m_focused = false;
    // Return to domain-only display
    setDisplayUrl(m_currentUrl);
    QLineEdit::focusOutEvent(event);
}

// ── Key handling ─────────────────────────────────────────────────────

void UrlBar::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        // Restore original URL and defocus
        setDisplayUrl(m_currentUrl);
        clearFocus();
        return;
    }
    QLineEdit::keyPressEvent(event);
}

// ── Input processing ─────────────────────────────────────────────────

void UrlBar::processInput()
{
    const QString input = text().trimmed();
    if (input.isEmpty())
        return;

    if (looksLikeUrl(input)) {
        QUrl url = QUrl::fromUserInput(input);
        m_currentUrl = url;
        addHistoryEntry(url.toString());
        emit urlEntered(url);
    } else {
        emit searchRequested(input);
    }
}

bool UrlBar::looksLikeUrl(const QString &input) const
{
    // Explicit scheme
    if (input.startsWith(QLatin1String("http://"))  ||
        input.startsWith(QLatin1String("https://")) ||
        input.startsWith(QLatin1String("oyn://"))   ||
        input.startsWith(QLatin1String("file://")))
    {
        return true;
    }

    // localhost or IP
    if (input.startsWith(QLatin1String("localhost")) ||
        input.startsWith(QLatin1String("127.")))
    {
        return true;
    }

    // Domain-like pattern (word.tld or word.tld/path)
    static const QRegularExpression domainRe(
        QStringLiteral(R"(^[\w\-]+(\.[\w\-]+)+(/.*)?$)"));
    if (domainRe.match(input).hasMatch())
        return true;

    return false;
}

// ── Styling ──────────────────────────────────────────────────────────

void UrlBar::applyStyle()
{
    // Inherits from ThemeEngine global stylesheet (QLineEdit rules).
    // Only override the few bits specific to the URL bar.
    setStyleSheet(QString());
}
