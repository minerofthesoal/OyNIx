#pragma once

#include <QWebEnginePage>
#include <QWebEngineCertificateError>

class WebPage : public QWebEnginePage
{
    Q_OBJECT

public:
    explicit WebPage(QWebEngineProfile *profile, QObject *parent = nullptr);
    ~WebPage() override;

signals:
    void oynUrlRequested(const QString &path);

protected:
    bool acceptNavigationRequest(const QUrl &url,
                                 NavigationType type,
                                 bool isMainFrame) override;

    QWebEnginePage *createWindow(WebWindowType type) override;

    void javaScriptConsoleMessage(JavaScriptConsoleMessageLevel level,
                                  const QString &message,
                                  int lineNumber,
                                  const QString &sourceID) override;

private:
    void handleCertificateError(const QWebEngineCertificateError &error);
};
