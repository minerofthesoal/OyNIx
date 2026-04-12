#include "InternalPages.h"
#include <QJsonDocument>
#include <QUrl>
#include <QDateTime>
#include <QTimeZone>

namespace InternalPages {

// ── Shared CSS ──────────────────────────────────────────────────────
QString sharedCss(const QMap<QString, QString> &c) {
    return QStringLiteral(R"CSS(
* { margin: 0; padding: 0; box-sizing: border-box; }
body { background: %1; color: %2; font-family: 'Inter', -apple-system, 'Segoe UI', system-ui, sans-serif;
  min-height: 100vh; overflow-x: hidden; line-height: 1.6; }
a { color: %3; text-decoration: none; transition: color .2s ease; }
a:hover { color: %4; }

@keyframes fadeIn { from { opacity: 0; } to { opacity: 1; } }
@keyframes slideUp { from { opacity: 0; transform: translateY(16px); } to { opacity: 1; transform: translateY(0); } }

.wrap { display: flex; flex-direction: column; align-items: center;
  justify-content: center; min-height: 100vh; padding: 40px 24px; }

.container { max-width: 900px; margin: 0 auto; padding: 32px 24px; }

.card { background: %5; border: 1px solid %6; border-radius: 12px;
  padding: 18px; transition: border-color .2s, background .2s, box-shadow .2s; }
.card:hover { border-color: %7; background: %8;
  box-shadow: 0 4px 20px rgba(110,106,179,0.08); }

.header { text-align: center; margin-bottom: 40px; }

.section-title { font-size: 1.05em; font-weight: 600; color: %3;
  margin: 28px 0 14px; padding-bottom: 8px; border-bottom: 1px solid %6;
  letter-spacing: 0.02em; }

.btn { display: inline-block; padding: 8px 20px; border-radius: 8px;
  border: 1px solid %6; color: %2; cursor: pointer; background: transparent;
  font-size: 0.85em; transition: all .2s ease; text-decoration: none; }
.btn:hover { border-color: %7; background: %5; }
.btn-accent { background: %7; color: %1; border-color: %7; font-weight: 600; }
.btn-accent:hover { background: %3; }

.badge { display: inline-block; padding: 2px 10px; border-radius: 6px;
  font-size: .72em; font-weight: 600; letter-spacing: 0.03em; }
)CSS")
        .arg(c["bg-darkest"], c["text-primary"], c["purple-light"],
             c["purple-glow"], c["bg-mid"], c["border"],
             c["purple-mid"], c["bg-light"]);
}

// ── Homepage — rich new-tab page with greeting, search, stats, history ──
QString homePage(const QMap<QString, QString> &c,
                 const QJsonObject &stats,
                 const QJsonArray &recentHistory,
                 const QJsonArray &bookmarks) {

    // Time-based greeting
    const int hour = QDateTime::currentDateTime().time().hour();
    QString greeting;
    if (hour < 5)        greeting = QStringLiteral("Good night");
    else if (hour < 12)  greeting = QStringLiteral("Good morning");
    else if (hour < 17)  greeting = QStringLiteral("Good afternoon");
    else                 greeting = QStringLiteral("Good evening");

    // Format current date
    const QString dateStr = QDateTime::currentDateTime().toString(QStringLiteral("dddd, MMMM d"));

    // Stats
    const int indexedPages = stats[QStringLiteral("indexed_pages")].toInt();
    const int dbSites = stats[QStringLiteral("database_sites")].toInt();
    const int totalVisits = stats[QStringLiteral("total_visits")].toInt();

    // Build recent history items (max 6)
    QString historyItems;
    int hCount = 0;
    for (const auto &val : recentHistory) {
        if (hCount >= 6) break;
        auto h = val.toObject();
        QString title = h[QStringLiteral("title")].toString();
        QString url = h[QStringLiteral("url")].toString();
        if (title.isEmpty()) title = url;
        if (title.length() > 40) title = title.left(37) + QStringLiteral("...");
        QString domain = QUrl(url).host();
        historyItems += QStringLiteral(
            "<a class='recent-item' href='%1'>"
            "<div class='recent-title'>%2</div>"
            "<div class='recent-domain'>%3</div>"
            "</a>")
            .arg(url.toHtmlEscaped(), title.toHtmlEscaped(), domain.toHtmlEscaped());
        hCount++;
    }

    // Build top bookmarks (max 4)
    QString bookmarkItems;
    int bCount = 0;
    for (const auto &val : bookmarks) {
        if (bCount >= 4) break;
        auto b = val.toObject();
        QString title = b[QStringLiteral("title")].toString();
        QString url = b[QStringLiteral("url")].toString();
        if (title.isEmpty()) title = url;
        if (title.length() > 30) title = title.left(27) + QStringLiteral("...");
        bookmarkItems += QStringLiteral(
            "<a class='bm-chip' href='%1'>%2</a>")
            .arg(url.toHtmlEscaped(), title.toHtmlEscaped());
        bCount++;
    }

    return QStringLiteral(R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>OyNIx — New Tab</title>
<style>
%1

.home-bg { position: fixed; inset: 0; z-index: -1;
  background: radial-gradient(ellipse at 30%% 20%%, rgba(110,106,179,0.08) 0%%, transparent 50%%),
              radial-gradient(ellipse at 70%% 80%%, rgba(123,79,191,0.06) 0%%, transparent 50%%),
              %2; }

.home-wrap { display: flex; flex-direction: column; align-items: center;
  min-height: 100vh; padding: 0 24px; }

/* Top section */
.top-section { width: 100%%; max-width: 720px; padding-top: 14vh;
  text-align: center; animation: fadeIn .6s ease; }

.greeting { font-size: 1.6em; font-weight: 300; color: %3;
  margin-bottom: 2px; letter-spacing: -0.01em; }
.date-display { font-size: .82em; color: %4; margin-bottom: 36px; }

/* Logo */
.logo-mark { font-size: 2.2em; font-weight: 800; color: %5;
  letter-spacing: -2px; margin-bottom: 32px;
  background: linear-gradient(135deg, %6, %7);
  -webkit-background-clip: text; -webkit-text-fill-color: transparent; }

/* Search */
.search-area { width: 100%%; max-width: 580px; margin: 0 auto 40px;
  position: relative; }
.search-input { width: 100%%; padding: 14px 48px 14px 20px; font-size: 1em;
  background: %8; border: 1px solid %9; border-radius: 12px;
  color: %3; outline: none; transition: border-color .2s, box-shadow .2s; }
.search-input:focus { border-color: %6;
  box-shadow: 0 0 0 3px rgba(110,106,179,0.15); }
.search-input::placeholder { color: %4; }
.search-icon { position: absolute; right: 16px; top: 50%%; transform: translateY(-50%%);
  color: %4; font-size: 1.1em; pointer-events: none; }
.search-hint { text-align: center; margin-top: 8px; font-size: .72em; color: %4; }

/* Quick nav */
.quick-nav { display: flex; gap: 8px; justify-content: center; flex-wrap: wrap;
  margin-bottom: 40px; animation: slideUp .5s .1s ease both; }
.nav-pill { display: flex; align-items: center; gap: 6px;
  padding: 8px 16px; border-radius: 10px; text-decoration: none;
  color: %3; background: %8; border: 1px solid %9;
  font-size: .82em; font-weight: 500; transition: all .2s ease; }
.nav-pill:hover { border-color: %6; background: %10;
  box-shadow: 0 2px 12px rgba(110,106,179,0.1); transform: translateY(-1px); }
.nav-pill .pill-icon { font-size: 1em; }

/* Stats bar */
.stats-bar { display: flex; gap: 32px; justify-content: center;
  margin-bottom: 36px; animation: slideUp .5s .2s ease both; }
.stat-item { text-align: center; }
.stat-value { font-size: 1.5em; font-weight: 700; color: %6; }
.stat-label { font-size: .7em; color: %4; text-transform: uppercase;
  letter-spacing: 0.08em; margin-top: 2px; }

/* Recent history */
.section-heading { font-size: .8em; font-weight: 600; color: %4;
  text-transform: uppercase; letter-spacing: 0.1em; margin-bottom: 12px;
  text-align: left; max-width: 680px; width: 100%%; }

.recent-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
  gap: 10px; max-width: 680px; width: 100%%; margin-bottom: 32px;
  animation: slideUp .5s .3s ease both; }
.recent-item { display: block; padding: 14px 16px; border-radius: 10px;
  background: %8; border: 1px solid %9; text-decoration: none;
  transition: all .2s ease; overflow: hidden; }
.recent-item:hover { border-color: %6; background: %10; transform: translateY(-2px);
  box-shadow: 0 4px 16px rgba(110,106,179,0.1); }
.recent-title { font-size: .85em; font-weight: 600; color: %3;
  white-space: nowrap; overflow: hidden; text-overflow: ellipsis; margin-bottom: 4px; }
.recent-domain { font-size: .72em; color: %4; }

/* Bookmark chips */
.bm-row { display: flex; gap: 8px; flex-wrap: wrap; max-width: 680px; width: 100%%;
  margin-bottom: 32px; animation: slideUp .5s .35s ease both; }
.bm-chip { padding: 6px 14px; border-radius: 8px; font-size: .78em;
  background: rgba(110,106,179,0.12); color: %7; border: 1px solid rgba(110,106,179,0.2);
  text-decoration: none; transition: all .2s ease; }
.bm-chip:hover { background: rgba(110,106,179,0.22); border-color: %6; }

/* Footer */
.home-footer { position: fixed; bottom: 0; left: 0; right: 0;
  display: flex; justify-content: space-between; padding: 8px 20px;
  font-size: .68em; color: %4; }
.home-footer a { color: %4; }
.home-footer a:hover { color: %7; }

/* Keyboard shortcuts overlay */
.kbd { display: inline-block; padding: 1px 6px; border-radius: 4px;
  background: rgba(255,255,255,0.06); border: 1px solid rgba(255,255,255,0.1);
  font-family: monospace; font-size: .85em; }
</style></head><body>
<div class="home-bg"></div>
<div class="home-wrap">
  <div class="top-section">
    <div class="greeting">%11</div>
    <div class="date-display">%12</div>
    <div class="logo-mark">OyNIx</div>
  </div>

  <div class="search-area">
    <input class="search-input" id="searchInput"
      placeholder="Search with Nyx or enter a URL..." autofocus
      onkeydown="if(event.key==='Enter'){var v=this.value.trim();if(v)window.location='oyn://search?q='+encodeURIComponent(v)}">
    <span class="search-icon">&#x2315;</span>
    <div class="search-hint">Press <span class="kbd">Enter</span> to search &middot; <span class="kbd">Ctrl+L</span> focus URL bar</div>
  </div>

  <div class="quick-nav">
    <a class="nav-pill" href="oyn://bookmarks"><span class="pill-icon">&#9733;</span> Bookmarks</a>
    <a class="nav-pill" href="oyn://history"><span class="pill-icon">&#8635;</span> History</a>
    <a class="nav-pill" href="oyn://downloads"><span class="pill-icon">&#8615;</span> Downloads</a>
    <a class="nav-pill" href="oyn://settings"><span class="pill-icon">&#9881;</span> Settings</a>
    <a class="nav-pill" href="oyn://profiles"><span class="pill-icon">&#9673;</span> Profiles</a>
  </div>

  <div class="stats-bar">
    <div class="stat-item"><div class="stat-value">%13</div><div class="stat-label">Pages Indexed</div></div>
    <div class="stat-item"><div class="stat-value">%14</div><div class="stat-label">Sites in DB</div></div>
    <div class="stat-item"><div class="stat-value">%15</div><div class="stat-label">Total Visits</div></div>
  </div>

  %16

  %17
</div>

<div class="home-footer">
  <span>OyNIx Browser v3.1</span>
  <span><span class="kbd">Ctrl+K</span> Command Palette &middot; <span class="kbd">Ctrl+Shift+A</span> AI Assistant</span>
</div>

<script>document.getElementById('searchInput').focus();</script>
</body></html>)HTML")
        .arg(sharedCss(c))                    // %1
        .arg(c["bg-darkest"])                 // %2
        .arg(c["text-primary"])               // %3
        .arg(c["text-muted"])                 // %4
        .arg(c["text-primary"])               // %5
        .arg(c["purple-mid"])                 // %6
        .arg(c["purple-light"])               // %7
        .arg(c["bg-mid"])                     // %8
        .arg(c["border"])                     // %9
        .arg(c["bg-light"])                   // %10
        .arg(greeting)                        // %11
        .arg(dateStr)                         // %12
        .arg(QString::number(indexedPages))   // %13
        .arg(QString::number(dbSites))        // %14
        .arg(QString::number(totalVisits))    // %15
        .arg(historyItems.isEmpty() ? QString() :
             QStringLiteral("<div class='section-heading'>Recently Visited</div>"
                            "<div class='recent-grid'>%1</div>").arg(historyItems))  // %16
        .arg(bookmarkItems.isEmpty() ? QString() :
             QStringLiteral("<div class='section-heading'>Bookmarks</div>"
                            "<div class='bm-row'>%1</div>").arg(bookmarkItems));     // %17
}

// ── Search Results ─────────────────────────────────────────────────
QString searchPage(const QString &query,
                   const QJsonArray &localResults,
                   const QJsonArray &webResults,
                   const QMap<QString, QString> &c) {
    auto buildCards = [&](const QJsonArray &results, const QString &badgeClass) -> QString {
        QString html;
        int i = 0;
        for (const auto &val : results) {
            auto r = val.toObject();
            QString delay = QString::number(0.08 + i * 0.04, 'f', 2);
            QString url = r[QStringLiteral("url")].toString().toHtmlEscaped();
            QString title = r[QStringLiteral("title")].toString().toHtmlEscaped();
            QString desc = r[QStringLiteral("description")].toString().toHtmlEscaped();
            QString domain = QUrl(r[QStringLiteral("url")].toString()).host().toHtmlEscaped();
            if (title.isEmpty()) title = url;

            html += QStringLiteral(
                "<div class='result' style='animation: slideUp .35s %1s ease both;'>"
                "<div class='r-meta'>"
                "<span class='r-domain'>%2</span>"
                "<span class='badge %3'>%3</span>"
                "</div>"
                "<a class='r-title' href='%4'>%5</a>"
                "<div class='r-url'>%4</div>"
                "<div class='r-desc'>%6</div>"
                "</div>")
                .arg(delay, domain, badgeClass, url, title, desc);
            i++;
        }
        return html;
    };

    int total = localResults.size() + webResults.size();

    QString noResults;
    if (total == 0 && webResults.isEmpty()) {
        noResults = QStringLiteral(
            "<div class='no-results'>"
            "<div class='no-results-icon'>&#128269;</div>"
            "<div class='no-results-text'>No results found for \"%1\"</div>"
            "<div class='no-results-hint'>Try different keywords or browse the web directly</div>"
            "<a class='btn btn-accent' href='https://duckduckgo.com/?q=%2' style='margin-top:16px'>Search DuckDuckGo</a>"
            "</div>")
            .arg(query.toHtmlEscaped(), QUrl::toPercentEncoding(query).constData());
    }

    return QStringLiteral(R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Nyx: %1</title>
<style>
%2

.search-header { text-align: center; padding: 32px 0 24px; }
.search-logo { font-size: 1.4em; font-weight: 800; letter-spacing: -1px;
  background: linear-gradient(135deg, %3, %4);
  -webkit-background-clip: text; -webkit-text-fill-color: transparent;
  margin-bottom: 16px; }

.search-again { max-width: 560px; margin: 0 auto 8px; position: relative; }
.search-again input { width: 100%%; padding: 12px 18px; font-size: .95em;
  background: %5; border: 1px solid %6; border-radius: 10px;
  color: %7; outline: none; transition: border-color .2s, box-shadow .2s; }
.search-again input:focus { border-color: %3; box-shadow: 0 0 0 3px rgba(110,106,179,0.12); }
.result-count { text-align: center; color: %8; font-size: .82em; margin-bottom: 28px; }

.results-container { max-width: 680px; margin: 0 auto; }

.result { background: %5; border: 1px solid %6; border-radius: 12px;
  padding: 16px 20px; margin-bottom: 10px; transition: all .25s ease; }
.result:hover { border-color: %3; transform: translateX(4px);
  box-shadow: 0 4px 20px rgba(110,106,179,0.1); }
.r-meta { display: flex; align-items: center; gap: 8px; margin-bottom: 6px; }
.r-domain { font-size: .75em; color: %8; font-weight: 500; }
.r-title { color: %4; font-size: 1.05em; font-weight: 600; display: block;
  margin-bottom: 4px; transition: color .2s; text-decoration: none; }
.r-title:hover { color: %9; }
.r-url { color: %8; font-size: .75em; margin-bottom: 6px; word-break: break-all;
  white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.r-desc { color: %10; line-height: 1.5; font-size: .88em; }

.badge { padding: 2px 10px; border-radius: 8px; font-size: .68em;
  font-weight: 700; letter-spacing: 0.04em; text-transform: uppercase; }
.badge.nyx { background: rgba(110,106,179,0.15); color: %4; }
.badge.database { background: rgba(110,106,179,0.15); color: %4; }
.badge.web { background: rgba(80,80,100,0.3); color: %10; }

.no-results { text-align: center; padding: 60px 20px; }
.no-results-icon { font-size: 3em; margin-bottom: 16px; opacity: 0.5; }
.no-results-text { font-size: 1.1em; color: %7; margin-bottom: 8px; }
.no-results-hint { color: %8; font-size: .88em; }

.web-loading { text-align: center; padding: 20px; color: %8; font-size: .85em;
  animation: fadeIn .5s ease; }
</style></head><body>
<div class="container">
  <div class="search-header">
    <div class="search-logo">Nyx Search</div>
    <div class="search-again">
      <input type="text" value="%1" autofocus
        onkeydown="if(event.key==='Enter'){var v=this.value.trim();if(v)window.location='oyn://search?q='+encodeURIComponent(v)}">
    </div>
  </div>
  <div class="result-count">%11 results found</div>
  <div class="results-container">
    %12
    %13
    %14
  </div>
</div>
</body></html>)HTML")
        .arg(query.toHtmlEscaped())                // %1
        .arg(sharedCss(c))                         // %2
        .arg(c["purple-mid"])                      // %3
        .arg(c["purple-light"])                    // %4
        .arg(c["bg-mid"])                          // %5
        .arg(c["border"])                          // %6
        .arg(c["text-primary"])                    // %7
        .arg(c["text-muted"])                      // %8
        .arg(c["purple-glow"])                     // %9
        .arg(c["text-secondary"])                  // %10
        .arg(QString::number(total))               // %11
        .arg(localResults.isEmpty() ? QString() :
             QStringLiteral("<div class='section-title'>Local &amp; Database</div>") +
             buildCards(localResults, QStringLiteral("nyx")))    // %12
        .arg(webResults.isEmpty() ?
             (total == 0 ? noResults :
              QStringLiteral("<div class='web-loading'>Fetching web results...</div>")) :
             QStringLiteral("<div class='section-title'>Web Results</div>") +
             buildCards(webResults, QStringLiteral("web")))      // %13
        .arg(QString());                           // %14
}

// ── Bookmarks Page ─────────────────────────────────────────────────
QString bookmarksPage(const QJsonArray &bookmarks, const QMap<QString, QString> &c) {
    QString items;
    int i = 0;
    for (const auto &val : bookmarks) {
        auto b = val.toObject();
        QString delay = QString::number(0.06 + i * 0.03, 'f', 2);
        QString url = b[QStringLiteral("url")].toString().toHtmlEscaped();
        QString title = b[QStringLiteral("title")].toString().toHtmlEscaped();
        QString domain = QUrl(b[QStringLiteral("url")].toString()).host().toHtmlEscaped();
        QString folder = b[QStringLiteral("folder")].toString().toHtmlEscaped();
        if (title.isEmpty()) title = url;

        items += QStringLiteral(
            "<div class='bm-card' style='animation: slideUp .35s %1s ease both;'>"
            "<a class='bm-title' href='%2'>%3</a>"
            "<div class='bm-domain'>%4</div>"
            "<div class='bm-footer'>"
            "<span class='bm-folder'>%5</span>"
            "</div></div>")
            .arg(delay, url, title, domain,
                 folder.isEmpty() ? QStringLiteral("Unsorted") : folder);
        i++;
    }

    QString emptyState;
    if (bookmarks.isEmpty()) {
        emptyState = QStringLiteral(
            "<div class='empty-state'>"
            "<div class='empty-icon'>&#9733;</div>"
            "<div class='empty-text'>No bookmarks yet</div>"
            "<div class='empty-hint'>Press <span class='kbd'>Ctrl+D</span> to bookmark a page</div>"
            "</div>");
    }

    return QStringLiteral(R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Bookmarks</title>
<style>%1

.page-title { font-size: 1.6em; font-weight: 700; color: %2;
  margin-bottom: 4px; }
.page-subtitle { color: %3; font-size: .85em; margin-bottom: 8px; }

.search-filter { max-width: 400px; margin: 0 auto 28px; }
.search-filter input { width: 100%%; padding: 10px 16px; font-size: .9em;
  background: %4; border: 1px solid %5; border-radius: 10px;
  color: %6; outline: none; transition: border-color .2s; }
.search-filter input:focus { border-color: %7; }

.bm-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(280px, 1fr));
  gap: 12px; }
.bm-card { background: %4; border: 1px solid %5; border-radius: 12px;
  padding: 16px 18px; transition: all .2s ease; }
.bm-card:hover { border-color: %7; transform: translateY(-2px);
  box-shadow: 0 4px 16px rgba(110,106,179,0.1); }
.bm-title { font-size: .95em; font-weight: 600; color: %2;
  text-decoration: none; display: block; margin-bottom: 6px;
  white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }
.bm-title:hover { color: %8; }
.bm-domain { font-size: .75em; color: %3; margin-bottom: 10px; }
.bm-footer { display: flex; justify-content: space-between; align-items: center; }
.bm-folder { font-size: .7em; padding: 2px 8px; border-radius: 6px;
  background: rgba(110,106,179,0.1); color: %7; }

.empty-state { text-align: center; padding: 60px 20px; }
.empty-icon { font-size: 3em; margin-bottom: 16px; opacity: 0.4; }
.empty-text { font-size: 1.1em; color: %6; margin-bottom: 8px; }
.empty-hint { color: %3; font-size: .85em; }
.kbd { display: inline-block; padding: 1px 6px; border-radius: 4px;
  background: rgba(255,255,255,0.06); border: 1px solid rgba(255,255,255,0.1);
  font-family: monospace; font-size: .85em; }
</style></head><body>
<div class="container">
<div class="header">
  <div class="page-title">&#9733; Bookmarks</div>
  <div class="page-subtitle">%9 saved bookmarks</div>
</div>
<div class="search-filter">
  <input type="text" placeholder="Filter bookmarks..." id="bmFilter"
    oninput="var q=this.value.toLowerCase();document.querySelectorAll('.bm-card').forEach(function(c){c.style.display=c.textContent.toLowerCase().includes(q)?'':'none'})">
</div>
%10
</div></body></html>)HTML")
        .arg(sharedCss(c))                        // %1
        .arg(c["purple-light"])                    // %2
        .arg(c["text-muted"])                      // %3
        .arg(c["bg-mid"])                          // %4
        .arg(c["border"])                          // %5
        .arg(c["text-primary"])                    // %6
        .arg(c["purple-mid"])                      // %7
        .arg(c["purple-glow"])                     // %8
        .arg(QString::number(bookmarks.size()))    // %9
        .arg(bookmarks.isEmpty() ? emptyState :
             QStringLiteral("<div class='bm-grid'>%1</div>").arg(items));  // %10
}

// ── History Page ───────────────────────────────────────────────────
QString historyPage(const QJsonArray &history, const QMap<QString, QString> &c) {
    QString items;
    int i = 0;
    for (const auto &val : history) {
        auto h = val.toObject();
        if (i >= 200) break;
        QString delay = QString::number(0.04 + i * 0.015, 'f', 3);
        QString url = h[QStringLiteral("url")].toString().toHtmlEscaped();
        QString title = h[QStringLiteral("title")].toString().toHtmlEscaped();
        QString timeStr = h[QStringLiteral("time")].toString();
        QString domain = QUrl(h[QStringLiteral("url")].toString()).host().toHtmlEscaped();
        if (title.isEmpty()) title = url;

        items += QStringLiteral(
            "<div class='hist-item' style='animation: slideUp .3s %1s ease both;'>"
            "<div class='hist-main'>"
            "<a class='hist-title' href='%2'>%3</a>"
            "<span class='hist-time'>%4</span>"
            "</div>"
            "<div class='hist-url'>%5 &mdash; %2</div>"
            "</div>")
            .arg(delay, url, title, timeStr, domain);
        i++;
    }

    QString emptyState;
    if (history.isEmpty()) {
        emptyState = QStringLiteral(
            "<div style='text-align:center;padding:60px 20px;'>"
            "<div style='font-size:3em;margin-bottom:16px;opacity:0.4;'>&#128337;</div>"
            "<div style='font-size:1.1em;color:%1;margin-bottom:8px;'>No browsing history</div>"
            "<div style='color:%2;font-size:.85em;'>Pages you visit will appear here</div>"
            "</div>").arg(c["text-primary"], c["text-muted"]);
    }

    return QStringLiteral(R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>History</title>
<style>%1

.page-title { font-size: 1.6em; font-weight: 700; color: %2;
  margin-bottom: 4px; }
.page-subtitle { color: %3; font-size: .85em; margin-bottom: 8px; }

.search-filter { max-width: 400px; margin: 0 auto 28px; }
.search-filter input { width: 100%%; padding: 10px 16px; font-size: .9em;
  background: %4; border: 1px solid %5; border-radius: 10px;
  color: %6; outline: none; transition: border-color .2s; }
.search-filter input:focus { border-color: %7; }

.hist-item { padding: 12px 16px; border-bottom: 1px solid %5;
  transition: background .15s; }
.hist-item:hover { background: %4; }
.hist-main { display: flex; justify-content: space-between; align-items: baseline;
  margin-bottom: 4px; }
.hist-title { font-size: .92em; font-weight: 600; color: %2;
  text-decoration: none; flex: 1; white-space: nowrap;
  overflow: hidden; text-overflow: ellipsis; margin-right: 12px; }
.hist-title:hover { color: %8; }
.hist-time { font-size: .75em; color: %3; white-space: nowrap; }
.hist-url { font-size: .72em; color: %3;
  white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }

.clear-btn { display: inline-block; padding: 8px 20px; margin-top: 24px;
  border-radius: 8px; border: 1px solid %5; color: %3; font-size: .82em;
  cursor: pointer; background: transparent; transition: all .2s; text-decoration: none; }
.clear-btn:hover { border-color: #e05050; color: #e05050; }
</style></head><body>
<div class="container">
<div class="header">
  <div class="page-title">&#128337; History</div>
  <div class="page-subtitle">%9 entries</div>
</div>
<div class="search-filter">
  <input type="text" placeholder="Search history..." id="histFilter"
    oninput="var q=this.value.toLowerCase();document.querySelectorAll('.hist-item').forEach(function(c){c.style.display=c.textContent.toLowerCase().includes(q)?'':'none'})">
</div>
%10
</div></body></html>)HTML")
        .arg(sharedCss(c))                        // %1
        .arg(c["purple-light"])                    // %2
        .arg(c["text-muted"])                      // %3
        .arg(c["bg-mid"])                          // %4
        .arg(c["border"])                          // %5
        .arg(c["text-primary"])                    // %6
        .arg(c["purple-mid"])                      // %7
        .arg(c["purple-glow"])                     // %8
        .arg(QString::number(history.size()))       // %9
        .arg(history.isEmpty() ? emptyState : items);  // %10
}

// ── Settings Page ──────────────────────────────────────────────────
QString settingsPage(const QJsonObject &config, const QMap<QString, QString> &c) {
    auto toggle = [&](const QString &key, const QString &label, const QString &desc) -> QString {
        bool val = config[key].toBool();
        return QStringLiteral(
            "<div class='setting-row'>"
            "<div class='setting-info'>"
            "<div class='setting-label'>%1</div>"
            "<div class='setting-desc'>%2</div>"
            "</div>"
            "<label class='toggle'>"
            "<input type='checkbox' %3 onchange=\"window.location='oyn://setting?%4='+(this.checked?'true':'false')\">"
            "<span class='toggle-slider'></span>"
            "</label></div>")
            .arg(label, desc, val ? QStringLiteral("checked") : QString(), key);
    };

    return QStringLiteral(R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Settings</title>
<style>%1

.page-title { font-size: 1.6em; font-weight: 700; color: %2;
  margin-bottom: 4px; }
.page-subtitle { color: %3; font-size: .85em; }

.setting-section { background: %4; border: 1px solid %5; border-radius: 12px;
  padding: 20px 24px; margin-bottom: 16px; }
.setting-section h3 { color: %2; font-size: 1em; font-weight: 600;
  margin-bottom: 16px; padding-bottom: 10px; border-bottom: 1px solid %5; }

.setting-row { display: flex; justify-content: space-between; align-items: center;
  padding: 10px 0; border-bottom: 1px solid rgba(255,255,255,0.03); }
.setting-row:last-child { border-bottom: none; }
.setting-info { flex: 1; margin-right: 16px; }
.setting-label { font-size: .9em; color: %6; font-weight: 500; }
.setting-desc { font-size: .75em; color: %3; margin-top: 2px; }

/* Toggle switch */
.toggle { position: relative; display: inline-block; width: 44px; height: 24px; }
.toggle input { opacity: 0; width: 0; height: 0; }
.toggle-slider { position: absolute; cursor: pointer; inset: 0;
  background: %5; border-radius: 24px; transition: .3s; }
.toggle-slider:before { content: ""; position: absolute; height: 18px; width: 18px;
  left: 3px; bottom: 3px; background: %3; border-radius: 50%%; transition: .3s; }
.toggle input:checked + .toggle-slider { background: %7; }
.toggle input:checked + .toggle-slider:before { transform: translateX(20px);
  background: white; }

.about-card { text-align: center; background: %4; border: 1px solid %5;
  border-radius: 12px; padding: 32px 24px; margin-top: 24px; }
.about-version { font-size: 1.1em; font-weight: 700; color: %6; margin-bottom: 6px; }
.about-tech { font-size: .8em; color: %3; }
.about-shortcuts { margin-top: 20px; font-size: .78em; color: %3; }
.kbd { display: inline-block; padding: 1px 6px; border-radius: 4px;
  background: rgba(255,255,255,0.06); border: 1px solid rgba(255,255,255,0.1);
  font-family: monospace; font-size: .9em; }
</style></head><body>
<div class="container">
<div class="header">
  <div class="page-title">&#9881; Settings</div>
  <div class="page-subtitle">Configure your browser</div>
</div>

<div class="setting-section">
<h3>&#128269; Search &amp; Indexing</h3>
%8
%9
</div>

<div class="setting-section">
<h3>&#128274; Privacy &amp; Security</h3>
%10
</div>

<div class="setting-section">
<h3>&#127912; Appearance</h3>
%11
%12
</div>

<div class="about-card">
  <div class="about-version">OyNIx Browser v3.1</div>
  <div class="about-tech">Qt6 WebEngine &middot; C++17 &middot; .NET 8 NativeAOT Core</div>
  <div class="about-shortcuts">
    <span class="kbd">Ctrl+T</span> New Tab &middot;
    <span class="kbd">Ctrl+W</span> Close Tab &middot;
    <span class="kbd">Ctrl+L</span> Focus URL &middot;
    <span class="kbd">Ctrl+K</span> Command Palette &middot;
    <span class="kbd">F11</span> Fullscreen
  </div>
</div>
</div></body></html>)HTML")
        .arg(sharedCss(c))                         // %1
        .arg(c["purple-light"])                    // %2
        .arg(c["text-muted"])                      // %3
        .arg(c["bg-mid"])                          // %4
        .arg(c["border"])                          // %5
        .arg(c["text-primary"])                    // %6
        .arg(c["purple-mid"])                      // %7
        .arg(toggle(QStringLiteral("auto_index_pages"),
                    QStringLiteral("Auto-index pages"),
                    QStringLiteral("Index visited pages for Nyx local search")))
        .arg(toggle(QStringLiteral("auto_expand_database"),
                    QStringLiteral("Expand site database"),
                    QStringLiteral("Automatically add discovered sites to the database")))
        .arg(toggle(QStringLiteral("show_security_prompts"),
                    QStringLiteral("Security prompts"),
                    QStringLiteral("Show warnings when visiting login pages on untrusted sites")))
        .arg(toggle(QStringLiteral("force_nyx_theme_external"),
                    QStringLiteral("Theme external pages"),
                    QStringLiteral("Apply Nyx dark theme to external search engine results")))
        .arg(toggle(QStringLiteral("restore_session"),
                    QStringLiteral("Restore session"),
                    QStringLiteral("Reopen previous tabs when starting the browser")));
}

// ── Downloads Page ─────────────────────────────────────────────────
QString downloadsPage(const QJsonArray &downloads, const QMap<QString, QString> &c) {
    QString items;
    if (downloads.isEmpty()) {
        items = QStringLiteral(
            "<div class='empty-state'>"
            "<div class='empty-icon'>&#8615;</div>"
            "<div class='empty-text'>No downloads yet</div>"
            "<div class='empty-hint'>Files you download will appear here</div>"
            "</div>");
    } else {
        int i = 0;
        for (const auto &val : downloads) {
            auto d = val.toObject();
            QString delay = QString::number(0.06 + i * 0.03, 'f', 2);
            QString filename = d[QStringLiteral("filename")].toString().toHtmlEscaped();
            QString size = d[QStringLiteral("size")].toString();
            QString status = d[QStringLiteral("status")].toString();

            items += QStringLiteral(
                "<div class='dl-item' style='animation: slideUp .3s %1s ease both;'>"
                "<div class='dl-icon'>&#128196;</div>"
                "<div class='dl-info'>"
                "<div class='dl-name'>%2</div>"
                "<div class='dl-meta'>%3 &middot; %4</div>"
                "</div></div>")
                .arg(delay, filename, size, status);
            i++;
        }
    }

    return QStringLiteral(R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Downloads</title>
<style>%1

.page-title { font-size: 1.6em; font-weight: 700; color: %2;
  margin-bottom: 4px; }
.page-subtitle { color: %3; font-size: .85em; }

.dl-item { display: flex; align-items: center; gap: 14px;
  padding: 14px 16px; background: %4; border: 1px solid %5;
  border-radius: 10px; margin-bottom: 8px; transition: all .2s; }
.dl-item:hover { border-color: %6; }
.dl-icon { font-size: 1.6em; opacity: 0.6; }
.dl-name { font-size: .9em; font-weight: 600; color: %7; margin-bottom: 2px; }
.dl-meta { font-size: .75em; color: %3; }

.empty-state { text-align: center; padding: 60px 20px; }
.empty-icon { font-size: 3em; margin-bottom: 16px; opacity: 0.4; }
.empty-text { font-size: 1.1em; color: %7; margin-bottom: 8px; }
.empty-hint { color: %3; font-size: .85em; }
</style></head><body>
<div class="container">
<div class="header">
  <div class="page-title">&#8615; Downloads</div>
  <div class="page-subtitle">%8 files</div>
</div>
%9
</div></body></html>)HTML")
        .arg(sharedCss(c))                         // %1
        .arg(c["purple-light"])                    // %2
        .arg(c["text-muted"])                      // %3
        .arg(c["bg-mid"])                          // %4
        .arg(c["border"])                          // %5
        .arg(c["purple-mid"])                      // %6
        .arg(c["text-primary"])                    // %7
        .arg(QString::number(downloads.size()))    // %8
        .arg(items);                               // %9
}

// ── Profiles Page ──────────────────────────────────────────────────
QString profilesPage(const QJsonArray &profiles, const QString &activeProfile,
                     const QMap<QString, QString> &c) {
    QString cards;
    int i = 0;
    for (const auto &val : profiles) {
        auto p = val.toObject();
        QString name = p[QStringLiteral("name")].toString();
        bool active = (name == activeProfile);
        QString delay = QString::number(0.08 + i * 0.06, 'f', 2);
        cards += QStringLiteral(
            "<div class='profile-card%1' style='animation: slideUp .35s %2s ease both;'>"
            "<div class='profile-avatar'>&#128100;</div>"
            "<div class='profile-name'>%3</div>"
            "%4"
            "<div class='profile-action'>"
            "<a class='btn%5' href='oyn://switch-profile?name=%3'>%6</a>"
            "</div></div>")
            .arg(active ? QStringLiteral(" active") : QString(),
                 delay, name.toHtmlEscaped(),
                 active ? QStringLiteral("<div class='profile-status'>Active</div>") : QString(),
                 active ? QString() : QStringLiteral(" btn-accent"),
                 active ? QStringLiteral("Current") : QStringLiteral("Switch"));
        i++;
    }

    return QStringLiteral(R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Profiles</title>
<style>%1

.page-title { font-size: 1.6em; font-weight: 700; color: %2;
  margin-bottom: 4px; }
.page-subtitle { color: %3; font-size: .85em; }

.profile-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));
  gap: 16px; margin-bottom: 28px; }
.profile-card { background: %4; border: 1px solid %5; border-radius: 14px;
  padding: 28px 20px; text-align: center; transition: all .2s ease; }
.profile-card:hover { border-color: %6; transform: translateY(-2px);
  box-shadow: 0 4px 16px rgba(110,106,179,0.1); }
.profile-card.active { border-color: %6; }
.profile-avatar { font-size: 2.4em; margin-bottom: 10px; }
.profile-name { font-weight: 700; font-size: 1.05em; color: %7; margin-bottom: 6px; }
.profile-status { font-size: .75em; color: %6; font-weight: 600;
  margin-bottom: 10px; }
.profile-action { margin-top: 12px; }

.new-profile { text-align: center; margin-top: 8px; }
</style></head><body>
<div class="container">
<div class="header">
  <div class="page-title">&#128100; Profiles</div>
  <div class="page-subtitle">Manage browser profiles</div>
</div>
<div class="profile-grid">%8</div>
<div class="new-profile">
  <a class="btn btn-accent" href="oyn://new-profile">+ New Profile</a>
</div>
</div></body></html>)HTML")
        .arg(sharedCss(c))                         // %1
        .arg(c["purple-light"])                    // %2
        .arg(c["text-muted"])                      // %3
        .arg(c["bg-mid"])                          // %4
        .arg(c["border"])                          // %5
        .arg(c["purple-mid"])                      // %6
        .arg(c["text-primary"])                    // %7
        .arg(cards);                               // %8
}

}  // namespace InternalPages
