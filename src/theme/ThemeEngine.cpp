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
    m_colors = {
        {"bg-darkest",    "#08080d"},   {"bg-dark",       "#0e0e16"},
        {"bg-mid",        "#16161f"},   {"bg-light",      "#1e1e2a"},
        {"bg-lighter",    "#282836"},   {"bg-surface",    "#303042"},
        {"purple-dark",   "#4a2d7a"},   {"purple-mid",    "#7B4FBF"},
        {"purple-light",  "#9B6FDF"},   {"purple-glow",   "#B088F0"},
        {"purple-soft",   "#C9A8F0"},   {"purple-pale",   "#E0D0F8"},
        {"text-primary",  "#E8E0F0"},   {"text-secondary","#A8A0B8"},
        {"text-muted",    "#605878"},   {"text-accent",   "#C9A8F0"},
        {"success",       "#6FCF97"},   {"warning",       "#F2C94C"},
        {"error",         "#EB5757"},   {"info",          "#7B4FBF"},
        {"border",        "rgba(58,58,74,0.6)"},
        {"border-active", "#7B4FBF"},   {"selection",     "rgba(123,79,191,0.25)"},
        {"scrollbar",     "#3a2560"},   {"scrollbar-hover","#7B4FBF"},
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
    return {"nyx", "nyx-midnight", "nyx-violet", "nyx-amethyst", "nyx-ember"};
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
