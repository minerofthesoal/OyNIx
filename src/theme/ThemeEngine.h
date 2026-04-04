#pragma once

#include <QObject>
#include <QMap>
#include <QStringList>
#include <QCoreApplication>

class ThemeEngine : public QObject {
    Q_OBJECT

public:
    static ThemeEngine &instance();

    bool loadTheme(const QString &name);
    QString currentTheme() const { return m_currentTheme; }
    QStringList availableThemes() const;
    QString getColor(const QString &key) const;
    QString getQtStylesheet() const { return m_qtStylesheet; }
    const QMap<QString, QString> &colors() const { return m_colors; }

signals:
    void themeChanged(const QString &name);

private:
    explicit ThemeEngine(QObject *parent = nullptr);
    void loadDefaultColors();
    void parseVariables(const QString &css);
    QString findThemeFile(const QString &name) const;
    QString generateQtStylesheet() const;

    QString m_currentTheme;
    QMap<QString, QString> m_colors;
    QString m_qtStylesheet;
};
