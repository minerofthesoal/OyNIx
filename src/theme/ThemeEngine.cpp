#include "ThemeEngine.h"
#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QDebug>

ThemeEngine &ThemeEngine::instance() {
    static ThemeEngine inst;
    return inst;
}

ThemeEngine::ThemeEngine(QObject *parent) : QObject(parent) {
    loadDefaultColors();
    loadTheme("nyx");
}

void ThemeEngine::loadDefaultColors() {
    // Obsidian — professional dark theme inspired by mature IDEs/browsers
    m_colors = {
        {"bg-darkest",    "#1a1b26"},   {"bg-dark",       "#1f2133"},
        {"bg-mid",        "#24263a"},   {"bg-light",      "#2a2d42"},
        {"bg-lighter",    "#333650"},   {"bg-surface",    "#3b3e56"},
        {"purple-dark",   "#3d3a6b"},   {"purple-mid",    "#6e6ab3"},
        {"purple-light",  "#8884c7"},   {"purple-glow",   "#a09cd8"},
        {"purple-soft",   "#b4b1e0"},   {"purple-pale",   "#d4d2ee"},
        {"text-primary",  "#c8cad8"},   {"text-secondary","#8b8fa5"},
        {"text-muted",    "#565b7e"},   {"text-accent",   "#9f9bdb"},
        {"success",       "#73c991"},   {"warning",       "#e5a84b"},
        {"error",         "#d4565e"},   {"info",          "#6e6ab3"},
        {"border",        "#383b52"},
        {"border-active", "#565b7e"},   {"selection",     "rgba(110,106,179,0.2)"},
        {"scrollbar",     "#383b52"},   {"scrollbar-hover","#565b7e"},
    };
}

bool ThemeEngine::loadTheme(const QString &name) {
    // Start from defaults
    loadDefaultColors();

    // Try to load variant CSS (parse --var: value; lines)
    QString cssPath = findThemeFile(name);
    if (!cssPath.isEmpty()) {
        QFile file(cssPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            QString css = file.readAll();
            parseVariables(css);
        }
    }

    m_currentTheme = name;
    m_qtStylesheet = generateQtStylesheet();
    emit themeChanged(name);
    return true;
}

QString ThemeEngine::findThemeFile(const QString &name) const {
    // Try Qt resource first
    QString resPath = QStringLiteral(":/themes/%1.css").arg(name);
    if (QFile::exists(resPath)) return resPath;

    // Try local themes directory
    QString appDir = QCoreApplication::applicationDirPath();
    QStringList searchPaths = {
        appDir + "/themes/" + name + ".css",
        appDir + "/../themes/" + name + ".css",
        appDir + "/../share/oynix/themes/" + name + ".css",
        "/usr/share/oynix/themes/" + name + ".css",
    };
    for (const auto &p : searchPaths) {
        if (QFile::exists(p)) return p;
    }
    return {};
}

void ThemeEngine::parseVariables(const QString &css) {
    static QRegularExpression re(R"(--([\w-]+)\s*:\s*([^;]+);)");
    auto it = re.globalMatch(css);
    while (it.hasNext()) {
        auto match = it.next();
        m_colors[match.captured(1)] = match.captured(2).trimmed();
    }
}

QString ThemeEngine::getColor(const QString &key) const {
    return m_colors.value(key, "#ff00ff");  // magenta = missing color (visible debug)
}

QStringList ThemeEngine::availableThemes() const {
    return {"obsidian", "midnight", "slate", "nord", "ember"};
}

QString ThemeEngine::generateQtStylesheet() const {
    const auto &c = m_colors;
    auto g = [&](const QString &k) { return c.value(k); };

    return QStringLiteral(R"(
QMainWindow { background-color: %1; color: %2; }
QWidget { background-color: %3; color: %2;
  font-family: 'Segoe UI', 'Ubuntu', 'Cantarell', sans-serif; font-size: 13px; }

QMenuBar { background: %1; color: %2; border-bottom: 1px solid %4; padding: 2px; }
QMenuBar::item { padding: 6px 14px; border-radius: 6px; }
QMenuBar::item:selected { background: %5; color: %6; }
QMenu { background: %7; color: %2; border: 1px solid %4; border-radius: 10px; padding: 6px; }
QMenu::item { padding: 8px 30px 8px 20px; border-radius: 6px; margin: 1px 2px; }
QMenu::item:selected { background: %5; color: %6; }
QMenu::separator { height: 1px; background: %4; margin: 4px 12px; }

QToolBar { background: %1; border: none; padding: 4px 8px; spacing: 2px; }
QToolBar::separator { width: 1px; background: %4; margin: 4px 6px; }

QLineEdit { background: %7; color: %2; border: 2px solid %4; border-radius: 22px;
  padding: 9px 18px; font-size: 14px; selection-background-color: %8; }
QLineEdit:focus { border-color: %9; background: %10; }

QPushButton { background: %11; color: %2; border: 1px solid %4; border-radius: 10px;
  padding: 8px 14px; font-weight: 600; min-width: 28px; }
QPushButton:hover { background: %5; border-color: %9; color: %6; }
QPushButton:pressed { background: %9; color: %1; }
QPushButton#navBtn { background: transparent; border: none; border-radius: 8px;
  padding: 6px; min-width: 34px; min-height: 34px; }
QPushButton#navBtn:hover { background: %11; }
QPushButton#accentBtn { background: %9; color: %1; border: none; font-weight: bold; }
QPushButton#accentBtn:hover { background: %12; }

QTabWidget::pane { border: none; background: %1; }
QTabBar { background: %1; }
QTabBar::tab { background: %7; color: %13; padding: 8px 20px; margin-right: 1px;
  border-top-left-radius: 10px; border-top-right-radius: 10px;
  border: 1px solid %4; border-bottom: none; min-width: 100px; max-width: 220px; }
QTabBar::tab:selected { background: %10; color: %12; border-color: %9; }
QTabBar::tab:hover:!selected { background: %11; color: %2; }
QTabBar::close-button { subcontrol-position: right; border-radius: 4px; padding: 2px; }
QTabBar::close-button:hover { background: %14; }

QScrollBar:vertical { background: transparent; width: 8px; }
QScrollBar::handle:vertical { background: %15; border-radius: 4px; min-height: 30px; }
QScrollBar::handle:vertical:hover { background: %16; }
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
QScrollBar:horizontal { background: transparent; height: 8px; }
QScrollBar::handle:horizontal { background: %15; border-radius: 4px; min-width: 30px; }
QScrollBar::handle:horizontal:hover { background: %16; }
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }

QStatusBar { background: %1; color: %17; border-top: 1px solid %4; font-size: 11px; padding: 2px 8px; }
QStatusBar::item { border: none; }

QDialog { background: %3; color: %2; border-radius: 14px; }
QGroupBox { font-weight: bold; border: 1px solid %4; border-radius: 10px;
  margin-top: 14px; padding-top: 18px; color: %12; }

QListWidget { background: %3; color: %2; border: 1px solid %4; border-radius: 8px; outline: none; }
QListWidget::item { padding: 8px; border-radius: 6px; }
QListWidget::item:selected { background: %5; color: %6; }
QListWidget::item:hover:!selected { background: %11; }

QProgressBar { background: %11; border: none; border-radius: 4px; height: 6px; text-align: center; }
QProgressBar::chunk { background: %9; border-radius: 4px; }

QSplitter::handle { background: %4; width: 2px; }
QTextEdit { background: %7; color: %2; border: 1px solid %4; border-radius: 8px; padding: 8px; }
QLabel { background: transparent; }
QToolTip { background: %7; color: %2; border: 1px solid %4; border-radius: 6px; padding: 6px 10px; }
)")
        .arg(g("bg-darkest"))           // %1
        .arg(g("text-primary"))         // %2
        .arg(g("bg-dark"))             // %3
        .arg(g("border"))             // %4
        .arg(g("purple-dark"))         // %5
        .arg(g("purple-pale"))         // %6
        .arg(g("bg-mid"))             // %7
        .arg(g("selection"))           // %8
        .arg(g("purple-mid"))          // %9
        .arg(g("bg-light"))           // %10
        .arg(g("bg-lighter"))          // %11
        .arg(g("purple-light"))        // %12
        .arg(g("text-secondary"))      // %13
        .arg(g("error"))              // %14
        .arg(g("scrollbar"))           // %15
        .arg(g("scrollbar-hover"))     // %16
        .arg(g("text-muted"));         // %17
}
