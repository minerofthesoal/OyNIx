#pragma once

#include <QObject>
#include <QJsonArray>
#include <QString>

class BookmarkManager : public QObject
{
    Q_OBJECT

public:
    explicit BookmarkManager(const QString &dataDir, QObject *parent = nullptr);
    ~BookmarkManager() override = default;

    bool add(const QString &url, const QString &title, const QString &folder = QStringLiteral("Unsorted"));
    bool remove(const QString &url);
    [[nodiscard]] bool isBookmarked(const QString &url) const;

    [[nodiscard]] QJsonArray getAll() const;
    [[nodiscard]] QJsonArray getByFolder(const QString &folder) const;

    bool importBookmarks(const QString &path);
    bool exportBookmarks(const QString &path) const;

signals:
    void bookmarksChanged();

private:
    void load();
    bool save() const;

    QString m_filePath;
    QJsonArray m_bookmarks;
};
