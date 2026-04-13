/*
 * OyNIx Browser v3.1 — UrlBar implementation
 * Custom URL bar with security indicator, smart autocomplete, and
 * protocol-aware input detection.
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
    setPlaceholderText(tr("Search or enter URL — Ctrl+L"));
    setClearButtonEnabled(true);
    setMinimumWidth(300);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    // Security indicator (leading action)
    m_securityAction = addAction(style()->standardIcon(QStyle::SP_DriveNetIcon),
                                 QLineEdit::LeadingPosition);
    m_securityAction->setToolTip(tr("Connection security"));

    // History-based completer with better matching
    m_historyModel = new QStringListModel(this);
    m_completer = new QCompleter(m_historyModel, this);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setFilterMode(Qt::MatchContains);
    m_completer->setMaxVisibleItems(12);
    m_completer->setCompletionMode(QCompleter::PopupCompletion);
    setCompleter(m_completer);

    // Style the completer popup
    if (auto *popup = m_completer->popup()) {
        popup->setStyleSheet(QStringLiteral(
            "QListView {"
            "  background: #24263a; color: #c8cad8;"
            "  border: 1px solid #383b52; border-radius: 10px;"
            "  padding: 4px; outline: none;"
            "}"
            "QListView::item {"
            "  padding: 6px 12px; border-radius: 6px;"
            "}"
            "QListView::item:selected {"
            "  background: #3d3a6b; color: #d4d2ee;"
            "}"
            "QListView::item:hover:!selected {"
            "  background: #2a2d42;"
            "}"
        ));
    }

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
        const QString scheme = url.scheme();
        if (scheme == QLatin1String("oyn")) {
            // Show internal page name nicely
            const QString path = url.host();
            setText(QStringLiteral("oyn://") + path);
        } else {
            // Show domain only when unfocused, with scheme hint
            const QString host = url.host();
            if (host.isEmpty()) {
                setText(url.toString());
            } else {
                setText(host);
            }
        }
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
        m_securityAction->setToolTip(tr("OyNIx internal page"));
    } else {
        m_securityAction->setIcon(style()->standardIcon(QStyle::SP_MessageBoxWarning));
        m_securityAction->setToolTip(tr("Connection is not secure"));
    }
}

void UrlBar::addHistoryEntry(const QString &url)
{
    // Move to front if already exists (most-recent first)
    int idx = m_historyList.indexOf(url);
    if (idx >= 0)
        m_historyList.removeAt(idx);

    m_historyList.prepend(url);

    // Cap at 1000 entries
    if (m_historyList.size() > 1000)
        m_historyList.removeLast();

    m_historyModel->setStringList(m_historyList);
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
        setDisplayUrl(m_currentUrl);
        clearFocus();
        return;
    }

    // Ctrl+Enter — auto-complete .com
    if (event->key() == Qt::Key_Return && (event->modifiers() & Qt::ControlModifier)) {
        QString input = text().trimmed();
        if (!input.contains(QLatin1Char('.')) && !input.contains(QLatin1String("://"))) {
            setText(QStringLiteral("https://www.") + input + QStringLiteral(".com"));
        }
        processInput();
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
        input.startsWith(QLatin1String("file://"))  ||
        input.startsWith(QLatin1String("ftp://")))
    {
        return true;
    }

    // localhost, IP, or port notation
    if (input.startsWith(QLatin1String("localhost")) ||
        input.startsWith(QLatin1String("127.")) ||
        input.startsWith(QLatin1String("192.168.")) ||
        input.startsWith(QLatin1String("10.")) ||
        input.startsWith(QLatin1String("[::1]")))
    {
        return true;
    }

    // hostname:port pattern (e.g. "myserver:8080")
    static const QRegularExpression portRe(
        QStringLiteral(R"(^[\w\.\-]+:\d{1,5}(/.*)?$)"));
    if (portRe.match(input).hasMatch())
        return true;

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
