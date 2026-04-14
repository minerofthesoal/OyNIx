#pragma once

#include <QObject>
#include <QJsonArray>
#include <QString>

class QTimer;

class HistoryManager : public QObject
{
    Q_OBJECT

public:
    explicit HistoryManager(const QString &dataDir, QObject *parent = nullptr);
    ~HistoryManager() override;

    void addEntry(const QString &url, const QString &title);
    [[nodiscard]] QJsonArray getAll() const;
    [[nodiscard]] QJsonArray search(const QString &query) const;
    [[nodiscard]] QJsonArray getRecent(int count = 20) const;
    void clear();
    void flush();

signals:
    void historyChanged();

private:
    void load();
    bool save() const;
    void trimEntries();
    void scheduleSave();

    static constexpr int MaxEntries = 5000;
    static constexpr int SaveDelayMs = 3000;  // batch writes: flush every 3s

    QString m_filePath;
    QJsonArray m_history;
    QTimer *m_saveTimer = nullptr;
    bool m_dirty = false;
};
