#include "InternalPages.h"
#include <QJsonDocument>
#include <QUrl>

namespace InternalPages {

// ── Shared CSS — clean, professional, no glassmorphism ─────────────
QString sharedCss(const QMap<QString, QString> &c) {
    return QStringLiteral(R"CSS(
* { margin: 0; padding: 0; box-sizing: border-box; }
body { background: %1; color: %2; font-family: -apple-system, 'Segoe UI', 'Ubuntu', sans-serif;
  min-height: 100vh; overflow-x: hidden; line-height: 1.5; }
a { color: %3; text-decoration: none; transition: color .15s; }
a:hover { color: %4; }

.wrap { display: flex; flex-direction: column; align-items: center;
  justify-content: center; min-height: 100vh; padding: 40px 24px; }

.container { max-width: 860px; margin: 0 auto; padding: 32px 24px; }

.card { background: %5; border: 1px solid %6; border-radius: 8px;
  padding: 16px; transition: border-color .15s, background .15s; }
.card:hover { border-color: %7; background: %8; }

.header { text-align: center; margin-bottom: 40px; }

.section-title { font-size: 1.1em; font-weight: 600; color: %3;
  margin: 28px 0 12px; padding-bottom: 8px; border-bottom: 1px solid %6; }

.btn { display: inline-block; padding: 8px 18px; border-radius: 6px;
  border: 1px solid %6; color: %2; cursor: pointer;
  font-size: 0.85em; transition: all .15s; text-decoration: none; }
.btn:hover { border-color: %7; background: %5; }
.btn-accent { background: %7; color: %1; border-color: %7; font-weight: 600; }
.btn-accent:hover { background: %3; }

.badge { display: inline-block; padding: 2px 8px; border-radius: 4px;
  font-size: .75em; font-weight: 600; }
)CSS")
        .arg(c["bg-darkest"], c["text-primary"], c["purple-light"],
             c["purple-glow"], c["bg-mid"], c["border"],
             c["purple-mid"], c["bg-light"]);
}

// ── Homepage — clean, minimal ──────────────────────────────────────
QString homePage(const QMap<QString, QString> &c) {
    return QStringLiteral(R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>OyNIx</title>
<style>
%1

.logo { font-size: 2.8em; font-weight: 700; color: %2; letter-spacing: -1px;
  margin-bottom: 4px; }
.tagline { color: %3; font-size: .85em; letter-spacing: 2px; text-transform: uppercase;
  margin-bottom: 40px; }

.search-wrap { width: 520px; max-width: 90vw; margin-bottom: 48px; }
.search-box { width: 100%%; padding: 12px 20px; font-size: 0.95em;
  background: %4; border: 1px solid %5; border-radius: 8px;
  color: %6; outline: none; transition: border-color .15s; }
.search-box:focus { border-color: %7; }
.search-box::placeholder { color: %3; }

.shortcuts { display: grid; grid-template-columns: repeat(3, 1fr); gap: 10px;
  max-width: 520px; width: 100%%; }
.shortcut { display: flex; align-items: center; gap: 10px;
  padding: 12px 16px; border-radius: 8px; text-decoration: none;
  color: %6; background: %4; border: 1px solid %5;
  transition: border-color .15s, background .15s; }
.shortcut:hover { border-color: %7; background: %8; }
.shortcut-icon { font-size: 1.2em; width: 28px; text-align: center; }
.shortcut-label { font-size: .85em; font-weight: 500; }

.version { position: fixed; bottom: 10px; right: 14px; color: %3; font-size: .7em; }
</style></head><body>
<div class="wrap">
  <div class="logo">OyNIx</div>
  <div class="tagline">Browse. Search. Build.</div>
  <div class="search-wrap">
    <input class="search-box" id="searchInput" placeholder="Search or enter URL"
      autofocus onkeydown="if(event.key==='Enter'){var v=this.value.trim();if(v)window.location='oyn://search?q='+encodeURIComponent(v)}">
  </div>
  <div class="shortcuts">
    <a class="shortcut" href="oyn://bookmarks"><span class="shortcut-icon">&#9733;</span><span class="shortcut-label">Bookmarks</span></a>
    <a class="shortcut" href="oyn://history"><span class="shortcut-icon">&#8635;</span><span class="shortcut-label">History</span></a>
    <a class="shortcut" href="oyn://downloads"><span class="shortcut-icon">&#8615;</span><span class="shortcut-label">Downloads</span></a>
    <a class="shortcut" href="oyn://settings"><span class="shortcut-icon">&#9881;</span><span class="shortcut-label">Settings</span></a>
    <a class="shortcut" href="oyn://profiles"><span class="shortcut-icon">&#9673;</span><span class="shortcut-label">Profiles</span></a>
    <a class="shortcut" href="oyn://extensions"><span class="shortcut-icon">&#9883;</span><span class="shortcut-label">Extensions</span></a>
  </div>
</div>
<div class="version">OyNIx v3.1</div>
</body></html>)HTML")
        .arg(sharedCss(c))
        .arg(c["text-primary"])    // %2 logo
        .arg(c["text-muted"])      // %3 tagline/placeholder
        .arg(c["bg-mid"])          // %4 input/card bg
        .arg(c["border"])          // %5 border
        .arg(c["text-primary"])    // %6 text
        .arg(c["purple-mid"])      // %7 focus/hover accent
        .arg(c["bg-light"]);       // %8 hover bg
}

// ── Search Results ─────────────────────────────────────────────────
QString searchPage(const QString &query,
                   const QJsonArray &localResults,
                   const QJsonArray &webResults,
                   const QMap<QString, QString> &c) {
    auto buildCards = [&](const QJsonArray &results, const QString &badge) -> QString {
        QString html;
        int i = 0;
        for (const auto &val : results) {
            auto r = val.toObject();
            QString delay = QString::number(0.1 + i * 0.05, 'f', 2);
            html += QStringLiteral(
                "<div class='result' style='animation: slideUp .4s %1s ease forwards;'>"
                "<div class='r-top'><a class='r-title' href='%2'>%3</a>"
                "<span class='badge %4'>%4</span></div>"
                "<div class='r-url'>%2</div>"
                "<div class='r-desc'>%5</div></div>")
                .arg(delay, r["url"].toString().toHtmlEscaped(),
                     r["title"].toString().toHtmlEscaped(),
                     badge, r["description"].toString().toHtmlEscaped());
            i++;
        }
        return html;
    };

    int total = localResults.size() + webResults.size();
    return QStringLiteral(R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Nyx Search: %1</title>
<style>
%2
.result { background: rgba(22,22,31,0.5); border: 1px solid rgba(58,58,74,0.3);
  border-radius: 14px; padding: 16px 20px; margin-bottom: 10px;
  opacity: 0; transition: all .4s cubic-bezier(.4,0,.2,1); }
.result:hover { border-color: %3; transform: translateX(6px);
  box-shadow: 0 8px 28px rgba(123,79,191,.15); }
.r-top { display: flex; align-items: center; gap: 8px; margin-bottom: 5px; }
.r-title { color: %4; font-size: 1.1em; font-weight: 600; transition: color .3s; }
.r-title:hover { color: %5; text-shadow: 0 0 12px rgba(123,79,191,.3); text-decoration: none; }
.badge { padding: 2px 10px; border-radius: 10px; font-size: .7em; font-weight: bold; }
.badge.nyx { background: rgba(123,79,191,.2); color: %6; }
.badge.web { background: rgba(40,40,54,.8); color: %7; }
.r-url { color: %8; font-size: .8em; margin-bottom: 5px; word-break: break-all; }
.r-desc { color: %7; line-height: 1.5; font-size: .92em; }
</style></head><body>
<div class="glass-orb" style="width:350px;height:350px;background:%3;top:5%%;right:-3%%"></div>
<div class="container">
<div class="header"><h1 style="background:linear-gradient(135deg,%3,%5);-webkit-background-clip:text;-webkit-text-fill-color:transparent;font-size:1.8em">Nyx Search Results</h1>
<div style="color:%8;margin-top:6px;font-size:.9em">"%1" &mdash; %9 results</div></div>
%10
%11
</div>
<script>%12</script>
</body></html>)HTML")
        .arg(query.toHtmlEscaped())       // %1
        .arg(sharedCss(c))                // %2
        .arg(c["purple-mid"])             // %3
        .arg(c["purple-light"])           // %4
        .arg(c["purple-glow"])            // %5
        .arg(c["purple-pale"])            // %6
        .arg(c["text-secondary"])         // %7
        .arg(c["text-muted"])             // %8
        .arg(QString::number(total))      // %9
        .arg(localResults.isEmpty() ? "" :
             "<div class='section-title'>From Your Database</div>" + buildCards(localResults, "nyx"))  // %10
        .arg(webResults.isEmpty() ? "" :
             "<div class='section-title'>Web Results</div>" + buildCards(webResults, "web"))  // %11
        .arg(QString());                   // %12 (no JS)
}

// ── Bookmarks Page ─────────────────────────────────────────────────
QString bookmarksPage(const QJsonArray &bookmarks, const QMap<QString, QString> &c) {
    QString items;
    int i = 0;
    for (const auto &val : bookmarks) {
        auto b = val.toObject();
        QString delay = QString::number(0.1 + i * 0.04, 'f', 2);
        items += QStringLiteral(
            "<div class='glass-card' style='opacity:0;animation:slideUp .4s %1s ease forwards'>"
            "<a href='%2' style='color:%3;font-weight:600;font-size:1.05em'>%4</a>"
            "<div style='color:%5;font-size:.8em;margin-top:4px'>%2</div>"
            "<div style='color:%6;font-size:.75em;margin-top:4px'>%7</div>"
            "</div>")
            .arg(delay, b["url"].toString().toHtmlEscaped(),
                 c["purple-light"], b["title"].toString().toHtmlEscaped(),
                 c["text-muted"], c["text-secondary"],
                 b["added"].toString());
        i++;
    }

    return QStringLiteral(R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Bookmarks</title>
<style>%1
.grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(300px, 1fr));
  gap: 12px; }
</style></head><body>
<div class="container">
<div class="header"><h1 style="color:%2;font-size:1.8em">&#9733; Bookmarks</h1>
<div style="color:%3;margin-top:6px">%4 bookmarks</div></div>
<div class="grid">%5</div>
</div></body></html>)HTML")
        .arg(sharedCss(c), c["purple-light"], c["text-muted"],
             QString::number(bookmarks.size()), items);
}

// ── History Page ───────────────────────────────────────────────────
QString historyPage(const QJsonArray &history, const QMap<QString, QString> &c) {
    QString items;
    int i = 0;
    for (const auto &val : history) {
        auto h = val.toObject();
        if (i >= 200) break;
        QString delay = QString::number(0.05 + i * 0.02, 'f', 2);
        items += QStringLiteral(
            "<div class='glass-card' style='opacity:0;animation:slideUp .3s %1s ease forwards;"
            "margin-bottom:8px;padding:12px 16px'>"
            "<a href='%2' style='color:%3;font-weight:600'>%4</a>"
            "<span style='float:right;color:%5;font-size:.8em'>%6</span>"
            "<div style='color:%7;font-size:.8em;margin-top:2px'>%2</div>"
            "</div>")
            .arg(delay, h["url"].toString().toHtmlEscaped(),
                 c["purple-light"], h["title"].toString().toHtmlEscaped(),
                 c["text-muted"], h["time"].toString(),
                 c["text-secondary"]);
        i++;
    }

    return QStringLiteral(R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>History</title>
<style>%1</style></head><body>
<div class="container">
<div class="header"><h1 style="color:%2;font-size:1.8em">&#128337; History</h1>
<div style="color:%3;margin-top:6px">%4 entries</div></div>
%5
</div></body></html>)HTML")
        .arg(sharedCss(c), c["purple-light"], c["text-muted"],
             QString::number(history.size()), items);
}

// ── Settings Page ──────────────────────────────────────────────────
QString settingsPage(const QJsonObject &config, const QMap<QString, QString> &c) {
    auto boolCheck = [&](const QString &key, const QString &label) -> QString {
        bool val = config[key].toBool();
        return QStringLiteral(
            "<div style='display:flex;align-items:center;gap:10px;padding:8px 0'>"
            "<input type='checkbox' %1 onchange=\"window.location='oyn://setting?%2='+(this.checked?'true':'false')\">"
            "<label style='color:%3'>%4</label></div>")
            .arg(val ? "checked" : "", key, c["text-primary"], label);
    };

    return QStringLiteral(R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Settings</title>
<style>%1
.setting-group { margin-bottom: 24px; }
.setting-group h3 { color: %2; margin-bottom: 12px; }
input[type=checkbox] { width: 20px; height: 20px; accent-color: %3; }
</style></head><body>
<div class="container">
<div class="header"><h1 style="color:%2;font-size:1.8em">&#9881; Settings</h1></div>
<div class="glass-card setting-group">
<h3>Search</h3>
%4
%5
</div>
<div class="glass-card setting-group">
<h3>Privacy</h3>
%6
</div>
<div class="glass-card setting-group">
<h3>Appearance</h3>
%7
</div>
<div class="glass-card setting-group" style="text-align:center;margin-top:32px">
<div style="color:%8;font-size:.9em">OyNIx Browser v3.0</div>
<div style="color:%9;font-size:.8em;margin-top:4px">Chromium WebEngine &bull; C++ Core</div>
</div>
</div></body></html>)HTML")
        .arg(sharedCss(c), c["purple-light"], c["purple-mid"])
        .arg(boolCheck("auto_index_pages", "Auto-index visited pages for Nyx search"))
        .arg(boolCheck("auto_expand_database", "Auto-expand site database from browsing"))
        .arg(boolCheck("show_security_prompts", "Show security prompts for untrusted sites"))
        .arg(boolCheck("force_nyx_theme_external", "Force Nyx theme on external search engines"))
        .arg(c["text-muted"], c["text-secondary"]);
}

// ── Downloads Page ─────────────────────────────────────────────────
QString downloadsPage(const QJsonArray &downloads, const QMap<QString, QString> &c) {
    QString items;
    if (downloads.isEmpty()) {
        items = QStringLiteral("<div class='glass-card' style='text-align:center;padding:40px;"
            "color:%1'>No downloads yet</div>").arg(c["text-muted"]);
    } else {
        int i = 0;
        for (const auto &val : downloads) {
            auto d = val.toObject();
            QString delay = QString::number(0.1 + i * 0.04, 'f', 2);
            items += QStringLiteral(
                "<div class='glass-card' style='opacity:0;animation:slideUp .4s %1s ease forwards;"
                "margin-bottom:8px'>"
                "<div style='font-weight:600;color:%2'>%3</div>"
                "<div style='color:%4;font-size:.85em'>%5 &mdash; %6</div></div>")
                .arg(delay, c["text-primary"],
                     d["filename"].toString().toHtmlEscaped(),
                     c["text-secondary"], d["size"].toString(), d["status"].toString());
            i++;
        }
    }
    return QStringLiteral(R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Downloads</title>
<style>%1</style></head><body>
<div class="container">
<div class="header"><h1 style="color:%2;font-size:1.8em">&#11015; Downloads</h1></div>
%3
</div></body></html>)HTML")
        .arg(sharedCss(c), c["purple-light"], items);
}

// ── Profiles Page ──────────────────────────────────────────────────
QString profilesPage(const QJsonArray &profiles, const QString &activeProfile,
                     const QMap<QString, QString> &c) {
    QString cards;
    int i = 0;
    for (const auto &val : profiles) {
        auto p = val.toObject();
        QString name = p["name"].toString();
        bool active = (name == activeProfile);
        QString delay = QString::number(0.1 + i * 0.06, 'f', 2);
        cards += QStringLiteral(
            "<div class='glass-card' style='opacity:0;animation:slideUp .4s %1s ease forwards;"
            "border-color:%2;text-align:center;padding:24px'>"
            "<div style='font-size:2em;margin-bottom:8px'>&#128100;</div>"
            "<div style='font-weight:700;font-size:1.1em;color:%3'>%4</div>"
            "%5"
            "<div style='margin-top:12px'>"
            "<a class='btn%6' href='oyn://switch-profile?name=%4'>%7</a>"
            "</div></div>")
            .arg(delay, active ? c["purple-mid"] : "rgba(58,58,74,0.3)",
                 c["text-primary"], name.toHtmlEscaped(),
                 active ? QStringLiteral("<div style='color:%1;font-size:.8em;margin-top:4px'>Active</div>")
                           .arg(c["success"]) : "",
                 active ? "" : " btn-accent",
                 active ? "Current" : "Switch");
        i++;
    }

    return QStringLiteral(R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>Profiles</title>
<style>%1
.grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(200px, 1fr)); gap: 16px; }
</style></head><body>
<div class="container">
<div class="header"><h1 style="color:%2;font-size:1.8em">&#128100; Profiles</h1></div>
<div class="grid">%3</div>
<div style="text-align:center;margin-top:24px">
<a class="btn btn-accent" href="oyn://new-profile">+ New Profile</a>
</div>
</div></body></html>)HTML")
        .arg(sharedCss(c), c["purple-light"], cards);
}

}  // namespace InternalPages
