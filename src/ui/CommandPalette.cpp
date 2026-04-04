#include "CommandPalette.h"

#include <QLineEdit>
#include <QListWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QKeyEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QApplication>
#include <QScreen>
#include <algorithm>

// ── Constructor / Destructor ─────────────────────────────────────────
CommandPalette::CommandPalette(QWidget *parent)
    : QWidget(parent)
{
    setWindowFlags(Qt::FramelessWindowHint | Qt::Popup);
    setAttribute(Qt::WA_TranslucentBackground);
    setupUi();
    setupStyles();

    m_fadeAnim = new QPropertyAnimation(this, "fadeOpacity", this);
    m_fadeAnim->setDuration(150);
    m_fadeAnim->setEasingCurve(QEasingCurve::OutCubic);
}

CommandPalette::~CommandPalette() = default;

// ── UI Setup ─────────────────────────────────────────────────────────
void CommandPalette::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 12, 16, 12);
    layout->setSpacing(8);

    m_searchInput = new QLineEdit(this);
    m_searchInput->setPlaceholderText(QStringLiteral("Type a command..."));
    m_searchInput->setObjectName(QStringLiteral("cpInput"));
    m_searchInput->installEventFilter(this);
    layout->addWidget(m_searchInput);

    m_resultsList = new QListWidget(this);
    m_resultsList->setObjectName(QStringLiteral("cpResults"));
    m_resultsList->setMaximumHeight(320);
    m_resultsList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    layout->addWidget(m_resultsList);

    connect(m_searchInput, &QLineEdit::textChanged,
            this, &CommandPalette::filterCommands);
    connect(m_resultsList, &QListWidget::itemActivated,
            this, [this](QListWidgetItem *) { executeSelected(); });
}

void CommandPalette::setupStyles()
{
    setStyleSheet(QStringLiteral(
        "#cpInput { background: #12121e; color: #E8E0F0; border: 1px solid #7B4FBF;"
        "  border-radius: 8px; padding: 10px 14px; font-size: 14px; }"
        "#cpInput:focus { border-color: #9B6FDF; }"
        "#cpResults { background: #0e0e16; border: 1px solid #2a2a40;"
        "  border-radius: 8px; color: #E8E0F0; font-size: 13px;"
        "  outline: none; }"
        "#cpResults::item { padding: 8px 12px; border-radius: 4px; }"
        "#cpResults::item:selected { background: rgba(123,79,191,0.4); }"
        "#cpResults::item:hover { background: rgba(123,79,191,0.2); }"
        "QScrollBar:vertical { background: transparent; width: 6px; }"
        "QScrollBar::handle:vertical { background: #7B4FBF; border-radius: 3px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }"
    ));
}

// ── Command registration ─────────────────────────────────────────────
void CommandPalette::registerCommand(const Command &cmd)
{
    m_commands.append(cmd);
}

void CommandPalette::removeCommand(const QString &id)
{
    m_commands.erase(
        std::remove_if(m_commands.begin(), m_commands.end(),
                        [&id](const Command &c) { return c.id == id; }),
        m_commands.end());
}

void CommandPalette::clearCommands()
{
    m_commands.clear();
}

// ── Show / Hide ──────────────────────────────────────────────────────
void CommandPalette::show()
{
    // Position at top-center of parent
    if (parentWidget()) {
        const int w = qMin(500, parentWidget()->width() - 40);
        const int x = (parentWidget()->width() - w) / 2;
        setGeometry(x, 60, w, 400);
    } else {
        resize(500, 400);
    }

    m_searchInput->clear();
    filterCommands(QString());
    QWidget::show();
    m_searchInput->setFocus();

    m_fadeAnim->stop();
    m_fadeAnim->setStartValue(0.0);
    m_fadeAnim->setEndValue(1.0);
    m_fadeAnim->start();
}

void CommandPalette::hide()
{
    m_fadeAnim->stop();
    m_fadeAnim->setStartValue(m_opacity);
    m_fadeAnim->setEndValue(0.0);
    connect(m_fadeAnim, &QPropertyAnimation::finished, this, [this]() {
        QWidget::hide();
        disconnect(m_fadeAnim, &QPropertyAnimation::finished, nullptr, nullptr);
    });
    m_fadeAnim->start();
}

void CommandPalette::toggle()
{
    if (isVisible()) hide();
    else             show();
}

void CommandPalette::setFadeOpacity(qreal o)
{
    m_opacity = o;
    update();
}

// ── Paint with transparency ──────────────────────────────────────────
void CommandPalette::paintEvent(QPaintEvent * /*event*/)
{
    QPainter p(this);
    p.setOpacity(m_opacity);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(QColor(10, 10, 18, 240));
    p.setPen(QColor(123, 79, 191));
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 12, 12);
}

// ── Fuzzy matching ───────────────────────────────────────────────────
int CommandPalette::fuzzyScore(const QString &query, const Command &cmd) const
{
    if (query.isEmpty()) return 100;

    int score = 0;
    const QString q = query.toLower();
    const QString name = cmd.name.toLower();
    const QString desc = cmd.description.toLower();
    const QString cat  = cmd.category.toLower();

    // Exact prefix match on name
    if (name.startsWith(q)) score += 100;
    // Contains in name
    else if (name.contains(q)) score += 60;
    // Contains in description
    if (desc.contains(q)) score += 30;
    // Contains in category
    if (cat.contains(q)) score += 20;
    // Character-by-character fuzzy
    if (score == 0) {
        int qi = 0;
        for (int ni = 0; ni < name.size() && qi < q.size(); ++ni) {
            if (name[ni] == q[qi]) ++qi;
        }
        if (qi == q.size()) score += 40;
    }

    return score;
}

void CommandPalette::filterCommands(const QString &query)
{
    m_resultsList->clear();
    m_filteredIndices.clear();

    struct Scored { int index; int score; };
    QList<Scored> scored;
    for (int i = 0; i < m_commands.size(); ++i) {
        const int s = fuzzyScore(query, m_commands[i]);
        if (s > 0) scored.append({i, s});
    }

    std::sort(scored.begin(), scored.end(),
              [](const Scored &a, const Scored &b) { return a.score > b.score; });

    for (const auto &s : scored) {
        const auto &cmd = m_commands[s.index];
        QString display = cmd.name;
        if (!cmd.shortcut.isEmpty())
            display += QStringLiteral("  [%1]").arg(cmd.shortcut);
        if (!cmd.category.isEmpty())
            display = QStringLiteral("[%1] %2").arg(cmd.category, display);

        auto *item = new QListWidgetItem(display, m_resultsList);
        item->setToolTip(cmd.description);
        m_filteredIndices.append(s.index);
    }

    if (m_resultsList->count() > 0)
        m_resultsList->setCurrentRow(0);
}

void CommandPalette::executeSelected()
{
    const int row = m_resultsList->currentRow();
    if (row < 0 || row >= m_filteredIndices.size()) return;

    const int cmdIndex = m_filteredIndices[row];
    if (cmdIndex >= 0 && cmdIndex < m_commands.size()) {
        const auto &cmd = m_commands[cmdIndex];
        if (cmd.action) cmd.action();
        emit commandExecuted(cmd.id);
    }
    hide();
}

// ── Event filter (keyboard nav) ──────────────────────────────────────
bool CommandPalette::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_searchInput && event->type() == QEvent::KeyPress) {
        auto *ke = static_cast<QKeyEvent *>(event);
        switch (ke->key()) {
        case Qt::Key_Escape:
            hide();
            return true;
        case Qt::Key_Return:
        case Qt::Key_Enter:
            executeSelected();
            return true;
        case Qt::Key_Down:
            if (m_resultsList->currentRow() < m_resultsList->count() - 1)
                m_resultsList->setCurrentRow(m_resultsList->currentRow() + 1);
            return true;
        case Qt::Key_Up:
            if (m_resultsList->currentRow() > 0)
                m_resultsList->setCurrentRow(m_resultsList->currentRow() - 1);
            return true;
        case Qt::Key_Tab:
            // Autocomplete from first result
            if (m_resultsList->count() > 0 && !m_filteredIndices.isEmpty()) {
                m_searchInput->setText(m_commands[m_filteredIndices[0]].name);
            }
            return true;
        default:
            break;
        }
    }
    return QWidget::eventFilter(obj, event);
}
