#include "HistoryManager.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

HistoryManager::HistoryManager(const QString &dataDir, QObject *parent)
    : QObject(parent)
    , m_filePath(dataDir + QStringLiteral("/history.json"))
    , m_saveTimer(new QTimer(this))
{
    QDir().mkpath(dataDir);
    load();

    // Deferred save: coalesce rapid addEntry() calls into a single disk write
    m_saveTimer->setSingleShot(true);
    m_saveTimer->setInterval(SaveDelayMs);
    connect(m_saveTimer, &QTimer::timeout, this, &HistoryManager::flush);
}

HistoryManager::~HistoryManager()
{
    // Ensure any pending writes are flushed on shutdown
    if (m_dirty)
        save();
}

void HistoryManager::load()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isArray())
        m_history = doc.array();
}

bool HistoryManager::save() const
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly))
        return false;

    file.write(QJsonDocument(m_history).toJson(QJsonDocument::Compact));
    return true;
}

void HistoryManager::trimEntries()
{
    while (m_history.size() > MaxEntries)
        m_history.removeAt(0);
}

void HistoryManager::addEntry(const QString &url, const QString &title)
{
    QJsonObject entry;
    entry[QStringLiteral("url")] = url;
    entry[QStringLiteral("title")] = title;
    entry[QStringLiteral("time")] = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

    m_history.append(entry);
    trimEntries();

    m_dirty = true;
    scheduleSave();
    emit historyChanged();
}

void HistoryManager::scheduleSave()
{
    if (!m_saveTimer->isActive())
        m_saveTimer->start();
}

void HistoryManager::flush()
{
    if (!m_dirty)
        return;
    if (save())
        m_dirty = false;
}

QJsonArray HistoryManager::getAll() const
{
    return m_history;
}

QJsonArray HistoryManager::search(const QString &query) const
{
    QJsonArray results;
    const QString lowerQuery = query.toLower();

    for (const QJsonValue &v : m_history) {
        const QJsonObject obj = v.toObject();
        const QString url = obj[QStringLiteral("url")].toString().toLower();
        const QString title = obj[QStringLiteral("title")].toString().toLower();

        if (url.contains(lowerQuery) || title.contains(lowerQuery))
            results.append(v);
    }

    return results;
}

QJsonArray HistoryManager::getRecent(int count) const
{
    QJsonArray results;
    const int start = qMax(0, m_history.size() - count);

    for (int i = m_history.size() - 1; i >= start; --i)
        results.append(m_history[i]);

    return results;
}

void HistoryManager::clear()
{
    m_history = QJsonArray();
    m_dirty = true;
    flush();
    emit historyChanged();
}
