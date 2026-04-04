#pragma once

#include <QWidget>
#include <QList>
#include <QString>

class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QPropertyAnimation;

/**
 * CommandPalette — VS Code-style Ctrl+K command launcher.
 * Fuzzy-matches commands, tabs, bookmarks, and history.
 */
class CommandPalette : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(qreal fadeOpacity READ fadeOpacity WRITE setFadeOpacity)

public:
    struct Command {
        QString id;
        QString name;
        QString description;
        QString shortcut;
        QString category;  // "Navigation", "Tools", "AI", "Tab", etc.
        std::function<void()> action;
    };

    explicit CommandPalette(QWidget *parent = nullptr);
    ~CommandPalette() override;

    void registerCommand(const Command &cmd);
    void removeCommand(const QString &id);
    void clearCommands();

    void show();
    void hide();
    void toggle();

    qreal fadeOpacity() const { return m_opacity; }
    void setFadeOpacity(qreal o);

signals:
    void commandExecuted(const QString &commandId);

protected:
    bool eventFilter(QObject *obj, QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void setupUi();
    void setupStyles();
    void filterCommands(const QString &query);
    void executeSelected();
    int fuzzyScore(const QString &query, const Command &cmd) const;

    QLineEdit   *m_searchInput = nullptr;
    QListWidget *m_resultsList = nullptr;
    QPropertyAnimation *m_fadeAnim = nullptr;

    QList<Command> m_commands;
    QList<int>     m_filteredIndices;
    qreal          m_opacity = 0.0;
};
