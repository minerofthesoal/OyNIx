#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QUrl>

class TabWidget;

/**
 * SessionManager — Save and restore browser sessions.
 * Manages named sessions and auto-save on exit.
 */
class SessionManager : public QObject
{
    Q_OBJECT

public:
    explicit SessionManager(QObject *parent = nullptr);
    ~SessionManager() override;

    /// Save current session (all open tabs)
    bool saveSession(TabWidget *tabWidget, const QString &name = QStringLiteral("default"));

    /// Restore a saved session
    [[nodiscard]] QJsonArray loadSession(const QString &name = QStringLiteral("default")) const;

    /// List available session names
    [[nodiscard]] QStringList availableSessions() const;

    /// Delete a saved session
    bool deleteSession(const QString &name);

    /// Auto-save session
    void setAutoSave(bool enabled) { m_autoSave = enabled; }
    [[nodiscard]] bool autoSave() const { return m_autoSave; }

signals:
    void sessionSaved(const QString &name);
    void sessionLoaded(const QString &name, int tabCount);

private:
    QString sessionDir() const;
    QString sessionFilePath(const QString &name) const;

    bool m_autoSave = true;
};
