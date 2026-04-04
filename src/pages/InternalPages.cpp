#include "InternalPages.h"
#include <QJsonDocument>
#include <QUrl>

namespace InternalPages {

// ── Shared CSS (glassmorphism, animations, cards) ──────────────────
QString sharedCss(const QMap<QString, QString> &c) {
    return QStringLiteral(R"CSS(
* { margin: 0; padding: 0; box-sizing: border-box; }
body { background: %1; color: %2; font-family: 'Segoe UI', 'Ubuntu', sans-serif;
  min-height: 100vh; overflow-x: hidden; }
a { color: %3; text-decoration: none; }
a:hover { color: %4; text-decoration: underline; }

.glass-orb { position: fixed; border-radius: 50%; filter: blur(80px); opacity: 0.12;
  pointer-events: none; z-index: 0; }

.wrap { position: relative; z-index: 1; display: flex; flex-direction: column;
  align-items: center; justify-content: center; min-height: 100vh; padding: 30px 20px; }

.container { position: relative; z-index: 1; max-width: 900px; margin: 0 auto; padding: 24px; }

.glass-card { background: rgba(22,22,31,0.6); border: 1px solid rgba(58,58,74,0.3);
  border-radius: 16px; padding: 20px; backdrop-filter: blur(10px);
  transition: all .4s cubic-bezier(.4,0,.2,1); }
.glass-card:hover { border-color: %5; transform: translateY(-2px);
  box-shadow: 0 8px 28px rgba(123,79,191,0.15); }

.header { text-align: center; margin-bottom: 32px; padding: 22px;
  background: rgba(22,22,31,0.6); border-radius: 18px;
  border: 1px solid rgba(58,58,74,0.3); backdrop-filter: blur(12px);
  animation: shimmerIn .5s ease forwards; }

.section-title { font-size: 1.2em; color: %3; margin: 24px 0 12px; padding-bottom: 6px;
  border-bottom: 2px solid rgba(58,58,74,0.3); }

.btn { display: inline-block; padding: 8px 20px; border-radius: 12px;
  border: 1px solid rgba(58,58,74,0.4); color: %2; cursor: pointer;
  font-size: 0.9em; transition: all .3s; text-decoration: none; }
.btn:hover { border-color: %5; color: %3; background: rgba(123,79,191,0.1);
  text-decoration: none; }
.btn-accent { background: %5; color: %1; border-color: %5; font-weight: bold; }
.btn-accent:hover { background: %3; color: %1; }

@keyframes fadeIn { from { opacity: 0; } to { opacity: 1; } }
@keyframes slideUp { from { opacity: 0; transform: translateY(20px); }
  to { opacity: 1; transform: translateY(0); } }
@keyframes shimmerIn { from { opacity: 0; transform: scale(0.97); }
  to { opacity: 1; transform: scale(1); } }
@keyframes glowPulse { 0% { text-shadow: 0 0 20px rgba(123,79,191,0.3); }
  100% { text-shadow: 0 0 40px rgba(123,79,191,0.5); } }
)CSS")
        .arg(c["bg-darkest"], c["text-primary"], c["purple-light"],
             c["purple-glow"], c["purple-mid"]);
}

// ── Mouse-tracking refraction JS ───────────────────────────────────
QString refractionJs() {
    return QStringLiteral(R"JS(
document.addEventListener('mousemove', function(e) {
    var targets = document.querySelectorAll('.refract-target');
    for (var i = 0; i < targets.length; i++) {
        var r = targets[i].getBoundingClientRect();
        var x = ((e.clientX - r.left) / r.width) * 100;
        var y = ((e.clientY - r.top) / r.height) * 100;
        targets[i].style.setProperty('--mx', x + '%');
        targets[i].style.setProperty('--my', y + '%');
    }
});
)JS");
}

// ── Homepage ───────────────────────────────────────────────────────
QString homePage(const QMap<QString, QString> &c) {
    return QStringLiteral(R"HTML(<!DOCTYPE html>
<html><head><meta charset="utf-8"><title>OyNIx</title>
<style>
%1

.logo { font-size: 5em; font-weight: 900; letter-spacing: -2px;
  background: linear-gradient(135deg, %2, %3, %4);
  -webkit-background-clip: text; -webkit-text-fill-color: transparent;
  opacity: 0; animation: fadeIn .7s ease forwards, glowPulse 4s 1s ease-in-out infinite alternate; }

.tagline { color: %5; font-size: .9em; letter-spacing: 6px; text-transform: uppercase;
  margin-bottom: 36px; opacity: 0; animation: shimmerIn .8s .2s ease forwards; }

.search-wrap { position: relative; width: 580px; max-width: 88vw; margin-bottom: 44px;
  opacity: 0; animation: slideUp .6s .4s ease forwards; }

.search-box { width: 100%%; padding: 15px 52px 15px 20px; font-size: 1.05em;
  background: %6; border: 2px solid rgba(58,58,74,0.4); border-radius: 50px;
  color: %7; outline: none; transition: all .4s cubic-bezier(.4,0,.2,1); }
.search-box:focus { border-color: %2; box-shadow: 0 0 40px rgba(123,79,191,.3);
  background: %8; }
.search-box::placeholder { color: %5; }

.cards { display: grid; grid-template-columns: repeat(4, 1fr); gap: 12px;
  max-width: 780px; width: 100%%; }
.card { background: rgba(22,22,31,0.6); border: 1px solid rgba(58,58,74,0.3);
  border-radius: 16px; padding: 24px 12px 18px; text-align: center; cursor: pointer;
  transition: all .45s cubic-bezier(.4,0,.2,1); text-decoration: none;
  opacity: 0; animation: shimmerIn .5s ease forwards; color: %7; }
.card:nth-child(1){animation-delay:.5s} .card:nth-child(2){animation-delay:.57s}
.card:nth-child(3){animation-delay:.64s} .card:nth-child(4){animation-delay:.71s}
.card:nth-child(5){animation-delay:.78s} .card:nth-child(6){animation-delay:.85s}
.card:hover { border-color: %2; transform: translateY(-4px);
  box-shadow: 0 12px 32px rgba(123,79,191,.2); }
.card-icon { font-size: 2em; margin-bottom: 8px; }
.card-title { font-weight: 600; font-size: .95em; }

.version { position: fixed; bottom: 12px; right: 16px; color: %5;
  font-size: .75em; opacity: 0; animation: fadeIn 1s 1.5s forwards; }
</style></head><body>
<div class="glass-orb" style="width:400px;height:400px;background:%2;top:-5%%;right:-5%%"></div>
<div class="glass-orb" style="width:300px;height:300px;background:%9;bottom:5%%;left:-3%%"></div>
<div class="wrap">
  <div class="logo">OyNIx</div>
  <div class="tagline">The Nyx-Powered Browser</div>
  <div class="search-wrap refract-target">
    <input class="search-box" id="searchInput" placeholder="Search with Nyx or enter URL..."
      autofocus onkeydown="if(event.key==='Enter'){var v=this.value.trim();if(v)window.location='oyn://search?q='+encodeURIComponent(v)}">
  </div>
  <div class="cards">
    <a class="card" href="oyn://bookmarks"><div class="card-icon">&#9733;</div><div class="card-title">Bookmarks</div></a>
    <a class="card" href="oyn://history"><div class="card-icon">&#128337;</div><div class="card-title">History</div></a>
    <a class="card" href="oyn://downloads"><div class="card-icon">&#11015;</div><div class="card-title">Downloads</div></a>
    <a class="card" href="oyn://settings"><div class="card-icon">&#9881;</div><div class="card-title">Settings</div></a>
    <a class="card" href="oyn://profiles"><div class="card-icon">&#128100;</div><div class="card-title">Profiles</div></a>
    <a class="card" href="oyn://extensions"><div class="card-icon">&#128268;</div><div class="card-title">Extensions</div></a>
  </div>
</div>
<div class="version">OyNIx v3.0 &mdash; Chromium WebEngine</div>
<script>%10</script>
</body></html>)HTML")
        .arg(sharedCss(c))                // %1
        .arg(c["purple-mid"])             // %2
        .arg(c["purple-glow"])            // %3
        .arg(c["purple-soft"])            // %4
        .arg(c["text-muted"])             // %5
        .arg(c["bg-mid"])                 // %6
        .arg(c["text-primary"])           // %7
        .arg(c["bg-light"])              // %8
        .arg(c["purple-dark"])            // %9
        .arg(refractionJs());             // %10
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
        .arg(refractionJs());             // %12
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
