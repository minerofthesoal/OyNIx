#include "SecurityManager.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QStandardPaths>
#include <QRegularExpression>

// ── Known login URL patterns ─────────────────────────────────────────
struct LoginPattern {
    SecurityManager::SiteCategory category;
    QString pattern;
};

static const QList<LoginPattern> s_loginPatterns = {
    // Microsoft
    {SecurityManager::SiteCategory::Microsoft, QStringLiteral("login.microsoftonline.com")},
    {SecurityManager::SiteCategory::Microsoft, QStringLiteral("login.live.com")},
    {SecurityManager::SiteCategory::Microsoft, QStringLiteral("account.microsoft.com")},
    {SecurityManager::SiteCategory::Microsoft, QStringLiteral("login.windows.net")},
    // Google
    {SecurityManager::SiteCategory::Google, QStringLiteral("accounts.google.com")},
    {SecurityManager::SiteCategory::Google, QStringLiteral("myaccount.google.com")},
    // GitHub
    {SecurityManager::SiteCategory::GitHub, QStringLiteral("github.com/login")},
    {SecurityManager::SiteCategory::GitHub, QStringLiteral("github.com/session")},
    // Facebook
    {SecurityManager::SiteCategory::Facebook, QStringLiteral("facebook.com/login")},
    {SecurityManager::SiteCategory::Facebook, QStringLiteral("m.facebook.com/login")},
    {SecurityManager::SiteCategory::Facebook, QStringLiteral("instagram.com/accounts/login")},
    // Banking
    {SecurityManager::SiteCategory::Banking, QStringLiteral("online.bankofamerica.com")},
    {SecurityManager::SiteCategory::Banking, QStringLiteral("chase.com/web/auth")},
    {SecurityManager::SiteCategory::Banking, QStringLiteral("wellsfargo.com/signin")},
    {SecurityManager::SiteCategory::Banking, QStringLiteral("paypal.com/signin")},
    {SecurityManager::SiteCategory::Banking, QStringLiteral("secure.capitalone.com")},
    {SecurityManager::SiteCategory::Banking, QStringLiteral("citi.com/login")},
    {SecurityManager::SiteCategory::Banking, QStringLiteral("usbank.com/signin")},
    {SecurityManager::SiteCategory::Banking, QStringLiteral("ally.com/login")},
};

// ── Singleton ────────────────────────────────────────────────────────
SecurityManager &SecurityManager::instance()
{
    static SecurityManager s;
    return s;
}

SecurityManager::SecurityManager(QObject *parent)
    : QObject(parent)
{
    loadConfig();
}

SecurityManager::~SecurityManager()
{
    saveConfig();
}

// ── Login detection ──────────────────────────────────────────────────
bool SecurityManager::isLoginPage(const QUrl &url) const
{
    const QString fullUrl = url.toString().toLower();
    const QString host = url.host().toLower();

    for (const auto &lp : s_loginPatterns) {
        if (fullUrl.contains(lp.pattern) || host.contains(lp.pattern.split(QLatin1Char('/')).first()))
            return true;
    }

    // Generic login detection: path contains "login", "signin", "auth"
    const QString path = url.path().toLower();
    if (path.contains(QStringLiteral("/login")) ||
        path.contains(QStringLiteral("/signin")) ||
        path.contains(QStringLiteral("/auth")) ||
        path.contains(QStringLiteral("/sso"))) {
        return true;
    }

    return false;
}

SecurityManager::SecurityLevel SecurityManager::securityLevel(const QUrl &url) const
{
    if (isLoginPage(url))
        return SecurityLevel::High;

    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1String("https") && scheme != QLatin1String("oyn"))
        return SecurityLevel::Medium;

    return SecurityLevel::Low;
}

SecurityManager::SiteCategory SecurityManager::siteCategory(const QUrl &url) const
{
    const QString fullUrl = url.toString().toLower();
    const QString host = url.host().toLower();

    for (const auto &lp : s_loginPatterns) {
        if (fullUrl.contains(lp.pattern) || host.contains(lp.pattern.split(QLatin1Char('/')).first()))
            return lp.category;
    }
    return SiteCategory::Unknown;
}

QString SecurityManager::categoryName(SiteCategory cat)
{
    switch (cat) {
    case SiteCategory::Microsoft: return QStringLiteral("Microsoft");
    case SiteCategory::Google:    return QStringLiteral("Google");
    case SiteCategory::GitHub:    return QStringLiteral("GitHub");
    case SiteCategory::Facebook:  return QStringLiteral("Facebook");
    case SiteCategory::Banking:   return QStringLiteral("Banking");
    case SiteCategory::Unknown:   return QStringLiteral("Unknown");
    }
    return QStringLiteral("Unknown");
}

// ── Domain trust ─────────────────────────────────────────────────────
void SecurityManager::trustDomain(const QString &domain)
{
    m_trustedDomains.insert(domain.toLower());
    m_blockedDomains.remove(domain.toLower());
    saveConfig();
}

void SecurityManager::blockDomain(const QString &domain)
{
    m_blockedDomains.insert(domain.toLower());
    m_trustedDomains.remove(domain.toLower());
    saveConfig();
}

void SecurityManager::removeTrust(const QString &domain)
{
    m_trustedDomains.remove(domain.toLower());
    m_blockedDomains.remove(domain.toLower());
    saveConfig();
}

bool SecurityManager::isDomainTrusted(const QString &domain) const
{
    return m_trustedDomains.contains(domain.toLower());
}

bool SecurityManager::isDomainBlocked(const QString &domain) const
{
    return m_blockedDomains.contains(domain.toLower());
}

void SecurityManager::suppressForDomain(const QString &domain)
{
    m_suppressedDomains.insert(domain.toLower());
    saveConfig();
}

bool SecurityManager::isSuppressed(const QString &domain) const
{
    return m_suppressedDomains.contains(domain.toLower());
}

void SecurityManager::setPromptsEnabled(bool enabled)
{
    m_promptsEnabled = enabled;
    saveConfig();
}

// ── Should prompt? ───────────────────────────────────────────────────
bool SecurityManager::shouldPrompt(const QUrl &url) const
{
    if (!m_promptsEnabled) return false;
    if (!isLoginPage(url)) return false;

    const QString domain = url.host().toLower();
    if (isDomainTrusted(domain)) return false;
    if (isSuppressed(domain)) return false;
    if (isDomainBlocked(domain)) return true;  // still prompt for blocked

    return true;
}

QJsonObject SecurityManager::securityInfo(const QUrl &url) const
{
    QJsonObject info;
    info[QStringLiteral("url")]      = url.toString();
    info[QStringLiteral("is_login")] = isLoginPage(url);
    info[QStringLiteral("level")]    = static_cast<int>(securityLevel(url));
    info[QStringLiteral("category")] = categoryName(siteCategory(url));
    info[QStringLiteral("scheme")]   = url.scheme();
    info[QStringLiteral("trusted")]  = isDomainTrusted(url.host());
    info[QStringLiteral("blocked")]  = isDomainBlocked(url.host());
    return info;
}

// ── Persistence ──────────────────────────────────────────────────────
void SecurityManager::loadConfig()
{
    const QString path = QDir::homePath() + QStringLiteral("/.config/oynix/security.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;

    auto doc = QJsonDocument::fromJson(f.readAll());
    auto obj = doc.object();

    m_promptsEnabled = obj[QStringLiteral("prompts_enabled")].toBool(true);

    for (const auto &v : obj[QStringLiteral("trusted_domains")].toArray())
        m_trustedDomains.insert(v.toString());
    for (const auto &v : obj[QStringLiteral("blocked_domains")].toArray())
        m_blockedDomains.insert(v.toString());
    for (const auto &v : obj[QStringLiteral("suppressed_domains")].toArray())
        m_suppressedDomains.insert(v.toString());
}

void SecurityManager::saveConfig()
{
    const QString dir = QDir::homePath() + QStringLiteral("/.config/oynix");
    QDir().mkpath(dir);

    QJsonObject obj;
    obj[QStringLiteral("prompts_enabled")] = m_promptsEnabled;

    QJsonArray trusted, blocked, suppressed;
    for (const auto &d : m_trustedDomains) trusted.append(d);
    for (const auto &d : m_blockedDomains) blocked.append(d);
    for (const auto &d : m_suppressedDomains) suppressed.append(d);

    obj[QStringLiteral("trusted_domains")]    = trusted;
    obj[QStringLiteral("blocked_domains")]    = blocked;
    obj[QStringLiteral("suppressed_domains")] = suppressed;

    QFile f(dir + QStringLiteral("/security.json"));
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(obj).toJson());
}
