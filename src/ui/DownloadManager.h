#pragma once

#include <QWidget>
#include <QList>
#include <QUrl>

class QVBoxLayout;
class QLabel;
class QPushButton;
class QProgressBar;
class QWebEngineDownloadRequest;

/**
 * DownloadManager — Tracks and displays active/completed downloads.
 * Integrates with QWebEngineDownloadRequest for progress and control.
 */
class DownloadManager : public QWidget
{
    Q_OBJECT

public:
    explicit DownloadManager(QWidget *parent = nullptr);
    ~DownloadManager() override;

    void addDownload(QWebEngineDownloadRequest *download);
    [[nodiscard]] int activeCount() const;
    [[nodiscard]] int totalCount() const;

signals:
    void downloadCountChanged(int active, int total);

public slots:
    void clearCompleted();

private:
    struct DownloadItem {
        QWidget *widget = nullptr;
        QProgressBar *progressBar = nullptr;
        QLabel *statusLabel = nullptr;
        QLabel *nameLabel = nullptr;
        QPushButton *cancelBtn = nullptr;
        QPushButton *openBtn = nullptr;
        QString filePath;
        bool completed = false;
        bool cancelled = false;
    };

    void setupUi();
    QWidget *createDownloadWidget(QWebEngineDownloadRequest *download);
    void updateCountBadge();
    static QString formatBytes(qint64 bytes);

    QVBoxLayout *m_listLayout = nullptr;
    QLabel      *m_emptyLabel = nullptr;
    QPushButton *m_clearAllBtn = nullptr;
    QList<DownloadItem> m_items;
};
