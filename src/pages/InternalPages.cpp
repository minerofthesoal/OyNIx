#include "InternalPages.h"
#include <QJsonDocument>
#include <QUrl>
#include <QDateTime>
#include <QTimeZone>

namespace InternalPages {

// ── Shared CSS ──────────────────────────────────────────────────────
QString sharedCss(const QMap<QString, QString> &c) {
    // Use string concatenation to exceed Qt's 9-arg limit
    QString css;
    css += QStringLiteral("* { margin: 0; padding: 0; box-sizing: border-box; }\n");

    // Body with animated dot-grid background
    css += QStringLiteral("body { background: ") + c["bg-darkest"]
        + QStringLiteral("; color: ") + c["text-primary"]
        + QStringLiteral("; font-family: 'Inter', -apple-system, 'Segoe UI', system-ui, sans-serif;"
                         " min-height: 100vh; overflow-x: hidden; line-height: 1.6; }\n");

    css += QStringLiteral(
        "body::before { content: ''; position: fixed; inset: 0; z-index: -2;"
        " background: radial-gradient(ellipse at 25% 20%, rgba(110,106,179,0.10) 0%, transparent 55%),"
        "             radial-gradient(ellipse at 75% 80%, rgba(123,79,191,0.07) 0%, transparent 55%);"
        " pointer-events: none; }\n");

    css += QStringLiteral(
        "body::after { content: ''; position: fixed; inset: 0; z-index: -1;"
        " background-image: radial-gradient(rgba(110,106,179,0.08) 1px, transparent 1px);"
        " background-size: 28px 28px;"
        " animation: dotDrift 30s linear infinite; pointer-events: none; }\n");

    // Links
    css += QStringLiteral("a { color: ") + c["purple-light"]
        + QStringLiteral("; text-decoration: none; transition: color .2s ease; }\n");
    css += QStringLiteral("a:hover { color: ") + c["purple-glow"] + QStringLiteral("; }\n");

    // Keyframe animations
    css += QStringLiteral(
        "@keyframes fadeIn { from { opacity: 0; } to { opacity: 1; } }\n"
        "@keyframes slideUp { from { opacity: 0; transform: translateY(18px); }"
        " to { opacity: 1; transform: translateY(0); } }\n"
        "@keyframes dotDrift { from { background-position: 0 0; }"
        " to { background-position: 28px 28px; } }\n"
        "@keyframes shimmer { 0% { background-position: -200% 0; }"
        " 100% { background-position: 200% 0; } }\n"
        "@keyframes pulse { 0%, 100% { opacity: 0.6; } 50% { opacity: 1; } }\n"
        "@keyframes glowBorder { 0%, 100% { border-color: rgba(110,106,179,0.3); }"
        " 50% { border-color: rgba(110,106,179,0.6); } }\n"
        "@keyframes floatUp { 0% { transform: translateY(0); }"
        " 50% { transform: translateY(-6px); } 100% { transform: translateY(0); } }\n");

    // Layout containers
    css += QStringLiteral(
        ".wrap { display: flex; flex-direction: column; align-items: center;"
        " justify-content: center; min-height: 100vh; padding: 40px 24px; }\n");
    css += QStringLiteral(
        ".container { max-width: 900px; margin: 0 auto; padding: 32px 24px;"
        " animation: fadeIn .4s ease; }\n");

    // Glass morphism card
    css += QStringLiteral(".card, .glass { background: ") + c["bg-mid"]
        + QStringLiteral("; border: 1px solid ") + c["border"]
        + QStringLiteral("; border-radius: 14px; padding: 18px;"
                         " backdrop-filter: blur(12px); -webkit-backdrop-filter: blur(12px);"
                         " transition: border-color .25s, background .25s, box-shadow .25s, transform .25s; }\n");
    css += QStringLiteral(".card:hover, .glass:hover { border-color: ") + c["purple-mid"]
        + QStringLiteral("; background: ") + c["bg-light"]
        + QStringLiteral("; box-shadow: 0 8px 32px rgba(110,106,179,0.12);"
                         " transform: translateY(-2px); }\n");

    // Header
    css += QStringLiteral(".header { text-align: center; margin-bottom: 40px;"
                          " animation: slideUp .4s ease; }\n");

    // Section titles with shimmer underline
    css += QStringLiteral(".section-title { font-size: 1.05em; font-weight: 600; color: ")
        + c["purple-light"]
        + QStringLiteral("; margin: 28px 0 14px; padding-bottom: 10px;"
                         " border-bottom: 2px solid ") + c["border"]
        + QStringLiteral("; letter-spacing: 0.02em;"
                         " background: linear-gradient(90deg, ") + c["purple-light"]
        + QStringLiteral(" 0%, ") + c["purple-glow"]
        + QStringLiteral(" 50%, ") + c["purple-light"]
        + QStringLiteral(" 100%); background-size: 200% 100%;"
                         " -webkit-background-clip: text; -webkit-text-fill-color: transparent;"
                         " animation: shimmer 4s ease infinite; }\n");

    // Buttons with glow effect
    css += QStringLiteral(".btn { display: inline-flex; align-items: center; gap: 6px;"
                          " padding: 9px 22px; border-radius: 10px; border: 1px solid ")
        + c["border"] + QStringLiteral("; color: ") + c["text-primary"]
        + QStringLiteral("; cursor: pointer; background: transparent; font-size: 0.85em;"
                         " transition: all .25s ease; text-decoration: none; }\n");
    css += QStringLiteral(".btn:hover { border-color: ") + c["purple-mid"]
        + QStringLiteral("; background: ") + c["bg-mid"]
        + QStringLiteral("; transform: translateY(-1px);"
                         " box-shadow: 0 4px 16px rgba(110,106,179,0.1); }\n");
    css += QStringLiteral(".btn-accent { background: ") + c["purple-mid"]
        + QStringLiteral("; color: ") + c["bg-darkest"]
        + QStringLiteral("; border-color: ") + c["purple-mid"]
        + QStringLiteral("; font-weight: 600; }\n");
    css += QStringLiteral(".btn-accent:hover { background: ") + c["purple-light"]
        + QStringLiteral("; box-shadow: 0 4px 20px rgba(110,106,179,0.25); }\n");

    // Badge
    css += QStringLiteral(
        ".badge { display: inline-block; padding: 3px 10px; border-radius: 8px;"
        " font-size: .72em; font-weight: 600; letter-spacing: 0.03em; }\n");

    // Scrollbar styling
    css += QStringLiteral(
        "::-webkit-scrollbar { width: 6px; }\n"
        "::-webkit-scrollbar-track { background: transparent; }\n"
        "::-webkit-scrollbar-thumb { background: rgba(110,106,179,0.25); border-radius: 3px; }\n"
        "::-webkit-scrollbar-thumb:hover { background: rgba(110,106,179,0.4); }\n");

    // Kbd tag
    css += QStringLiteral(
        ".kbd { display: inline-block; padding: 2px 7px; border-radius: 5px;"
        " background: rgba(255,255,255,0.06); border: 1px solid rgba(255,255,255,0.1);"
        " font-family: 'JetBrains Mono', monospace; font-size: .82em; }\n");

    return css;
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

    const QString dateStr = QDateTime::currentDateTime().toString(QStringLiteral("dddd, MMMM d"));

    const int indexedPages = stats[QStringLiteral("indexed_pages")].toInt();
    const int dbSites = stats[QStringLiteral("database_sites")].toInt();
    const int totalVisits = stats[QStringLiteral("total_visits")].toInt();

    // Build recent history items (max 8)
    QString historyItems;
    int hCount = 0;
    for (const auto &val : recentHistory) {
        if (hCount >= 8) break;
        auto h = val.toObject();
        QString title = h[QStringLiteral("title")].toString();
        QString url = h[QStringLiteral("url")].toString();
        if (title.isEmpty()) title = url;
        if (title.length() > 40) title = title.left(37) + QStringLiteral("...");
        QString domain = QUrl(url).host();
        QString delay = QString::number(0.1 + hCount * 0.05, 'f', 2);
        historyItems += QStringLiteral(
            "<a class='recent-item' href='%1' style='animation:slideUp .4s %4s ease both'>"
            "<div class='recent-favicon'>%5</div>"
            "<div class='recent-text'>"
            "<div class='recent-title'>%2</div>"
            "<div class='recent-domain'>%3</div>"
            "</div></a>")
            .arg(url.toHtmlEscaped(), title.toHtmlEscaped(), domain.toHtmlEscaped(),
                 delay, domain.left(1).toUpper());
        hCount++;
    }

    // Build top bookmarks (max 6)
    QString bookmarkItems;
    int bCount = 0;
    for (const auto &val : bookmarks) {
        if (bCount >= 6) break;
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

    // Build page via string concatenation
    QString html;
    html += QStringLiteral("<!DOCTYPE html><html><head><meta charset='utf-8'>"
                           "<title>OyNIx — New Tab</title><style>\n");
    html += sharedCss(c);

    // Page-specific styles
    html += QStringLiteral("\ncanvas#particles { position: fixed; inset: 0; z-index: -3;"
                           " pointer-events: none; }\n");

    // Floating orb
    html += QStringLiteral(".orb { position: fixed; width: 320px; height: 320px;"
                           " border-radius: 50%; filter: blur(80px); opacity: 0.12;"
                           " animation: orbFloat 12s ease-in-out infinite; z-index: -2;"
                           " background: linear-gradient(135deg, ")
        + c["purple-mid"] + QStringLiteral(", ") + c["purple-glow"]
        + QStringLiteral("); }\n"
                          ".orb-1 { top: 10%; left: 15%; }\n"
                          ".orb-2 { bottom: 20%; right: 10%; animation-delay: -6s;"
                          " width: 240px; height: 240px; }\n"
                          "@keyframes orbFloat { 0%,100% { transform: translate(0,0); }"
                          " 33% { transform: translate(30px,-20px); }"
                          " 66% { transform: translate(-20px,15px); } }\n");

    html += QStringLiteral(".home-wrap { display: flex; flex-direction: column; align-items: center;"
                           " min-height: 100vh; padding: 0 24px; position: relative; }\n");

    // Top section
    html += QStringLiteral(".top-section { width: 100%; max-width: 720px; padding-top: 12vh;"
                           " text-align: center; animation: fadeIn .6s ease; }\n");
    html += QStringLiteral(".greeting { font-size: 1.8em; font-weight: 300; color: ")
        + c["text-primary"] + QStringLiteral("; margin-bottom: 2px; letter-spacing: -0.02em; }\n");
    html += QStringLiteral(".date-display { font-size: .82em; color: ")
        + c["text-muted"] + QStringLiteral("; margin-bottom: 32px; }\n");

    // Logo with shimmer
    html += QStringLiteral(".logo-mark { font-size: 2.6em; font-weight: 800;"
                           " letter-spacing: -3px; margin-bottom: 36px;"
                           " background: linear-gradient(135deg, ")
        + c["purple-mid"] + QStringLiteral(" 0%, ") + c["purple-light"]
        + QStringLiteral(" 40%, ") + c["purple-glow"]
        + QStringLiteral(" 60%, ") + c["purple-light"]
        + QStringLiteral(" 100%); background-size: 200% 100%;"
                          " -webkit-background-clip: text; -webkit-text-fill-color: transparent;"
                          " animation: shimmer 6s ease infinite; }\n");

    // Search with glass effect
    html += QStringLiteral(".search-area { width: 100%; max-width: 600px; margin: 0 auto 36px;"
                           " position: relative; animation: slideUp .5s .05s ease both; }\n");
    html += QStringLiteral(".search-input { width: 100%; padding: 16px 52px 16px 22px; font-size: 1em;"
                           " background: ") + c["bg-mid"]
        + QStringLiteral("; border: 1px solid ") + c["border"]
        + QStringLiteral("; border-radius: 16px; color: ") + c["text-primary"]
        + QStringLiteral("; outline: none; transition: border-color .25s, box-shadow .25s;"
                         " backdrop-filter: blur(8px); }\n");
    html += QStringLiteral(".search-input:focus { border-color: ") + c["purple-mid"]
        + QStringLiteral("; box-shadow: 0 0 0 3px rgba(110,106,179,0.18),"
                         " 0 8px 32px rgba(110,106,179,0.08); }\n");
    html += QStringLiteral(".search-input::placeholder { color: ") + c["text-muted"]
        + QStringLiteral("; }\n");
    html += QStringLiteral(".search-icon { position: absolute; right: 18px; top: 50%;"
                           " transform: translateY(-50%); color: ") + c["text-muted"]
        + QStringLiteral("; font-size: 1.2em; pointer-events: none; }\n");
    html += QStringLiteral(".search-hint { text-align: center; margin-top: 10px;"
                           " font-size: .72em; color: ") + c["text-muted"]
        + QStringLiteral("; }\n");

    // Quick nav pills
    html += QStringLiteral(".quick-nav { display: flex; gap: 10px; justify-content: center;"
                           " flex-wrap: wrap; margin-bottom: 40px;"
                           " animation: slideUp .5s .1s ease both; }\n");
    html += QStringLiteral(".nav-pill { display: flex; align-items: center; gap: 7px;"
                           " padding: 9px 18px; border-radius: 12px; text-decoration: none;"
                           " color: ") + c["text-primary"]
        + QStringLiteral("; background: ") + c["bg-mid"]
        + QStringLiteral("; border: 1px solid ") + c["border"]
        + QStringLiteral("; font-size: .82em; font-weight: 500; transition: all .25s ease;"
                         " backdrop-filter: blur(8px); }\n");
    html += QStringLiteral(".nav-pill:hover { border-color: ") + c["purple-mid"]
        + QStringLiteral("; background: ") + c["bg-light"]
        + QStringLiteral("; box-shadow: 0 4px 16px rgba(110,106,179,0.12);"
                         " transform: translateY(-2px); }\n");

    // Stats with animated counters
    html += QStringLiteral(".stats-bar { display: flex; gap: 40px; justify-content: center;"
                           " margin-bottom: 40px; animation: slideUp .5s .15s ease both; }\n");
    html += QStringLiteral(".stat-item { text-align: center; }\n");
    html += QStringLiteral(".stat-value { font-size: 1.6em; font-weight: 700; color: ")
        + c["purple-mid"] + QStringLiteral("; }\n");
    html += QStringLiteral(".stat-label { font-size: .7em; color: ") + c["text-muted"]
        + QStringLiteral("; text-transform: uppercase; letter-spacing: 0.08em; margin-top: 3px; }\n");

    // Recent history
    html += QStringLiteral(".section-heading { font-size: .78em; font-weight: 600; color: ")
        + c["text-muted"]
        + QStringLiteral("; text-transform: uppercase; letter-spacing: 0.12em;"
                         " margin-bottom: 14px; text-align: left; max-width: 700px; width: 100%; }\n");
    html += QStringLiteral(".recent-grid { display: grid;"
                           " grid-template-columns: repeat(auto-fill, minmax(210px, 1fr));"
                           " gap: 12px; max-width: 700px; width: 100%; margin-bottom: 32px; }\n");
    html += QStringLiteral(".recent-item { display: flex; align-items: center; gap: 12px;"
                           " padding: 14px 16px; border-radius: 12px; background: ")
        + c["bg-mid"] + QStringLiteral("; border: 1px solid ") + c["border"]
        + QStringLiteral("; text-decoration: none; transition: all .25s ease; overflow: hidden;"
                         " backdrop-filter: blur(8px); }\n");
    html += QStringLiteral(".recent-item:hover { border-color: ") + c["purple-mid"]
        + QStringLiteral("; background: ") + c["bg-light"]
        + QStringLiteral("; transform: translateY(-2px);"
                         " box-shadow: 0 6px 24px rgba(110,106,179,0.1); }\n");
    html += QStringLiteral(".recent-favicon { width: 32px; height: 32px; border-radius: 8px;"
                           " background: rgba(110,106,179,0.15); display: flex;"
                           " align-items: center; justify-content: center; font-weight: 700;"
                           " font-size: .8em; color: ") + c["purple-light"]
        + QStringLiteral("; flex-shrink: 0; }\n");
    html += QStringLiteral(".recent-text { overflow: hidden; }\n");
    html += QStringLiteral(".recent-title { font-size: .84em; font-weight: 600; color: ")
        + c["text-primary"]
        + QStringLiteral("; white-space: nowrap; overflow: hidden; text-overflow: ellipsis;"
                         " margin-bottom: 2px; }\n");
    html += QStringLiteral(".recent-domain { font-size: .72em; color: ") + c["text-muted"]
        + QStringLiteral("; }\n");

    // Bookmark chips
    html += QStringLiteral(".bm-row { display: flex; gap: 8px; flex-wrap: wrap;"
                           " max-width: 700px; width: 100%; margin-bottom: 32px;"
                           " animation: slideUp .5s .3s ease both; }\n");
    html += QStringLiteral(".bm-chip { padding: 7px 16px; border-radius: 10px; font-size: .78em;"
                           " background: rgba(110,106,179,0.1); color: ") + c["purple-light"]
        + QStringLiteral("; border: 1px solid rgba(110,106,179,0.18);"
                         " text-decoration: none; transition: all .25s ease; }\n");
    html += QStringLiteral(".bm-chip:hover { background: rgba(110,106,179,0.2); border-color: ")
        + c["purple-mid"]
        + QStringLiteral("; transform: translateY(-1px);"
                         " box-shadow: 0 4px 12px rgba(110,106,179,0.1); }\n");

    // Footer
    html += QStringLiteral(".home-footer { position: fixed; bottom: 0; left: 0; right: 0;"
                           " display: flex; justify-content: space-between;"
                           " padding: 10px 24px; font-size: .68em; color: ")
        + c["text-muted"] + QStringLiteral("; backdrop-filter: blur(8px);"
                          " background: rgba(26,27,38,0.5); }\n");
    html += QStringLiteral(".home-footer a { color: ") + c["text-muted"]
        + QStringLiteral("; }\n");
    html += QStringLiteral(".home-footer a:hover { color: ") + c["purple-light"]
        + QStringLiteral("; }\n");

    html += QStringLiteral("</style></head><body>\n");

    // Particle canvas + floating orbs
    html += QStringLiteral("<canvas id='particles'></canvas>\n"
                           "<div class='orb orb-1'></div>\n"
                           "<div class='orb orb-2'></div>\n");

    html += QStringLiteral("<div class='home-wrap'>\n"
                           "  <div class='top-section'>\n"
                           "    <div class='greeting'>") + greeting
        + QStringLiteral("</div>\n    <div class='date-display'>") + dateStr
        + QStringLiteral("</div>\n    <div class='logo-mark'>OyNIx</div>\n"
                         "  </div>\n");

    // Search
    html += QStringLiteral(
        "  <div class='search-area'>\n"
        "    <input class='search-input' id='searchInput'"
        " placeholder='Search with Nyx or enter a URL...' autofocus"
        " onkeydown=\"if(event.key==='Enter'){var v=this.value.trim();"
        "if(v)window.location='oyn://search?q='+encodeURIComponent(v)}\">\n"
        "    <span class='search-icon'>&#x2315;</span>\n"
        "    <div class='search-hint'>Press <span class='kbd'>Enter</span> to search"
        " &middot; <span class='kbd'>Ctrl+L</span> focus URL bar</div>\n"
        "  </div>\n");

    // Quick nav
    html += QStringLiteral(
        "  <div class='quick-nav'>\n"
        "    <a class='nav-pill' href='oyn://bookmarks'><span>&#9733;</span> Bookmarks</a>\n"
        "    <a class='nav-pill' href='oyn://history'><span>&#8635;</span> History</a>\n"
        "    <a class='nav-pill' href='oyn://downloads'><span>&#8615;</span> Downloads</a>\n"
        "    <a class='nav-pill' href='oyn://settings'><span>&#9881;</span> Settings</a>\n"
        "    <a class='nav-pill' href='oyn://profiles'><span>&#9673;</span> Profiles</a>\n"
        "  </div>\n");

    // Stats
    html += QStringLiteral("  <div class='stats-bar'>\n")
        + QStringLiteral("    <div class='stat-item'><div class='stat-value'>")
        + QString::number(indexedPages)
        + QStringLiteral("</div><div class='stat-label'>Pages Indexed</div></div>\n")
        + QStringLiteral("    <div class='stat-item'><div class='stat-value'>")
        + QString::number(dbSites)
        + QStringLiteral("</div><div class='stat-label'>Sites in DB</div></div>\n")
        + QStringLiteral("    <div class='stat-item'><div class='stat-value'>")
        + QString::number(totalVisits)
        + QStringLiteral("</div><div class='stat-label'>Total Visits</div></div>\n")
        + QStringLiteral("  </div>\n");

    // Recent history section
    if (!historyItems.isEmpty()) {
        html += QStringLiteral("  <div class='section-heading'>Recently Visited</div>\n"
                               "  <div class='recent-grid'>") + historyItems
            + QStringLiteral("</div>\n");
    }

    // Bookmarks section
    if (!bookmarkItems.isEmpty()) {
        html += QStringLiteral("  <div class='section-heading'>Bookmarks</div>\n"
                               "  <div class='bm-row'>") + bookmarkItems
            + QStringLiteral("</div>\n");
    }

    html += QStringLiteral("</div>\n");

    // Footer
    html += QStringLiteral(
        "<div class='home-footer'>\n"
        "  <span>OyNIx Browser v3.1</span>\n"
        "  <span><span class='kbd'>Ctrl+K</span> Command Palette &middot;"
        " <span class='kbd'>Ctrl+Shift+A</span> AI Assistant</span>\n"
        "</div>\n");

    // Particle animation script
    html += QStringLiteral(R"JS(
<script>
(function(){
  var c=document.getElementById('particles'),x=c.getContext('2d');
  function resize(){c.width=window.innerWidth;c.height=window.innerHeight;}
  resize(); window.addEventListener('resize',resize);
  var pts=[];
  for(var i=0;i<60;i++) pts.push({
    x:Math.random()*c.width, y:Math.random()*c.height,
    vx:(Math.random()-0.5)*0.3, vy:(Math.random()-0.5)*0.3,
    r:Math.random()*1.5+0.5, a:Math.random()*0.3+0.1
  });
  function draw(){
    x.clearRect(0,0,c.width,c.height);
    for(var i=0;i<pts.length;i++){
      var p=pts[i];
      p.x+=p.vx; p.y+=p.vy;
      if(p.x<0)p.x=c.width; if(p.x>c.width)p.x=0;
      if(p.y<0)p.y=c.height; if(p.y>c.height)p.y=0;
      x.beginPath(); x.arc(p.x,p.y,p.r,0,6.28);
      x.fillStyle='rgba(110,106,179,'+p.a+')'; x.fill();
      for(var j=i+1;j<pts.length;j++){
        var q=pts[j], dx=p.x-q.x, dy=p.y-q.y, d=Math.sqrt(dx*dx+dy*dy);
        if(d<120){
          x.beginPath(); x.moveTo(p.x,p.y); x.lineTo(q.x,q.y);
          x.strokeStyle='rgba(110,106,179,'+(0.06*(1-d/120))+')';
          x.stroke();
        }
      }
    }
    requestAnimationFrame(draw);
  }
  draw();
  document.getElementById('searchInput').focus();
})();
</script>
)JS");

    html += QStringLiteral("</body></html>");
    return html;
}

// ── Search Results ─────────────────────────────────────────────────
QString searchPage(const QString &query,
                   const QJsonArray &localResults,
                   const QJsonArray &webResults,
                   const QMap<QString, QString> &c) {

    auto buildCards = [&](const QJsonArray &results, const QString &badgeClass,
                          const QString &badgeLabel) -> QString {
        QString cards;
        int i = 0;
        for (const auto &val : results) {
            auto r = val.toObject();
            QString delay = QString::number(0.06 + i * 0.04, 'f', 2);
            QString url = r[QStringLiteral("url")].toString().toHtmlEscaped();
            QString title = r[QStringLiteral("title")].toString().toHtmlEscaped();
            QString desc = r[QStringLiteral("description")].toString().toHtmlEscaped();
            QString domain = QUrl(r[QStringLiteral("url")].toString()).host().toHtmlEscaped();
            if (title.isEmpty()) title = url;

            cards += QStringLiteral(
                "<div class='result' style='animation:slideUp .35s %1s ease both'>"
                "<div class='r-meta'>"
                "<span class='r-favicon'>%7</span>"
                "<span class='r-domain'>%2</span>"
                "<span class='badge %3'>%8</span>"
                "</div>"
                "<a class='r-title' href='%4'>%5</a>"
                "<div class='r-url'>%4</div>"
                "<div class='r-desc'>%6</div>"
                "</div>")
                .arg(delay, domain, badgeClass, url, title, desc,
                     domain.left(1).toUpper(), badgeLabel);
            i++;
        }
        return cards;
    };

    int total = localResults.size() + webResults.size();

    // Build with string concatenation
    QString html;
    html += QStringLiteral("<!DOCTYPE html><html><head><meta charset='utf-8'>"
                           "<title>Nyx: ") + query.toHtmlEscaped()
        + QStringLiteral("</title><style>\n");
    html += sharedCss(c);

    // Search header
    html += QStringLiteral("\n.search-header { text-align: center; padding: 28px 0 20px;"
                           " animation: fadeIn .4s ease; }\n");
    html += QStringLiteral(".search-logo { font-size: 1.5em; font-weight: 800;"
                           " letter-spacing: -1px; margin-bottom: 18px;"
                           " background: linear-gradient(135deg, ")
        + c["purple-mid"] + QStringLiteral(", ") + c["purple-light"]
        + QStringLiteral("); background-size: 200% 100%;"
                         " -webkit-background-clip: text; -webkit-text-fill-color: transparent;"
                         " animation: shimmer 5s ease infinite; }\n");

    // Search input
    html += QStringLiteral(".search-again { max-width: 580px; margin: 0 auto 10px;"
                           " position: relative; }\n");
    html += QStringLiteral(".search-again input { width: 100%; padding: 13px 20px;"
                           " font-size: .95em; background: ") + c["bg-mid"]
        + QStringLiteral("; border: 1px solid ") + c["border"]
        + QStringLiteral("; border-radius: 12px; color: ") + c["text-primary"]
        + QStringLiteral("; outline: none; transition: border-color .25s, box-shadow .25s;"
                         " backdrop-filter: blur(8px); }\n");
    html += QStringLiteral(".search-again input:focus { border-color: ") + c["purple-mid"]
        + QStringLiteral("; box-shadow: 0 0 0 3px rgba(110,106,179,0.14); }\n");

    html += QStringLiteral(".result-count { text-align: center; color: ") + c["text-muted"]
        + QStringLiteral("; font-size: .82em; margin-bottom: 28px; }\n");
    html += QStringLiteral(".results-container { max-width: 700px; margin: 0 auto; }\n");

    // Result cards with glass effect
    html += QStringLiteral(".result { background: ") + c["bg-mid"]
        + QStringLiteral("; border: 1px solid ") + c["border"]
        + QStringLiteral("; border-radius: 14px; padding: 18px 22px; margin-bottom: 12px;"
                         " transition: all .25s ease; backdrop-filter: blur(8px); }\n");
    html += QStringLiteral(".result:hover { border-color: ") + c["purple-mid"]
        + QStringLiteral("; transform: translateX(6px);"
                         " box-shadow: 0 6px 28px rgba(110,106,179,0.12); }\n");
    html += QStringLiteral(".r-meta { display: flex; align-items: center; gap: 8px;"
                           " margin-bottom: 8px; }\n");
    html += QStringLiteral(".r-favicon { width: 22px; height: 22px; border-radius: 6px;"
                           " background: rgba(110,106,179,0.15); display: flex;"
                           " align-items: center; justify-content: center;"
                           " font-size: .65em; font-weight: 700; color: ")
        + c["purple-light"] + QStringLiteral("; }\n");
    html += QStringLiteral(".r-domain { font-size: .75em; color: ") + c["text-muted"]
        + QStringLiteral("; font-weight: 500; }\n");
    html += QStringLiteral(".r-title { color: ") + c["purple-light"]
        + QStringLiteral("; font-size: 1.05em; font-weight: 600; display: block;"
                         " margin-bottom: 4px; transition: color .2s; text-decoration: none; }\n");
    html += QStringLiteral(".r-title:hover { color: ") + c["purple-glow"]
        + QStringLiteral("; }\n");
    html += QStringLiteral(".r-url { color: ") + c["text-muted"]
        + QStringLiteral("; font-size: .73em; margin-bottom: 6px;"
                         " white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }\n");
    html += QStringLiteral(".r-desc { color: ") + c["text-secondary"]
        + QStringLiteral("; line-height: 1.55; font-size: .88em; }\n");

    // Badges
    html += QStringLiteral(
        ".badge.nyx { background: rgba(110,106,179,0.18); color: ") + c["purple-light"]
        + QStringLiteral("; }\n");
    html += QStringLiteral(
        ".badge.web { background: rgba(80,80,100,0.25); color: ") + c["text-secondary"]
        + QStringLiteral("; }\n");

    // No results / loading states
    html += QStringLiteral(".no-results { text-align: center; padding: 60px 20px;"
                           " animation: fadeIn .5s ease; }\n");
    html += QStringLiteral(".no-results-icon { font-size: 3.5em; margin-bottom: 16px;"
                           " opacity: 0.4; animation: floatUp 3s ease infinite; }\n");
    html += QStringLiteral(".no-results-text { font-size: 1.1em; color: ")
        + c["text-primary"] + QStringLiteral("; margin-bottom: 8px; }\n");
    html += QStringLiteral(".no-results-hint { color: ") + c["text-muted"]
        + QStringLiteral("; font-size: .88em; }\n");
    html += QStringLiteral(".web-loading { text-align: center; padding: 24px; color: ")
        + c["text-muted"]
        + QStringLiteral("; font-size: .85em; animation: pulse 1.5s ease infinite; }\n");

    html += QStringLiteral("</style></head><body>\n<div class='container'>\n");

    // Header + search
    html += QStringLiteral("  <div class='search-header'>\n"
                           "    <div class='search-logo'>Nyx Search</div>\n"
                           "    <div class='search-again'>\n"
                           "      <input type='text' value='")
        + query.toHtmlEscaped()
        + QStringLiteral("' autofocus onkeydown=\"if(event.key==='Enter'){"
                         "var v=this.value.trim();if(v)window.location="
                         "'oyn://search?q='+encodeURIComponent(v)}\">\n"
                         "    </div>\n  </div>\n");

    html += QStringLiteral("  <div class='result-count'>")
        + QString::number(total) + QStringLiteral(" results found</div>\n");

    html += QStringLiteral("  <div class='results-container'>\n");

    // Local results
    if (!localResults.isEmpty()) {
        html += QStringLiteral("    <div class='section-title'>Local &amp; Database</div>\n");
        html += buildCards(localResults, QStringLiteral("nyx"), QStringLiteral("NYX"));
    }

    // Web results or loading/empty state
    if (!webResults.isEmpty()) {
        html += QStringLiteral("    <div class='section-title'>Web Results</div>\n");
        html += buildCards(webResults, QStringLiteral("web"), QStringLiteral("WEB"));
    } else if (total == 0) {
        html += QStringLiteral(
            "    <div class='no-results'>"
            "<div class='no-results-icon'>&#128269;</div>"
            "<div class='no-results-text'>No results found for \"")
            + query.toHtmlEscaped()
            + QStringLiteral("\"</div>"
                             "<div class='no-results-hint'>Try different keywords or browse the web directly</div>"
                             "<a class='btn btn-accent' href='https://duckduckgo.com/?q=")
            + QString::fromUtf8(QUrl::toPercentEncoding(query))
            + QStringLiteral("' style='margin-top:16px'>Search DuckDuckGo</a></div>\n");
    } else {
        html += QStringLiteral("    <div class='web-loading'>Fetching web results...</div>\n");
    }

    html += QStringLiteral("  </div>\n</div>\n</body></html>");
    return html;
}

// ── Bookmarks Page ─────────────────────────────────────────────────
QString bookmarksPage(const QJsonArray &bookmarks, const QMap<QString, QString> &c) {
    QString items;
    int i = 0;
    for (const auto &val : bookmarks) {
        auto b = val.toObject();
        QString delay = QString::number(0.05 + i * 0.03, 'f', 2);
        QString url = b[QStringLiteral("url")].toString().toHtmlEscaped();
        QString title = b[QStringLiteral("title")].toString().toHtmlEscaped();
        QString domain = QUrl(b[QStringLiteral("url")].toString()).host().toHtmlEscaped();
        QString folder = b[QStringLiteral("folder")].toString().toHtmlEscaped();
        if (title.isEmpty()) title = url;

        items += QStringLiteral(
            "<div class='bm-card' style='animation:slideUp .35s %1s ease both'>"
            "<div class='bm-icon'>%6</div>"
            "<a class='bm-title' href='%2'>%3</a>"
            "<div class='bm-domain'>%4</div>"
            "<div class='bm-footer'>"
            "<span class='bm-folder'>%5</span>"
            "</div></div>")
            .arg(delay, url, title, domain,
                 folder.isEmpty() ? QStringLiteral("Unsorted") : folder,
                 domain.left(1).toUpper());
        i++;
    }

    // Build via concatenation
    QString html;
    html += QStringLiteral("<!DOCTYPE html><html><head><meta charset='utf-8'>"
                           "<title>Bookmarks</title><style>\n");
    html += sharedCss(c);

    // Page header
    html += QStringLiteral("\n.page-title { font-size: 1.7em; font-weight: 700; color: ")
        + c["purple-light"]
        + QStringLiteral("; margin-bottom: 4px;"
                         " background: linear-gradient(135deg, ") + c["purple-mid"]
        + QStringLiteral(", ") + c["purple-light"]
        + QStringLiteral("); -webkit-background-clip: text;"
                         " -webkit-text-fill-color: transparent; }\n");
    html += QStringLiteral(".page-subtitle { color: ") + c["text-muted"]
        + QStringLiteral("; font-size: .85em; margin-bottom: 8px; }\n");

    // Filter input
    html += QStringLiteral(".search-filter { max-width: 420px; margin: 0 auto 28px; }\n");
    html += QStringLiteral(".search-filter input { width: 100%; padding: 11px 18px;"
                           " font-size: .9em; background: ") + c["bg-mid"]
        + QStringLiteral("; border: 1px solid ") + c["border"]
        + QStringLiteral("; border-radius: 12px; color: ") + c["text-primary"]
        + QStringLiteral("; outline: none; transition: border-color .25s, box-shadow .25s;"
                         " backdrop-filter: blur(8px); }\n");
    html += QStringLiteral(".search-filter input:focus { border-color: ") + c["purple-mid"]
        + QStringLiteral("; box-shadow: 0 0 0 3px rgba(110,106,179,0.14); }\n");

    // Grid + cards with glass
    html += QStringLiteral(".bm-grid { display: grid;"
                           " grid-template-columns: repeat(auto-fill, minmax(260px, 1fr));"
                           " gap: 14px; }\n");
    html += QStringLiteral(".bm-card { background: ") + c["bg-mid"]
        + QStringLiteral("; border: 1px solid ") + c["border"]
        + QStringLiteral("; border-radius: 14px; padding: 18px 20px;"
                         " transition: all .25s ease; backdrop-filter: blur(8px);"
                         " position: relative; overflow: hidden; }\n");
    html += QStringLiteral(".bm-card::before { content: ''; position: absolute;"
                           " top: 0; left: 0; right: 0; height: 3px;"
                           " background: linear-gradient(90deg, transparent, ")
        + c["purple-mid"] + QStringLiteral(", transparent);"
                          " opacity: 0; transition: opacity .25s; }\n");
    html += QStringLiteral(".bm-card:hover { border-color: ") + c["purple-mid"]
        + QStringLiteral("; transform: translateY(-3px);"
                         " box-shadow: 0 8px 28px rgba(110,106,179,0.12); }\n");
    html += QStringLiteral(".bm-card:hover::before { opacity: 1; }\n");

    html += QStringLiteral(".bm-icon { width: 36px; height: 36px; border-radius: 10px;"
                           " background: rgba(110,106,179,0.12); display: flex;"
                           " align-items: center; justify-content: center; font-weight: 700;"
                           " font-size: .9em; color: ") + c["purple-light"]
        + QStringLiteral("; margin-bottom: 10px; }\n");
    html += QStringLiteral(".bm-title { font-size: .95em; font-weight: 600; color: ")
        + c["purple-light"]
        + QStringLiteral("; text-decoration: none; display: block; margin-bottom: 6px;"
                         " white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }\n");
    html += QStringLiteral(".bm-title:hover { color: ") + c["purple-glow"]
        + QStringLiteral("; }\n");
    html += QStringLiteral(".bm-domain { font-size: .75em; color: ") + c["text-muted"]
        + QStringLiteral("; margin-bottom: 12px; }\n");
    html += QStringLiteral(".bm-footer { display: flex; justify-content: space-between;"
                           " align-items: center; }\n");
    html += QStringLiteral(".bm-folder { font-size: .7em; padding: 3px 10px; border-radius: 8px;"
                           " background: rgba(110,106,179,0.1); color: ") + c["purple-mid"]
        + QStringLiteral("; }\n");

    // Empty state
    html += QStringLiteral(".empty-state { text-align: center; padding: 60px 20px;"
                           " animation: fadeIn .5s ease; }\n");
    html += QStringLiteral(".empty-icon { font-size: 3.5em; margin-bottom: 16px;"
                           " opacity: 0.35; animation: floatUp 3s ease infinite; }\n");
    html += QStringLiteral(".empty-text { font-size: 1.1em; color: ") + c["text-primary"]
        + QStringLiteral("; margin-bottom: 8px; }\n");
    html += QStringLiteral(".empty-hint { color: ") + c["text-muted"]
        + QStringLiteral("; font-size: .85em; }\n");

    html += QStringLiteral("</style></head><body>\n<div class='container'>\n");
    html += QStringLiteral("<div class='header'>\n"
                           "  <div class='page-title'>&#9733; Bookmarks</div>\n"
                           "  <div class='page-subtitle'>")
        + QString::number(bookmarks.size())
        + QStringLiteral(" saved bookmarks</div>\n</div>\n");

    html += QStringLiteral(
        "<div class='search-filter'>\n"
        "  <input type='text' placeholder='Filter bookmarks...' id='bmFilter'"
        " oninput=\"var q=this.value.toLowerCase();"
        "document.querySelectorAll('.bm-card').forEach(function(c){"
        "c.style.display=c.textContent.toLowerCase().includes(q)?'':'none'})\">\n"
        "</div>\n");

    if (bookmarks.isEmpty()) {
        html += QStringLiteral("<div class='empty-state'>"
                               "<div class='empty-icon'>&#9733;</div>"
                               "<div class='empty-text'>No bookmarks yet</div>"
                               "<div class='empty-hint'>Press <span class='kbd'>Ctrl+D</span>"
                               " to bookmark a page</div></div>\n");
    } else {
        html += QStringLiteral("<div class='bm-grid'>") + items
            + QStringLiteral("</div>\n");
    }

    html += QStringLiteral("</div>\n</body></html>");
    return html;
}

// ── History Page ───────────────────────────────────────────────────
QString historyPage(const QJsonArray &history, const QMap<QString, QString> &c) {
    QString items;
    int i = 0;
    for (const auto &val : history) {
        auto h = val.toObject();
        if (i >= 200) break;
        QString delay = QString::number(0.03 + i * 0.012, 'f', 3);
        QString url = h[QStringLiteral("url")].toString().toHtmlEscaped();
        QString title = h[QStringLiteral("title")].toString().toHtmlEscaped();
        QString timeStr = h[QStringLiteral("time")].toString();
        QString domain = QUrl(h[QStringLiteral("url")].toString()).host().toHtmlEscaped();
        if (title.isEmpty()) title = url;

        items += QStringLiteral(
            "<div class='hist-item' style='animation:slideUp .3s %1s ease both'>"
            "<div class='hist-dot'></div>"
            "<div class='hist-content'>"
            "<div class='hist-main'>"
            "<a class='hist-title' href='%2'>%3</a>"
            "<span class='hist-time'>%4</span>"
            "</div>"
            "<div class='hist-url'>%5 &mdash; %2</div>"
            "</div></div>")
            .arg(delay, url, title, timeStr, domain);
        i++;
    }

    // Build via concatenation
    QString html;
    html += QStringLiteral("<!DOCTYPE html><html><head><meta charset='utf-8'>"
                           "<title>History</title><style>\n");
    html += sharedCss(c);

    // Page header
    html += QStringLiteral("\n.page-title { font-size: 1.7em; font-weight: 700;"
                           " background: linear-gradient(135deg, ") + c["purple-mid"]
        + QStringLiteral(", ") + c["purple-light"]
        + QStringLiteral("); -webkit-background-clip: text;"
                         " -webkit-text-fill-color: transparent; margin-bottom: 4px; }\n");
    html += QStringLiteral(".page-subtitle { color: ") + c["text-muted"]
        + QStringLiteral("; font-size: .85em; margin-bottom: 8px; }\n");

    // Filter
    html += QStringLiteral(".search-filter { max-width: 420px; margin: 0 auto 28px; }\n");
    html += QStringLiteral(".search-filter input { width: 100%; padding: 11px 18px;"
                           " font-size: .9em; background: ") + c["bg-mid"]
        + QStringLiteral("; border: 1px solid ") + c["border"]
        + QStringLiteral("; border-radius: 12px; color: ") + c["text-primary"]
        + QStringLiteral("; outline: none; transition: border-color .25s, box-shadow .25s;"
                         " backdrop-filter: blur(8px); }\n");
    html += QStringLiteral(".search-filter input:focus { border-color: ") + c["purple-mid"]
        + QStringLiteral("; box-shadow: 0 0 0 3px rgba(110,106,179,0.14); }\n");

    // Timeline items
    html += QStringLiteral(".hist-list { position: relative; padding-left: 24px; }\n");
    html += QStringLiteral(".hist-list::before { content: ''; position: absolute;"
                           " left: 7px; top: 0; bottom: 0; width: 2px;"
                           " background: linear-gradient(to bottom, ")
        + c["purple-mid"] + QStringLiteral(" 0%, ") + c["border"]
        + QStringLiteral(" 100%); border-radius: 1px; }\n");

    html += QStringLiteral(".hist-item { display: flex; align-items: flex-start; gap: 14px;"
                           " padding: 12px 16px 12px 0; position: relative;"
                           " transition: background .2s; border-radius: 10px; margin-bottom: 2px; }\n");
    html += QStringLiteral(".hist-item:hover { background: ") + c["bg-mid"]
        + QStringLiteral("; }\n");
    html += QStringLiteral(".hist-dot { width: 10px; height: 10px; border-radius: 50%;"
                           " background: ") + c["purple-mid"]
        + QStringLiteral("; flex-shrink: 0; margin-top: 6px; position: relative;"
                         " z-index: 1; margin-left: -18px;"
                         " box-shadow: 0 0 0 3px ") + c["bg-darkest"]
        + QStringLiteral("; transition: all .25s; }\n");
    html += QStringLiteral(".hist-item:hover .hist-dot { background: ") + c["purple-glow"]
        + QStringLiteral("; box-shadow: 0 0 0 3px ") + c["bg-mid"]
        + QStringLiteral(", 0 0 8px rgba(110,106,179,0.3); }\n");

    html += QStringLiteral(".hist-content { flex: 1; overflow: hidden; }\n");
    html += QStringLiteral(".hist-main { display: flex; justify-content: space-between;"
                           " align-items: baseline; margin-bottom: 4px; }\n");
    html += QStringLiteral(".hist-title { font-size: .92em; font-weight: 600; color: ")
        + c["purple-light"]
        + QStringLiteral("; text-decoration: none; flex: 1; white-space: nowrap;"
                         " overflow: hidden; text-overflow: ellipsis; margin-right: 12px;"
                         " transition: color .2s; }\n");
    html += QStringLiteral(".hist-title:hover { color: ") + c["purple-glow"]
        + QStringLiteral("; }\n");
    html += QStringLiteral(".hist-time { font-size: .73em; color: ") + c["text-muted"]
        + QStringLiteral("; white-space: nowrap; font-variant-numeric: tabular-nums; }\n");
    html += QStringLiteral(".hist-url { font-size: .72em; color: ") + c["text-muted"]
        + QStringLiteral("; white-space: nowrap; overflow: hidden; text-overflow: ellipsis; }\n");

    // Clear button
    html += QStringLiteral(".clear-btn { display: inline-flex; align-items: center; gap: 6px;"
                           " padding: 9px 22px; margin-top: 24px; border-radius: 10px;"
                           " border: 1px solid ") + c["border"]
        + QStringLiteral("; color: ") + c["text-muted"]
        + QStringLiteral("; font-size: .82em; cursor: pointer; background: transparent;"
                         " transition: all .25s; text-decoration: none; }\n");
    html += QStringLiteral(".clear-btn:hover { border-color: #e05050; color: #e05050;"
                           " box-shadow: 0 4px 16px rgba(224,80,80,0.1); }\n");

    // Empty state
    html += QStringLiteral(".empty-state { text-align: center; padding: 60px 20px;"
                           " animation: fadeIn .5s ease; }\n");
    html += QStringLiteral(".empty-icon { font-size: 3.5em; margin-bottom: 16px;"
                           " opacity: 0.35; animation: floatUp 3s ease infinite; }\n");
    html += QStringLiteral(".empty-text { font-size: 1.1em; color: ") + c["text-primary"]
        + QStringLiteral("; margin-bottom: 8px; }\n");
    html += QStringLiteral(".empty-hint { color: ") + c["text-muted"]
        + QStringLiteral("; font-size: .85em; }\n");

    html += QStringLiteral("</style></head><body>\n<div class='container'>\n");
    html += QStringLiteral("<div class='header'>\n"
                           "  <div class='page-title'>&#128337; History</div>\n"
                           "  <div class='page-subtitle'>")
        + QString::number(history.size())
        + QStringLiteral(" entries</div>\n</div>\n");

    html += QStringLiteral(
        "<div class='search-filter'>\n"
        "  <input type='text' placeholder='Search history...' id='histFilter'"
        " oninput=\"var q=this.value.toLowerCase();"
        "document.querySelectorAll('.hist-item').forEach(function(c){"
        "c.style.display=c.textContent.toLowerCase().includes(q)?'':'none'})\">\n"
        "</div>\n");

    if (history.isEmpty()) {
        html += QStringLiteral("<div class='empty-state'>"
                               "<div class='empty-icon'>&#128337;</div>"
                               "<div class='empty-text'>No browsing history</div>"
                               "<div class='empty-hint'>Pages you visit will appear here</div>"
                               "</div>\n");
    } else {
        html += QStringLiteral("<div class='hist-list'>") + items
            + QStringLiteral("</div>\n");
    }

    html += QStringLiteral("</div>\n</body></html>");
    return html;
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

    // Build toggles
    QString searchToggles;
    searchToggles += toggle(QStringLiteral("auto_index_pages"),
                            QStringLiteral("Auto-index pages"),
                            QStringLiteral("Index visited pages for Nyx local search"));
    searchToggles += toggle(QStringLiteral("auto_expand_database"),
                            QStringLiteral("Expand site database"),
                            QStringLiteral("Automatically add discovered sites to the database"));

    QString privacyToggles;
    privacyToggles += toggle(QStringLiteral("show_security_prompts"),
                             QStringLiteral("Security prompts"),
                             QStringLiteral("Show warnings when visiting login pages on untrusted sites"));

    QString appearToggles;
    appearToggles += toggle(QStringLiteral("force_nyx_theme_external"),
                            QStringLiteral("Theme external pages"),
                            QStringLiteral("Apply Nyx dark theme to external search engine results"));
    appearToggles += toggle(QStringLiteral("restore_session"),
                            QStringLiteral("Restore session"),
                            QStringLiteral("Reopen previous tabs when starting the browser"));

    // Build via concatenation
    QString html;
    html += QStringLiteral("<!DOCTYPE html><html><head><meta charset='utf-8'>"
                           "<title>Settings</title><style>\n");
    html += sharedCss(c);

    // Page header
    html += QStringLiteral("\n.page-title { font-size: 1.7em; font-weight: 700;"
                           " background: linear-gradient(135deg, ") + c["purple-mid"]
        + QStringLiteral(", ") + c["purple-light"]
        + QStringLiteral("); -webkit-background-clip: text;"
                         " -webkit-text-fill-color: transparent; margin-bottom: 4px; }\n");
    html += QStringLiteral(".page-subtitle { color: ") + c["text-muted"]
        + QStringLiteral("; font-size: .85em; }\n");

    // Setting sections with glass
    html += QStringLiteral(".setting-section { background: ") + c["bg-mid"]
        + QStringLiteral("; border: 1px solid ") + c["border"]
        + QStringLiteral("; border-radius: 16px; padding: 22px 26px; margin-bottom: 18px;"
                         " backdrop-filter: blur(8px); transition: border-color .25s;"
                         " animation: slideUp .4s ease both; }\n");
    html += QStringLiteral(".setting-section:nth-child(2) { animation-delay: .05s; }\n");
    html += QStringLiteral(".setting-section:nth-child(3) { animation-delay: .1s; }\n");
    html += QStringLiteral(".setting-section:hover { border-color: rgba(110,106,179,0.3); }\n");
    html += QStringLiteral(".setting-section h3 { color: ") + c["purple-light"]
        + QStringLiteral("; font-size: 1em; font-weight: 600; margin-bottom: 16px;"
                         " padding-bottom: 12px; border-bottom: 1px solid ") + c["border"]
        + QStringLiteral("; display: flex; align-items: center; gap: 8px; }\n");

    // Setting rows
    html += QStringLiteral(".setting-row { display: flex; justify-content: space-between;"
                           " align-items: center; padding: 12px 0;"
                           " border-bottom: 1px solid rgba(255,255,255,0.03);"
                           " transition: background .15s; border-radius: 6px; }\n");
    html += QStringLiteral(".setting-row:last-child { border-bottom: none; }\n");
    html += QStringLiteral(".setting-row:hover { background: rgba(110,106,179,0.04);"
                           " padding-left: 8px; padding-right: 8px; }\n");
    html += QStringLiteral(".setting-info { flex: 1; margin-right: 16px; }\n");
    html += QStringLiteral(".setting-label { font-size: .9em; color: ") + c["text-primary"]
        + QStringLiteral("; font-weight: 500; }\n");
    html += QStringLiteral(".setting-desc { font-size: .75em; color: ") + c["text-muted"]
        + QStringLiteral("; margin-top: 3px; }\n");

    // Toggle switch with glow
    html += QStringLiteral(
        ".toggle { position: relative; display: inline-block; width: 46px; height: 26px; }\n"
        ".toggle input { opacity: 0; width: 0; height: 0; }\n");
    html += QStringLiteral(".toggle-slider { position: absolute; cursor: pointer; inset: 0;"
                           " background: ") + c["border"]
        + QStringLiteral("; border-radius: 26px; transition: .3s; }\n");
    html += QStringLiteral(".toggle-slider:before { content: ''; position: absolute;"
                           " height: 20px; width: 20px; left: 3px; bottom: 3px; background: ")
        + c["text-muted"]
        + QStringLiteral("; border-radius: 50%; transition: .3s; }\n");
    html += QStringLiteral(".toggle input:checked + .toggle-slider { background: ")
        + c["purple-mid"]
        + QStringLiteral("; box-shadow: 0 0 12px rgba(110,106,179,0.3); }\n");
    html += QStringLiteral(".toggle input:checked + .toggle-slider:before {"
                           " transform: translateX(20px); background: white; }\n");

    // About card
    html += QStringLiteral(".about-card { text-align: center; background: ") + c["bg-mid"]
        + QStringLiteral("; border: 1px solid ") + c["border"]
        + QStringLiteral("; border-radius: 16px; padding: 36px 28px; margin-top: 28px;"
                         " backdrop-filter: blur(8px); animation: slideUp .4s .15s ease both; }\n");
    html += QStringLiteral(".about-version { font-size: 1.2em; font-weight: 700;"
                           " margin-bottom: 6px;"
                           " background: linear-gradient(135deg, ") + c["purple-mid"]
        + QStringLiteral(", ") + c["purple-light"]
        + QStringLiteral("); -webkit-background-clip: text;"
                         " -webkit-text-fill-color: transparent; }\n");
    html += QStringLiteral(".about-tech { font-size: .8em; color: ") + c["text-muted"]
        + QStringLiteral("; }\n");
    html += QStringLiteral(".about-shortcuts { margin-top: 22px; font-size: .78em; color: ")
        + c["text-muted"] + QStringLiteral("; line-height: 2; }\n");

    html += QStringLiteral("</style></head><body>\n<div class='container'>\n");
    html += QStringLiteral("<div class='header'>\n"
                           "  <div class='page-title'>&#9881; Settings</div>\n"
                           "  <div class='page-subtitle'>Configure your browser</div>\n"
                           "</div>\n");

    // Search section
    html += QStringLiteral("<div class='setting-section'>\n"
                           "<h3>&#128269; Search &amp; Indexing</h3>\n")
        + searchToggles + QStringLiteral("</div>\n");

    // Privacy section
    html += QStringLiteral("<div class='setting-section'>\n"
                           "<h3>&#128274; Privacy &amp; Security</h3>\n")
        + privacyToggles + QStringLiteral("</div>\n");

    // Appearance section
    html += QStringLiteral("<div class='setting-section'>\n"
                           "<h3>&#127912; Appearance</h3>\n")
        + appearToggles + QStringLiteral("</div>\n");

    // About card
    html += QStringLiteral(
        "<div class='about-card'>\n"
        "  <div class='about-version'>OyNIx Browser v3.1</div>\n"
        "  <div class='about-tech'>Qt6 WebEngine &middot; C++17 &middot; .NET 8 NativeAOT Core</div>\n"
        "  <div class='about-shortcuts'>\n"
        "    <span class='kbd'>Ctrl+T</span> New Tab &middot;\n"
        "    <span class='kbd'>Ctrl+W</span> Close Tab &middot;\n"
        "    <span class='kbd'>Ctrl+L</span> Focus URL &middot;\n"
        "    <span class='kbd'>Ctrl+K</span> Command Palette &middot;\n"
        "    <span class='kbd'>F11</span> Fullscreen\n"
        "  </div>\n"
        "</div>\n");

    html += QStringLiteral("</div>\n</body></html>");
    return html;
}

// ── Downloads Page ─────────────────────────────────────────────────
QString downloadsPage(const QJsonArray &downloads, const QMap<QString, QString> &c) {
    QString items;
    int i = 0;
    for (const auto &val : downloads) {
        auto d = val.toObject();
        QString delay = QString::number(0.05 + i * 0.03, 'f', 2);
        QString filename = d[QStringLiteral("filename")].toString().toHtmlEscaped();
        QString size = d[QStringLiteral("size")].toString();
        QString status = d[QStringLiteral("status")].toString();

        // Pick icon based on status
        QString statusBadge;
        if (status == QStringLiteral("complete"))
            statusBadge = QStringLiteral("<span class='dl-badge dl-complete'>Complete</span>");
        else if (status == QStringLiteral("downloading"))
            statusBadge = QStringLiteral("<span class='dl-badge dl-active'>Downloading</span>");
        else
            statusBadge = QStringLiteral("<span class='dl-badge'>%1</span>").arg(status);

        items += QStringLiteral(
            "<div class='dl-item' style='animation:slideUp .3s %1s ease both'>"
            "<div class='dl-icon-wrap'><div class='dl-icon'>&#128196;</div></div>"
            "<div class='dl-info'>"
            "<div class='dl-name'>%2</div>"
            "<div class='dl-meta'>%3 &middot; %4</div>"
            "</div></div>")
            .arg(delay, filename, size, statusBadge);
        i++;
    }

    // Build via concatenation
    QString html;
    html += QStringLiteral("<!DOCTYPE html><html><head><meta charset='utf-8'>"
                           "<title>Downloads</title><style>\n");
    html += sharedCss(c);

    // Page header
    html += QStringLiteral("\n.page-title { font-size: 1.7em; font-weight: 700;"
                           " background: linear-gradient(135deg, ") + c["purple-mid"]
        + QStringLiteral(", ") + c["purple-light"]
        + QStringLiteral("); -webkit-background-clip: text;"
                         " -webkit-text-fill-color: transparent; margin-bottom: 4px; }\n");
    html += QStringLiteral(".page-subtitle { color: ") + c["text-muted"]
        + QStringLiteral("; font-size: .85em; }\n");

    // Download items with glass
    html += QStringLiteral(".dl-item { display: flex; align-items: center; gap: 16px;"
                           " padding: 16px 20px; background: ") + c["bg-mid"]
        + QStringLiteral("; border: 1px solid ") + c["border"]
        + QStringLiteral("; border-radius: 14px; margin-bottom: 10px;"
                         " transition: all .25s; backdrop-filter: blur(8px); }\n");
    html += QStringLiteral(".dl-item:hover { border-color: ") + c["purple-mid"]
        + QStringLiteral("; transform: translateX(4px);"
                         " box-shadow: 0 4px 20px rgba(110,106,179,0.1); }\n");

    html += QStringLiteral(".dl-icon-wrap { width: 44px; height: 44px; border-radius: 12px;"
                           " background: rgba(110,106,179,0.1); display: flex;"
                           " align-items: center; justify-content: center; flex-shrink: 0; }\n");
    html += QStringLiteral(".dl-icon { font-size: 1.4em; }\n");
    html += QStringLiteral(".dl-info { flex: 1; overflow: hidden; }\n");
    html += QStringLiteral(".dl-name { font-size: .9em; font-weight: 600; color: ")
        + c["text-primary"]
        + QStringLiteral("; margin-bottom: 4px; white-space: nowrap;"
                         " overflow: hidden; text-overflow: ellipsis; }\n");
    html += QStringLiteral(".dl-meta { font-size: .75em; color: ") + c["text-muted"]
        + QStringLiteral("; display: flex; align-items: center; gap: 6px; }\n");

    // Status badges
    html += QStringLiteral(".dl-badge { font-size: .72em; padding: 2px 8px; border-radius: 6px;"
                           " background: rgba(110,106,179,0.12); color: ") + c["purple-light"]
        + QStringLiteral("; font-weight: 600; }\n");
    html += QStringLiteral(".dl-complete { background: rgba(80,200,120,0.15);"
                           " color: #50c878; }\n");
    html += QStringLiteral(".dl-active { background: rgba(110,106,179,0.18); color: ")
        + c["purple-glow"]
        + QStringLiteral("; animation: pulse 1.5s ease infinite; }\n");

    // Empty state
    html += QStringLiteral(".empty-state { text-align: center; padding: 60px 20px;"
                           " animation: fadeIn .5s ease; }\n");
    html += QStringLiteral(".empty-icon { font-size: 3.5em; margin-bottom: 16px;"
                           " opacity: 0.35; animation: floatUp 3s ease infinite; }\n");
    html += QStringLiteral(".empty-text { font-size: 1.1em; color: ") + c["text-primary"]
        + QStringLiteral("; margin-bottom: 8px; }\n");
    html += QStringLiteral(".empty-hint { color: ") + c["text-muted"]
        + QStringLiteral("; font-size: .85em; }\n");

    html += QStringLiteral("</style></head><body>\n<div class='container'>\n");
    html += QStringLiteral("<div class='header'>\n"
                           "  <div class='page-title'>&#8615; Downloads</div>\n"
                           "  <div class='page-subtitle'>")
        + QString::number(downloads.size())
        + QStringLiteral(" files</div>\n</div>\n");

    if (downloads.isEmpty()) {
        html += QStringLiteral("<div class='empty-state'>"
                               "<div class='empty-icon'>&#8615;</div>"
                               "<div class='empty-text'>No downloads yet</div>"
                               "<div class='empty-hint'>Files you download will appear here</div>"
                               "</div>\n");
    } else {
        html += items;
    }

    html += QStringLiteral("</div>\n</body></html>");
    return html;
}

// ── Profiles Page ──────────────────────────────────────────────────
QString profilesPage(const QJsonArray &profiles, const QString &activeProfile,
                     const QMap<QString, QString> &c) {
    // Color palette for profile avatars
    static const char *avatarColors[] = {
        "#6e6ab3", "#7b4fbf", "#5b8dd9", "#50c878",
        "#e0a050", "#d06060", "#c070c0", "#6bc0b0"
    };

    QString cards;
    int i = 0;
    for (const auto &val : profiles) {
        auto p = val.toObject();
        QString name = p[QStringLiteral("name")].toString();
        bool active = (name == activeProfile);
        QString delay = QString::number(0.06 + i * 0.05, 'f', 2);
        QString color = QString::fromLatin1(avatarColors[i % 8]);
        QString initial = name.isEmpty() ? QStringLiteral("?") : name.left(1).toUpper();

        cards += QStringLiteral(
            "<div class='profile-card%1' style='animation:slideUp .35s %2s ease both'>"
            "<div class='profile-avatar' style='background:%7'>%8</div>"
            "<div class='profile-name'>%3</div>"
            "%4"
            "<div class='profile-action'>"
            "<a class='btn%5' href='oyn://switch-profile?name=%3'>%6</a>"
            "</div></div>")
            .arg(active ? QStringLiteral(" active") : QString(),
                 delay, name.toHtmlEscaped(),
                 active ? QStringLiteral("<div class='profile-status'>Active</div>") : QString(),
                 active ? QString() : QStringLiteral(" btn-accent"),
                 active ? QStringLiteral("Current") : QStringLiteral("Switch"),
                 color, initial);
        i++;
    }

    // Build via concatenation
    QString html;
    html += QStringLiteral("<!DOCTYPE html><html><head><meta charset='utf-8'>"
                           "<title>Profiles</title><style>\n");
    html += sharedCss(c);

    // Page header
    html += QStringLiteral("\n.page-title { font-size: 1.7em; font-weight: 700;"
                           " background: linear-gradient(135deg, ") + c["purple-mid"]
        + QStringLiteral(", ") + c["purple-light"]
        + QStringLiteral("); -webkit-background-clip: text;"
                         " -webkit-text-fill-color: transparent; margin-bottom: 4px; }\n");
    html += QStringLiteral(".page-subtitle { color: ") + c["text-muted"]
        + QStringLiteral("; font-size: .85em; }\n");

    // Profile grid with glass cards
    html += QStringLiteral(".profile-grid { display: grid;"
                           " grid-template-columns: repeat(auto-fill, minmax(200px, 1fr));"
                           " gap: 18px; margin-bottom: 28px; }\n");
    html += QStringLiteral(".profile-card { background: ") + c["bg-mid"]
        + QStringLiteral("; border: 1px solid ") + c["border"]
        + QStringLiteral("; border-radius: 18px; padding: 30px 22px; text-align: center;"
                         " transition: all .3s ease; backdrop-filter: blur(8px);"
                         " position: relative; overflow: hidden; }\n");
    html += QStringLiteral(".profile-card::before { content: ''; position: absolute;"
                           " top: 0; left: 0; right: 0; height: 4px;"
                           " background: linear-gradient(90deg, transparent, ")
        + c["purple-mid"] + QStringLiteral(", transparent);"
                          " opacity: 0; transition: opacity .3s; }\n");
    html += QStringLiteral(".profile-card:hover { border-color: ") + c["purple-mid"]
        + QStringLiteral("; transform: translateY(-4px);"
                         " box-shadow: 0 12px 36px rgba(110,106,179,0.15); }\n");
    html += QStringLiteral(".profile-card:hover::before { opacity: 1; }\n");
    html += QStringLiteral(".profile-card.active { border-color: ") + c["purple-mid"]
        + QStringLiteral("; }\n");
    html += QStringLiteral(".profile-card.active::before { opacity: 1;"
                           " animation: glowBorder 2s ease infinite; }\n");

    // Avatar with initial and color
    html += QStringLiteral(".profile-avatar { width: 56px; height: 56px; border-radius: 50%;"
                           " margin: 0 auto 14px; display: flex; align-items: center;"
                           " justify-content: center; font-size: 1.4em; font-weight: 700;"
                           " color: white; transition: transform .3s; }\n");
    html += QStringLiteral(".profile-card:hover .profile-avatar { transform: scale(1.08); }\n");

    html += QStringLiteral(".profile-name { font-weight: 700; font-size: 1.05em; color: ")
        + c["text-primary"] + QStringLiteral("; margin-bottom: 6px; }\n");
    html += QStringLiteral(".profile-status { font-size: .75em; color: ") + c["purple-mid"]
        + QStringLiteral("; font-weight: 600; margin-bottom: 10px;"
                         " display: flex; align-items: center; justify-content: center; gap: 5px; }\n");
    html += QStringLiteral(".profile-status::before { content: ''; width: 6px; height: 6px;"
                           " border-radius: 50%; background: #50c878;"
                           " animation: pulse 2s ease infinite; }\n");
    html += QStringLiteral(".profile-action { margin-top: 14px; }\n");

    // New profile button area
    html += QStringLiteral(".new-profile { text-align: center; margin-top: 12px;"
                           " animation: slideUp .4s .2s ease both; }\n");

    html += QStringLiteral("</style></head><body>\n<div class='container'>\n");
    html += QStringLiteral("<div class='header'>\n"
                           "  <div class='page-title'>&#128100; Profiles</div>\n"
                           "  <div class='page-subtitle'>Manage browser profiles</div>\n"
                           "</div>\n");
    html += QStringLiteral("<div class='profile-grid'>") + cards
        + QStringLiteral("</div>\n");
    html += QStringLiteral("<div class='new-profile'>\n"
                           "  <a class='btn btn-accent' href='oyn://new-profile'>+ New Profile</a>\n"
                           "</div>\n");
    html += QStringLiteral("</div>\n</body></html>");
    return html;
}

}  // namespace InternalPages
