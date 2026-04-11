#pragma once

#include <QObject>
#include <QUrl>
#include <QSet>
#include <QJsonObject>

/**
 * SecurityManager — Login page detection, security prompts, and domain trust.
 * Detects Microsoft, Google, GitHub, Facebook, and banking login pages.
 */
class SecurityManager : public QObject
{
    Q_OBJECT

public:
    enum class SecurityLevel { High, Medium, Low };
    Q_ENUM(SecurityLevel)

    enum class SiteCategory { Microsoft, Google, GitHub, Facebook, Banking, Unknown };
    Q_ENUM(SiteCategory)

    static SecurityManager &instance();

    /// Check if URL is a known login page
    [[nodiscard]] bool isLoginPage(const QUrl &url) const;

    /// Get security level for a URL
    [[nodiscard]] SecurityLevel securityLevel(const QUrl &url) const;

    /// Get the category of a login site
    [[nodiscard]] SiteCategory siteCategory(const QUrl &url) const;

    /// Get human-readable name for category
    [[nodiscard]] static QString categoryName(SiteCategory cat);

    /// Domain trust management
    void trustDomain(const QString &domain);
    void blockDomain(const QString &domain);
    void removeTrust(const QString &domain);
    [[nodiscard]] bool isDomainTrusted(const QString &domain) const;
    [[nodiscard]] bool isDomainBlocked(const QString &domain) const;

    /// "Don't show again" per-domain
    void suppressForDomain(const QString &domain);
    [[nodiscard]] bool isSuppressed(const QString &domain) const;

    /// Security prompts enabled
    [[nodiscard]] bool promptsEnabled() const { return m_promptsEnabled; }
    void setPromptsEnabled(bool enabled);

    /// Should show a security prompt for this URL?
    [[nodiscard]] bool shouldPrompt(const QUrl &url) const;

    /// Get security info for display
    [[nodiscard]] QJsonObject securityInfo(const QUrl &url) const;

signals:
    void securityPromptRequired(const QUrl &url, const QJsonObject &info);

private:
    explicit SecurityManager(QObject *parent = nullptr);
    ~SecurityManager() override;

    void loadConfig();
    void saveConfig();

    bool m_promptsEnabled = true;
    QSet<QString> m_trustedDomains;
    QSet<QString> m_blockedDomains;
    QSet<QString> m_suppressedDomains;
};
