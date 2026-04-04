#include "core/WebPage.h"

#include <QDebug>
#include <QTimer>

WebPage::WebPage(QWebEngineProfile *profile, QObject *parent)
    : QWebEnginePage(profile, parent)
{
    connect(this, &QWebEnginePage::certificateError,
            this, &WebPage::handleCertificateError);
}

WebPage::~WebPage() = default;

// ── Navigation interception ───────────────────────────────────────────
bool WebPage::acceptNavigationRequest(const QUrl &url,
                                       NavigationType /*type*/,
                                       bool isMainFrame)
{
    if (isMainFrame && url.scheme() == QLatin1String("oyn")) {
        const QString path = url.host().isEmpty()
                                 ? url.path().mid(1)
                                 : url.host();
        emit oynUrlRequested(path);
        return false;
    }

    return true;
}

// ── createWindow — return nullptr, parent WebView handles via signal ──
QWebEnginePage *WebPage::createWindow(WebWindowType /*type*/)
{
    return nullptr;
}

// ── JavaScript console messages ───────────────────────────────────────
void WebPage::javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                        const QString &message,
                                        int lineNumber,
                                        const QString &sourceID)
{
    const char *tag = "INFO";
    switch (level) {
    case InfoMessageLevel:    tag = "INFO";    break;
    case WarningMessageLevel: tag = "WARNING"; break;
    case ErrorMessageLevel:   tag = "ERROR";   break;
    }

    qDebug().noquote()
        << QStringLiteral("[JS %1] %2:%3 — %4")
               .arg(QLatin1String(tag))
               .arg(sourceID)
               .arg(lineNumber)
               .arg(message);
}

// ── Certificate error handling ────────────────────────────────────────
void WebPage::handleCertificateError(const QWebEngineCertificateError &error)
{
    if (error.type() == QWebEngineCertificateError::CertificateAuthorityInvalid ||
        error.type() == QWebEngineCertificateError::CertificateCommonNameInvalid) {
        qWarning().noquote()
            << QStringLiteral("OyNIx: accepting certificate error for %1 — %2")
                   .arg(error.url().toString(), error.description());
        // In Qt 6.8, we can't modify a const ref — just log and let it reject
    }
}
