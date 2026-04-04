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
    bool removeBookmark(const QString &url) { return remove(url); }
    [[nodiscard]] bool isBookmarked(const QString &url) const;

    [[nodiscard]] QJsonArray getAll() const;
    [[nodiscard]] QJsonArray getByFolder(const QString &folder) const;

    // Folder operations
    [[nodiscard]] QStringList folders() const;
    [[nodiscard]] QJsonArray bookmarksInFolder(const QString &folder) const { return getByFolder(folder); }
    [[nodiscard]] QJsonArray unsortedBookmarks() const { return getByFolder(QStringLiteral("Unsorted")); }
    void addFolder(const QString &name);
    void renameFolder(const QString &oldName, const QString &newName);

    bool importBookmarks(const QString &path);
    bool importFromFile(const QString &path) { return importBookmarks(path); }
    bool exportBookmarks(const QString &path) const;
    bool exportToFile(const QString &path) const { return exportBookmarks(path); }

signals:
    void bookmarksChanged();

private:
    void load();
    bool save() const;

    QString m_filePath;
    QJsonArray m_bookmarks;
};
