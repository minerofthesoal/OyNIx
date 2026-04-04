#pragma once

#include <QObject>
#include <QJsonArray>
#include <QString>

class HistoryManager : public QObject
{
    Q_OBJECT

public:
    explicit HistoryManager(const QString &dataDir, QObject *parent = nullptr);
    ~HistoryManager() override = default;

    void addEntry(const QString &url, const QString &title);
    [[nodiscard]] QJsonArray getAll() const;
    [[nodiscard]] QJsonArray search(const QString &query) const;
    [[nodiscard]] QJsonArray getRecent(int count = 20) const;
    void clear();

signals:
    void historyChanged();

private:
    void load();
    bool save() const;
    void trimEntries();

    static constexpr int MaxEntries = 5000;

    QString m_filePath;
    QJsonArray m_history;
};
