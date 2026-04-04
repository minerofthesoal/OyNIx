#pragma once

#include <QDialog>
#include <QJsonObject>

class QTabWidget;
class QComboBox;
class QCheckBox;
class QLineEdit;
class QSpinBox;
class QLabel;
class QPushButton;
class QListWidget;

/**
 * SettingsDialog — Full settings UI with multiple tabs:
 * General, Appearance, Search, AI, Privacy, Extensions, Sync, About.
 */
class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(const QJsonObject &config, QWidget *parent = nullptr);
    ~SettingsDialog() override;

    [[nodiscard]] QJsonObject updatedConfig() const;

signals:
    void themeChangeRequested(const QString &theme);
    void searchEngineChanged(const QString &engine);
    void aiConfigChanged();

private:
    void setupUi();
    QWidget *createGeneralTab();
    QWidget *createAppearanceTab();
    QWidget *createSearchTab();
    QWidget *createAiTab();
    QWidget *createPrivacyTab();
    QWidget *createExtensionsTab();
    QWidget *createSyncTab();
    QWidget *createAboutTab();

    void applyStyles();
    void loadFromConfig();
    void saveToConfig();

    QTabWidget *m_tabs = nullptr;
    QJsonObject m_config;

    // General
    QLineEdit *m_homepageEdit = nullptr;
    QComboBox *m_startupCombo = nullptr;
    QCheckBox *m_restoreSessionCheck = nullptr;
    QSpinBox  *m_maxTabsSpin = nullptr;

    // Appearance
    QComboBox *m_themeCombo = nullptr;
    QCheckBox *m_treeTabsCheck = nullptr;
    QCheckBox *m_animationsCheck = nullptr;
    QComboBox *m_fontSizeCombo = nullptr;

    // Search
    QComboBox *m_searchEngineCombo = nullptr;
    QCheckBox *m_searchSuggestionsCheck = nullptr;
    QCheckBox *m_autoIndexCheck = nullptr;

    // AI
    QComboBox *m_aiBackendCombo = nullptr;
    QLineEdit *m_aiEndpointEdit = nullptr;
    QLineEdit *m_aiApiKeyEdit = nullptr;
    QComboBox *m_aiModelCombo = nullptr;
    QPushButton *m_aiRefreshBtn = nullptr;
    QPushButton *m_aiTestBtn = nullptr;
    QLabel    *m_aiStatusLabel = nullptr;

    // Privacy
    QCheckBox *m_securityPromptsCheck = nullptr;
    QCheckBox *m_clearOnExitCheck = nullptr;
    QCheckBox *m_doNotTrackCheck = nullptr;
    QPushButton *m_clearDataBtn = nullptr;

    // Sync
    QLineEdit *m_syncTokenEdit = nullptr;
    QLineEdit *m_syncRepoEdit = nullptr;
    QCheckBox *m_autoSyncCheck = nullptr;
    QSpinBox  *m_syncIntervalSpin = nullptr;
};
