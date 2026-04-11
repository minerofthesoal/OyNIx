#pragma once

#include <QWebEngineView>
#include <QHash>
#include <QIcon>

class WebPage;
class QMenu;
class ExtensionManager;

class WebView : public QWebEngineView
{
    Q_OBJECT

public:
    explicit WebView(QWidget *parent = nullptr);
    ~WebView() override;

    [[nodiscard]] WebPage *webPage() const { return m_page; }
    [[nodiscard]] int loadProgress() const { return m_loadProgress; }
    [[nodiscard]] bool isAudioMuted() const;

    void setZoomForDomain(const QString &domain, qreal zoom);
    [[nodiscard]] qreal zoomForDomain(const QString &domain) const;

    /// Set the extension manager for content script injection
    void setExtensionManager(ExtensionManager *manager) { m_extensionManager = manager; }

public slots:
    void toggleAudioMute();

signals:
    void newTabRequested(const QUrl &url);
    void iconChanged(const QIcon &icon);
    void audioStateChanged(bool playing);

protected:
    void contextMenuEvent(QContextMenuEvent *event) override;
    QWebEngineView *createWindow(QWebEnginePage::WebWindowType type) override;

private:
    void setupPage();
    void onLoadStarted();
    void onLoadProgress(int progress);
    void onLoadFinished(bool ok);
    void onUrlChanged(const QUrl &url);
    void onIconChanged(const QIcon &icon);
    void onAudibleChanged(bool audible);

    void applyDomainZoom(const QUrl &url);
    void injectContentScripts(const QUrl &url);

    WebPage *m_page         = nullptr;
    int      m_loadProgress = 0;
    ExtensionManager *m_extensionManager = nullptr;

    static QHash<QString, qreal> s_domainZoomLevels;
};
