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

    // Use string concatenation to avoid Qt's 9-arg limit and produce
    // a richer, more polished stylesheet.
    QString ss;

    // ── Global ──
    ss += QStringLiteral("QMainWindow { background-color: ") + g("bg-darkest")
       + QStringLiteral("; color: ") + g("text-primary") + QStringLiteral("; }\n");
    ss += QStringLiteral("QWidget { background-color: ") + g("bg-dark")
       + QStringLiteral("; color: ") + g("text-primary")
       + QStringLiteral("; font-family: 'Inter', 'Segoe UI', 'Ubuntu', sans-serif;"
                        " font-size: 13px; }\n");

    // ── MenuBar ──
    ss += QStringLiteral("QMenuBar { background: ") + g("bg-darkest")
       + QStringLiteral("; color: ") + g("text-primary")
       + QStringLiteral("; border-bottom: 1px solid ") + g("border")
       + QStringLiteral("; padding: 2px 4px; }\n");
    ss += QStringLiteral("QMenuBar::item { padding: 6px 14px; border-radius: 6px;"
                         " margin: 1px; }\n");
    ss += QStringLiteral("QMenuBar::item:selected { background: ") + g("purple-dark")
       + QStringLiteral("; color: ") + g("purple-pale") + QStringLiteral("; }\n");

    // ── Menu ──
    ss += QStringLiteral("QMenu { background: ") + g("bg-mid")
       + QStringLiteral("; color: ") + g("text-primary")
       + QStringLiteral("; border: 1px solid ") + g("border")
       + QStringLiteral("; border-radius: 10px; padding: 6px 4px; }\n");
    ss += QStringLiteral("QMenu::item { padding: 8px 32px 8px 20px;"
                         " border-radius: 6px; margin: 1px 2px; }\n");
    ss += QStringLiteral("QMenu::item:selected { background: ") + g("purple-dark")
       + QStringLiteral("; color: ") + g("purple-pale") + QStringLiteral("; }\n");
    ss += QStringLiteral("QMenu::item:disabled { color: ") + g("text-muted")
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QMenu::separator { height: 1px; background: ") + g("border")
       + QStringLiteral("; margin: 4px 12px; }\n");
    ss += QStringLiteral("QMenu::icon { padding-left: 8px; }\n");

    // ── ToolBar ──
    ss += QStringLiteral("QToolBar { background: ") + g("bg-darkest")
       + QStringLiteral("; border: none; padding: 4px 8px; spacing: 3px; }\n");
    ss += QStringLiteral("QToolBar::separator { width: 1px; background: ") + g("border")
       + QStringLiteral("; margin: 4px 6px; }\n");

    // ── LineEdit / URL bar ──
    ss += QStringLiteral("QLineEdit { background: ") + g("bg-mid")
       + QStringLiteral("; color: ") + g("text-primary")
       + QStringLiteral("; border: 2px solid ") + g("border")
       + QStringLiteral("; border-radius: 22px; padding: 9px 18px;"
                        " font-size: 14px; selection-background-color: ") + g("selection")
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QLineEdit:focus { border-color: ") + g("purple-mid")
       + QStringLiteral("; background: ") + g("bg-light") + QStringLiteral("; }\n");
    ss += QStringLiteral("QLineEdit:hover:!focus { border-color: ") + g("border-active")
       + QStringLiteral("; }\n");

    // ── Buttons ──
    ss += QStringLiteral("QPushButton { background: ") + g("bg-lighter")
       + QStringLiteral("; color: ") + g("text-primary")
       + QStringLiteral("; border: 1px solid ") + g("border")
       + QStringLiteral("; border-radius: 10px; padding: 8px 16px;"
                        " font-weight: 600; min-width: 28px; }\n");
    ss += QStringLiteral("QPushButton:hover { background: ") + g("purple-dark")
       + QStringLiteral("; border-color: ") + g("purple-mid")
       + QStringLiteral("; color: ") + g("purple-pale") + QStringLiteral("; }\n");
    ss += QStringLiteral("QPushButton:pressed { background: ") + g("purple-mid")
       + QStringLiteral("; color: ") + g("bg-darkest") + QStringLiteral("; }\n");
    ss += QStringLiteral("QPushButton:disabled { background: ") + g("bg-dark")
       + QStringLiteral("; color: ") + g("text-muted")
       + QStringLiteral("; border-color: ") + g("border") + QStringLiteral("; }\n");

    // Nav buttons
    ss += QStringLiteral("QPushButton#navBtn { background: transparent; border: none;"
                         " border-radius: 8px; padding: 6px;"
                         " min-width: 34px; min-height: 34px; }\n");
    ss += QStringLiteral("QPushButton#navBtn:hover { background: ") + g("bg-lighter")
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QPushButton#navBtn:pressed { background: ") + g("purple-dark")
       + QStringLiteral("; }\n");

    // Accent buttons
    ss += QStringLiteral("QPushButton#accentBtn { background: ") + g("purple-mid")
       + QStringLiteral("; color: ") + g("bg-darkest")
       + QStringLiteral("; border: none; font-weight: bold; }\n");
    ss += QStringLiteral("QPushButton#accentBtn:hover { background: ") + g("purple-light")
       + QStringLiteral("; }\n");

    // ── Tabs ──
    ss += QStringLiteral("QTabWidget::pane { border: none; background: ") + g("bg-darkest")
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QTabBar { background: ") + g("bg-darkest") + QStringLiteral("; }\n");
    ss += QStringLiteral("QTabBar::tab { background: ") + g("bg-mid")
       + QStringLiteral("; color: ") + g("text-secondary")
       + QStringLiteral("; padding: 8px 20px; margin-right: 1px;"
                        " border-top-left-radius: 10px; border-top-right-radius: 10px;"
                        " border: 1px solid ") + g("border")
       + QStringLiteral("; border-bottom: none;"
                        " min-width: 100px; max-width: 220px; }\n");
    ss += QStringLiteral("QTabBar::tab:selected { background: ") + g("bg-light")
       + QStringLiteral("; color: ") + g("purple-light")
       + QStringLiteral("; border-color: ") + g("purple-mid") + QStringLiteral("; }\n");
    ss += QStringLiteral("QTabBar::tab:hover:!selected { background: ") + g("bg-lighter")
       + QStringLiteral("; color: ") + g("text-primary") + QStringLiteral("; }\n");
    ss += QStringLiteral("QTabBar::close-button { subcontrol-position: right;"
                         " border-radius: 4px; padding: 2px; }\n");
    ss += QStringLiteral("QTabBar::close-button:hover { background: ") + g("error")
       + QStringLiteral("; }\n");

    // ── ScrollBars ──
    ss += QStringLiteral("QScrollBar:vertical { background: transparent; width: 7px;"
                         " margin: 2px; }\n");
    ss += QStringLiteral("QScrollBar::handle:vertical { background: ") + g("scrollbar")
       + QStringLiteral("; border-radius: 3px; min-height: 30px; }\n");
    ss += QStringLiteral("QScrollBar::handle:vertical:hover { background: ") + g("scrollbar-hover")
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
                         " height: 0; }\n");
    ss += QStringLiteral("QScrollBar:horizontal { background: transparent; height: 7px;"
                         " margin: 2px; }\n");
    ss += QStringLiteral("QScrollBar::handle:horizontal { background: ") + g("scrollbar")
       + QStringLiteral("; border-radius: 3px; min-width: 30px; }\n");
    ss += QStringLiteral("QScrollBar::handle:horizontal:hover { background: ") + g("scrollbar-hover")
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {"
                         " width: 0; }\n");
    ss += QStringLiteral("QScrollBar::add-page, QScrollBar::sub-page { background: none; }\n");

    // ── StatusBar ──
    ss += QStringLiteral("QStatusBar { background: ") + g("bg-darkest")
       + QStringLiteral("; color: ") + g("text-muted")
       + QStringLiteral("; border-top: 1px solid ") + g("border")
       + QStringLiteral("; font-size: 11px; padding: 2px 10px; }\n");
    ss += QStringLiteral("QStatusBar::item { border: none; }\n");

    // ── Dialog / GroupBox ──
    ss += QStringLiteral("QDialog { background: ") + g("bg-dark")
       + QStringLiteral("; color: ") + g("text-primary")
       + QStringLiteral("; border-radius: 14px; }\n");
    ss += QStringLiteral("QGroupBox { font-weight: bold; border: 1px solid ") + g("border")
       + QStringLiteral("; border-radius: 10px; margin-top: 16px;"
                        " padding-top: 20px; color: ") + g("purple-light")
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QGroupBox::title { subcontrol-origin: margin;"
                         " subcontrol-position: top left; padding: 4px 12px;"
                         " color: ") + g("purple-light") + QStringLiteral("; }\n");

    // ── List/Tree widgets ──
    ss += QStringLiteral("QListWidget, QTreeWidget { background: ") + g("bg-dark")
       + QStringLiteral("; color: ") + g("text-primary")
       + QStringLiteral("; border: 1px solid ") + g("border")
       + QStringLiteral("; border-radius: 8px; outline: none; }\n");
    ss += QStringLiteral("QListWidget::item, QTreeWidget::item {"
                         " padding: 8px; border-radius: 6px; }\n");
    ss += QStringLiteral("QListWidget::item:selected, QTreeWidget::item:selected {"
                         " background: ") + g("purple-dark")
       + QStringLiteral("; color: ") + g("purple-pale") + QStringLiteral("; }\n");
    ss += QStringLiteral("QListWidget::item:hover:!selected, QTreeWidget::item:hover:!selected {"
                         " background: ") + g("bg-lighter") + QStringLiteral("; }\n");

    // ── ProgressBar ──
    ss += QStringLiteral("QProgressBar { background: ") + g("bg-lighter")
       + QStringLiteral("; border: none; border-radius: 4px; height: 6px;"
                        " text-align: center; color: transparent; }\n");
    ss += QStringLiteral("QProgressBar::chunk { background: qlineargradient("
                         "x1:0, y1:0, x2:1, y2:0, stop:0 ") + g("purple-mid")
       + QStringLiteral(", stop:1 ") + g("purple-light")
       + QStringLiteral("); border-radius: 4px; }\n");

    // ── Splitter / TextEdit / Label / Tooltip ──
    ss += QStringLiteral("QSplitter::handle { background: ") + g("border")
       + QStringLiteral("; width: 2px; }\n");
    ss += QStringLiteral("QSplitter::handle:hover { background: ") + g("purple-mid")
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QTextEdit { background: ") + g("bg-mid")
       + QStringLiteral("; color: ") + g("text-primary")
       + QStringLiteral("; border: 1px solid ") + g("border")
       + QStringLiteral("; border-radius: 10px; padding: 8px; }\n");
    ss += QStringLiteral("QTextEdit:focus { border-color: ") + g("purple-mid")
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QLabel { background: transparent; }\n");
    ss += QStringLiteral("QToolTip { background: ") + g("bg-mid")
       + QStringLiteral("; color: ") + g("text-primary")
       + QStringLiteral("; border: 1px solid ") + g("border")
       + QStringLiteral("; border-radius: 8px; padding: 6px 12px; }\n");

    // ── ComboBox ──
    ss += QStringLiteral("QComboBox { background: ") + g("bg-mid")
       + QStringLiteral("; color: ") + g("text-primary")
       + QStringLiteral("; border: 1px solid ") + g("border")
       + QStringLiteral("; border-radius: 8px; padding: 6px 12px; }\n");
    ss += QStringLiteral("QComboBox:hover { border-color: ") + g("purple-mid")
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QComboBox::drop-down { border: none; width: 24px; }\n");
    ss += QStringLiteral("QComboBox QAbstractItemView { background: ") + g("bg-mid")
       + QStringLiteral("; color: ") + g("text-primary")
       + QStringLiteral("; border: 1px solid ") + g("border")
       + QStringLiteral("; border-radius: 8px; selection-background-color: ") + g("purple-dark")
       + QStringLiteral("; }\n");

    // ── CheckBox / RadioButton ──
    ss += QStringLiteral("QCheckBox, QRadioButton { color: ") + g("text-primary")
       + QStringLiteral("; spacing: 8px; }\n");
    ss += QStringLiteral("QCheckBox::indicator { width: 18px; height: 18px;"
                         " border-radius: 4px; border: 2px solid ") + g("border")
       + QStringLiteral("; background: ") + g("bg-mid") + QStringLiteral("; }\n");
    ss += QStringLiteral("QCheckBox::indicator:checked { background: ") + g("purple-mid")
       + QStringLiteral("; border-color: ") + g("purple-mid") + QStringLiteral("; }\n");
    ss += QStringLiteral("QCheckBox::indicator:hover { border-color: ") + g("purple-mid")
       + QStringLiteral("; }\n");

    // ── SpinBox ──
    ss += QStringLiteral("QSpinBox, QDoubleSpinBox { background: ") + g("bg-mid")
       + QStringLiteral("; color: ") + g("text-primary")
       + QStringLiteral("; border: 1px solid ") + g("border")
       + QStringLiteral("; border-radius: 8px; padding: 4px 8px; }\n");
    ss += QStringLiteral("QSpinBox:focus, QDoubleSpinBox:focus { border-color: ")
       + g("purple-mid") + QStringLiteral("; }\n");

    // ── Slider ──
    ss += QStringLiteral("QSlider::groove:horizontal { background: ") + g("bg-lighter")
       + QStringLiteral("; height: 4px; border-radius: 2px; }\n");
    ss += QStringLiteral("QSlider::handle:horizontal { background: ") + g("purple-mid")
       + QStringLiteral("; width: 16px; height: 16px; margin: -6px 0;"
                        " border-radius: 8px; }\n");
    ss += QStringLiteral("QSlider::handle:horizontal:hover { background: ") + g("purple-light")
       + QStringLiteral("; }\n");

    return ss;
}
