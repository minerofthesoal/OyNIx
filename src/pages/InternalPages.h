#pragma once

#include <QString>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>

namespace InternalPages {

/// Generate homepage HTML with theme colors
QString homePage(const QMap<QString, QString> &colors);

/// Generate search results page
QString searchPage(const QString &query,
                   const QJsonArray &localResults,
                   const QJsonArray &webResults,
                   const QMap<QString, QString> &colors);

/// Generate bookmarks page
QString bookmarksPage(const QJsonArray &bookmarks,
                      const QMap<QString, QString> &colors);

/// Generate history page
QString historyPage(const QJsonArray &history,
                    const QMap<QString, QString> &colors);

/// Generate settings page
QString settingsPage(const QJsonObject &config,
                     const QMap<QString, QString> &colors);

/// Generate downloads page
QString downloadsPage(const QJsonArray &downloads,
                      const QMap<QString, QString> &colors);

/// Generate profiles page
QString profilesPage(const QJsonArray &profiles,
                     const QString &activeProfile,
                     const QMap<QString, QString> &colors);

/// CSS for Nyx glassmorphism + animations (shared by all pages)
QString sharedCss(const QMap<QString, QString> &c);

/// JavaScript for mouse-tracking refraction effect
QString refractionJs();

}  // namespace InternalPages
