#include "SettingsDialog.h"
#include "ai/AiManager.h"
#include "theme/ThemeEngine.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QPushButton>
#include <QListWidget>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QMessageBox>
#include <QScrollArea>

// ── Constructor / Destructor ─────────────────────────────────────────
SettingsDialog::SettingsDialog(const QJsonObject &config, QWidget *parent)
    : QDialog(parent)
    , m_config(config)
{
    setWindowTitle(QStringLiteral("OyNIx Settings"));
    resize(680, 520);
    setupUi();
    applyStyles();
    loadFromConfig();
}

SettingsDialog::~SettingsDialog() = default;

// ── UI Setup ─────────────────────────────────────────────────────────
void SettingsDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(createGeneralTab(),     QStringLiteral("General"));
    m_tabs->addTab(createAppearanceTab(),  QStringLiteral("Appearance"));
    m_tabs->addTab(createSearchTab(),      QStringLiteral("Search"));
    m_tabs->addTab(createAiTab(),          QStringLiteral("AI"));
    m_tabs->addTab(createPrivacyTab(),     QStringLiteral("Privacy"));
    m_tabs->addTab(createExtensionsTab(),  QStringLiteral("Extensions"));
    m_tabs->addTab(createSyncTab(),        QStringLiteral("Sync"));
    m_tabs->addTab(createAboutTab(),       QStringLiteral("About"));
    mainLayout->addWidget(m_tabs);

    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::Apply, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        saveToConfig();
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(buttonBox->button(QDialogButtonBox::Apply), &QPushButton::clicked, this, [this]() {
        saveToConfig();
    });
    mainLayout->addWidget(buttonBox);
}

// ── General tab ──────────────────────────────────────────────────────
QWidget *SettingsDialog::createGeneralTab()
{
    auto *widget = new QWidget(this);
    auto *form = new QFormLayout(widget);
    form->setContentsMargins(16, 16, 16, 16);
    form->setSpacing(12);

    m_homepageEdit = new QLineEdit(this);
    m_homepageEdit->setPlaceholderText(QStringLiteral("oyn://home"));
    form->addRow(QStringLiteral("Homepage:"), m_homepageEdit);

    m_startupCombo = new QComboBox(this);
    m_startupCombo->addItems({
        QStringLiteral("Open homepage"),
        QStringLiteral("Continue where you left off"),
        QStringLiteral("Open new tab page"),
    });
    form->addRow(QStringLiteral("On startup:"), m_startupCombo);

    m_restoreSessionCheck = new QCheckBox(QStringLiteral("Restore previous session"), this);
    form->addRow(QString(), m_restoreSessionCheck);

    m_maxTabsSpin = new QSpinBox(this);
    m_maxTabsSpin->setRange(5, 200);
    m_maxTabsSpin->setValue(50);
    form->addRow(QStringLiteral("Max tabs:"), m_maxTabsSpin);

    return widget;
}

// ── Appearance tab ───────────────────────────────────────────────────
QWidget *SettingsDialog::createAppearanceTab()
{
    auto *widget = new QWidget(this);
    auto *form = new QFormLayout(widget);
    form->setContentsMargins(16, 16, 16, 16);
    form->setSpacing(12);

    m_themeCombo = new QComboBox(this);
    m_themeCombo->addItems({
        QStringLiteral("Nyx Dark"),
        QStringLiteral("Midnight"),
        QStringLiteral("Violet"),
        QStringLiteral("Amethyst"),
        QStringLiteral("Ember"),
        QStringLiteral("Aether Light"),
        QStringLiteral("System"),
    });
    connect(m_themeCombo, &QComboBox::currentTextChanged,
            this, &SettingsDialog::themeChangeRequested);
    form->addRow(QStringLiteral("Theme:"), m_themeCombo);

    m_treeTabsCheck = new QCheckBox(QStringLiteral("Enable tree-style tab sidebar"), this);
    form->addRow(QString(), m_treeTabsCheck);

    m_animationsCheck = new QCheckBox(QStringLiteral("Enable animations"), this);
    m_animationsCheck->setChecked(true);
    form->addRow(QString(), m_animationsCheck);

    m_fontSizeCombo = new QComboBox(this);
    m_fontSizeCombo->addItems({
        QStringLiteral("Small"), QStringLiteral("Medium"),
        QStringLiteral("Large"), QStringLiteral("Extra Large"),
    });
    m_fontSizeCombo->setCurrentIndex(1);
    form->addRow(QStringLiteral("Font size:"), m_fontSizeCombo);

    return widget;
}

// ── Search tab ───────────────────────────────────────────────────────
QWidget *SettingsDialog::createSearchTab()
{
    auto *widget = new QWidget(this);
    auto *form = new QFormLayout(widget);
    form->setContentsMargins(16, 16, 16, 16);
    form->setSpacing(12);

    m_searchEngineCombo = new QComboBox(this);
    m_searchEngineCombo->addItems({
        QStringLiteral("DuckDuckGo"),
        QStringLiteral("Google"),
        QStringLiteral("Bing"),
        QStringLiteral("Brave"),
        QStringLiteral("Startpage"),
    });
    connect(m_searchEngineCombo, &QComboBox::currentTextChanged,
            this, &SettingsDialog::searchEngineChanged);
    form->addRow(QStringLiteral("Default search engine:"), m_searchEngineCombo);

    m_searchSuggestionsCheck = new QCheckBox(QStringLiteral("Show search suggestions"), this);
    m_searchSuggestionsCheck->setChecked(true);
    form->addRow(QString(), m_searchSuggestionsCheck);

    m_autoIndexCheck = new QCheckBox(QStringLiteral("Auto-index visited pages"), this);
    m_autoIndexCheck->setChecked(true);
    form->addRow(QString(), m_autoIndexCheck);

    auto *customBtn = new QPushButton(QStringLiteral("Manage Custom Engines..."), this);
    form->addRow(QString(), customBtn);

    return widget;
}

// ── AI tab ───────────────────────────────────────────────────────────
QWidget *SettingsDialog::createAiTab()
{
    auto *widget = new QWidget(this);
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);

    auto *group = new QGroupBox(QStringLiteral("AI Backend Configuration"), this);
    auto *form = new QFormLayout(group);

    m_aiBackendCombo = new QComboBox(this);
    m_aiBackendCombo->addItems({
        QStringLiteral("Ollama (Local)"),
        QStringLiteral("OpenAI Compatible"),
        QStringLiteral("Fallback (Rule-based)"),
    });
    form->addRow(QStringLiteral("Backend:"), m_aiBackendCombo);

    m_aiEndpointEdit = new QLineEdit(this);
    m_aiEndpointEdit->setPlaceholderText(QStringLiteral("http://localhost:11434"));
    form->addRow(QStringLiteral("Endpoint:"), m_aiEndpointEdit);

    m_aiApiKeyEdit = new QLineEdit(this);
    m_aiApiKeyEdit->setEchoMode(QLineEdit::Password);
    m_aiApiKeyEdit->setPlaceholderText(QStringLiteral("sk-..."));
    form->addRow(QStringLiteral("API Key:"), m_aiApiKeyEdit);

    auto *modelRow = new QHBoxLayout;
    m_aiModelCombo = new QComboBox(this);
    m_aiModelCombo->setEditable(true);
    m_aiModelCombo->addItems({
        QStringLiteral("tinyllama"),
        QStringLiteral("llama3.2"),
        QStringLiteral("mistral"),
        QStringLiteral("phi3"),
    });
    modelRow->addWidget(m_aiModelCombo, 1);

    m_aiRefreshBtn = new QPushButton(QStringLiteral("Refresh"), this);
    connect(m_aiRefreshBtn, &QPushButton::clicked, this, [this]() {
        AiManager::instance().refreshModels();
    });
    modelRow->addWidget(m_aiRefreshBtn);
    form->addRow(QStringLiteral("Model:"), modelRow);

    layout->addWidget(group);

    // Status & test
    auto *statusRow = new QHBoxLayout;
    m_aiStatusLabel = new QLabel(QStringLiteral("Status: Unknown"), this);
    m_aiStatusLabel->setStyleSheet(QStringLiteral("color: ") + ThemeEngine::instance().colors()["text-secondary"] + QStringLiteral(";"));
    statusRow->addWidget(m_aiStatusLabel);
    statusRow->addStretch();

    m_aiTestBtn = new QPushButton(QStringLiteral("Test Connection"), this);
    connect(m_aiTestBtn, &QPushButton::clicked, this, [this]() {
        auto &ai = AiManager::instance();
        const int backendIdx = m_aiBackendCombo->currentIndex();
        AiManager::Backend be = AiManager::Backend::Ollama;
        if (backendIdx == 1) be = AiManager::Backend::OpenAI;
        else if (backendIdx == 2) be = AiManager::Backend::Fallback;
        ai.configure(be, m_aiEndpointEdit->text(), m_aiApiKeyEdit->text(),
                     m_aiModelCombo->currentText());
        m_aiStatusLabel->setText(QStringLiteral("Testing..."));
    });
    statusRow->addWidget(m_aiTestBtn);
    layout->addLayout(statusRow);

    // Connect model refresh
    connect(&AiManager::instance(), &AiManager::modelsRefreshed, this,
            [this](const QStringList &models) {
                m_aiModelCombo->clear();
                m_aiModelCombo->addItems(models);
                m_aiStatusLabel->setText(QStringLiteral("Found %1 models").arg(models.size()));
            });

    connect(&AiManager::instance(), &AiManager::statusChanged, this,
            [this](AiManager::Status status, const QString &msg) {
                Q_UNUSED(status)
                m_aiStatusLabel->setText(msg);
            });

    layout->addStretch();
    return widget;
}

// ── Privacy tab ──────────────────────────────────────────────────────
QWidget *SettingsDialog::createPrivacyTab()
{
    auto *widget = new QWidget(this);
    auto *form = new QFormLayout(widget);
    form->setContentsMargins(16, 16, 16, 16);
    form->setSpacing(12);

    m_securityPromptsCheck = new QCheckBox(QStringLiteral("Show security prompts on login pages"), this);
    m_securityPromptsCheck->setChecked(true);
    form->addRow(QString(), m_securityPromptsCheck);

    m_doNotTrackCheck = new QCheckBox(QStringLiteral("Send Do Not Track header"), this);
    m_doNotTrackCheck->setChecked(true);
    form->addRow(QString(), m_doNotTrackCheck);

    m_clearOnExitCheck = new QCheckBox(QStringLiteral("Clear browsing data on exit"), this);
    form->addRow(QString(), m_clearOnExitCheck);

    form->addRow(new QLabel(QString(), this)); // spacer

    m_clearDataBtn = new QPushButton(QStringLiteral("Clear All Browsing Data..."), this);
    connect(m_clearDataBtn, &QPushButton::clicked, this, [this]() {
        auto reply = QMessageBox::question(this, QStringLiteral("Clear Data"),
            QStringLiteral("Clear all browsing history, cookies, and cached data?"));
        if (reply == QMessageBox::Yes) {
            // Will be handled by BrowserWindow
        }
    });
    form->addRow(QString(), m_clearDataBtn);

    return widget;
}

// ── Extensions tab ───────────────────────────────────────────────────
QWidget *SettingsDialog::createExtensionsTab()
{
    auto *widget = new QWidget(this);
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(16, 16, 16, 16);

    const auto &ce = ThemeEngine::instance().colors();
    auto *label = new QLabel(QStringLiteral("Installed extensions appear here.\n"
        "Use Tools > Extensions to install new NPI/XPI extensions."), this);
    label->setStyleSheet(QStringLiteral("color: ") + ce["text-secondary"] + QStringLiteral(";"));
    layout->addWidget(label);

    auto *list = new QListWidget(this);
    list->setStyleSheet(
        QStringLiteral("QListWidget { background: ") + ce["bg-dark"]
        + QStringLiteral("; color: ") + ce["text-primary"]
        + QStringLiteral("; border: 1px solid ") + ce["border"]
        + QStringLiteral("; border-radius: 8px; }"
                         "QListWidget::item { padding: 8px; border-radius: 4px; }"
                         "QListWidget::item:selected { background: ") + ce["selection"]
        + QStringLiteral("; }"));
    layout->addWidget(list, 1);

    return widget;
}

// ── Sync tab ─────────────────────────────────────────────────────────
QWidget *SettingsDialog::createSyncTab()
{
    auto *widget = new QWidget(this);
    auto *form = new QFormLayout(widget);
    form->setContentsMargins(16, 16, 16, 16);
    form->setSpacing(12);

    m_syncTokenEdit = new QLineEdit(this);
    m_syncTokenEdit->setEchoMode(QLineEdit::Password);
    m_syncTokenEdit->setPlaceholderText(QStringLiteral("GitHub personal access token"));
    form->addRow(QStringLiteral("Token:"), m_syncTokenEdit);

    m_syncRepoEdit = new QLineEdit(this);
    m_syncRepoEdit->setPlaceholderText(QStringLiteral("username/repo"));
    form->addRow(QStringLiteral("Repository:"), m_syncRepoEdit);

    m_autoSyncCheck = new QCheckBox(QStringLiteral("Enable auto-sync"), this);
    form->addRow(QString(), m_autoSyncCheck);

    m_syncIntervalSpin = new QSpinBox(this);
    m_syncIntervalSpin->setRange(30, 1440);
    m_syncIntervalSpin->setSuffix(QStringLiteral(" min"));
    m_syncIntervalSpin->setValue(60);
    form->addRow(QStringLiteral("Sync interval:"), m_syncIntervalSpin);

    return widget;
}

// ── About tab ────────────────────────────────────────────────────────
QWidget *SettingsDialog::createAboutTab()
{
    auto *widget = new QWidget(this);
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(24, 24, 24, 24);
    layout->setAlignment(Qt::AlignCenter);

    const auto &ca = ThemeEngine::instance().colors();
    auto *titleLabel = new QLabel(QStringLiteral("OyNIx Browser"), this);
    titleLabel->setStyleSheet(QStringLiteral("color: ") + ca["text-primary"]
        + QStringLiteral("; font-size: 24px; font-weight: bold;"));
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);

    auto *versionLabel = new QLabel(QStringLiteral("Version 3.1.0"), this);
    versionLabel->setStyleSheet(QStringLiteral("color: ") + ca["purple-mid"]
        + QStringLiteral("; font-size: 16px;"));
    versionLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(versionLabel);

    auto *descLabel = new QLabel(QStringLiteral(
        "A Chromium-based desktop browser built with Qt6 WebEngine and C++17.\n\n"
        "Features:\n"
        "- Nyx AI Assistant (Ollama / OpenAI)\n"
        "- Custom Nyx search engine with FTS5\n"
        "- NPI extension system\n"
        "- Multiple themes with glassmorphism\n"
        "- GitHub sync for bookmarks and settings\n"
        "- Tree-style tab sidebar\n"
        "- Command palette\n"
        "- Audio player with web media detection\n"
        "- Chrome data import"), this);
    descLabel->setStyleSheet(QStringLiteral("color: ") + ca["text-secondary"]
        + QStringLiteral("; font-size: 12px;"));
    descLabel->setAlignment(Qt::AlignCenter);
    descLabel->setWordWrap(true);
    layout->addWidget(descLabel);

    layout->addStretch();
    return widget;
}

// ── Styles ───────────────────────────────────────────────────────────
void SettingsDialog::applyStyles()
{
    const auto &c = ThemeEngine::instance().colors();

    QString ss;
    ss += QStringLiteral("QDialog { background: ") + c["bg-darkest"] + QStringLiteral("; }\n");
    ss += QStringLiteral("QTabWidget::pane { border: 1px solid ") + c["border"]
       + QStringLiteral("; background: ") + c["bg-dark"] + QStringLiteral("; }\n");
    ss += QStringLiteral("QTabBar::tab { background: ") + c["bg-darkest"]
       + QStringLiteral("; color: ") + c["text-secondary"]
       + QStringLiteral("; padding: 8px 16px; border: 1px solid transparent;"
                        " border-bottom: none; }\n");
    ss += QStringLiteral("QTabBar::tab:selected { background: ") + c["bg-dark"]
       + QStringLiteral("; color: ") + c["text-primary"]
       + QStringLiteral("; border-color: ") + c["purple-mid"] + QStringLiteral("; }\n");
    ss += QStringLiteral("QTabBar::tab:hover { color: ") + c["text-primary"]
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QLabel { color: ") + c["text-primary"] + QStringLiteral("; }\n");
    ss += QStringLiteral("QLineEdit { background: ") + c["bg-mid"]
       + QStringLiteral("; color: ") + c["text-primary"]
       + QStringLiteral("; border: 1px solid ") + c["border"]
       + QStringLiteral("; border-radius: 6px; padding: 6px; }\n");
    ss += QStringLiteral("QLineEdit:focus { border-color: ") + c["purple-mid"]
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QComboBox { background: ") + c["bg-mid"]
       + QStringLiteral("; color: ") + c["text-primary"]
       + QStringLiteral("; border: 1px solid ") + c["border"]
       + QStringLiteral("; border-radius: 6px; padding: 4px 8px; }\n");
    ss += QStringLiteral("QComboBox::drop-down { border: none; }\n");
    ss += QStringLiteral("QComboBox QAbstractItemView { background: ") + c["bg-darkest"]
       + QStringLiteral("; color: ") + c["text-primary"]
       + QStringLiteral("; selection-background-color: ") + c["purple-dark"]
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QCheckBox { color: ") + c["text-primary"]
       + QStringLiteral("; spacing: 8px; }\n");
    ss += QStringLiteral("QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid ")
       + c["purple-mid"] + QStringLiteral("; border-radius: 4px; background: ") + c["bg-mid"]
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QCheckBox::indicator:checked { background: ") + c["purple-mid"]
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QSpinBox { background: ") + c["bg-mid"]
       + QStringLiteral("; color: ") + c["text-primary"]
       + QStringLiteral("; border: 1px solid ") + c["border"]
       + QStringLiteral("; border-radius: 6px; padding: 4px; }\n");
    ss += QStringLiteral("QGroupBox { color: ") + c["text-primary"]
       + QStringLiteral("; border: 1px solid ") + c["border"]
       + QStringLiteral("; border-radius: 8px; margin-top: 12px; padding-top: 16px; }\n");
    ss += QStringLiteral("QGroupBox::title { subcontrol-origin: margin; left: 12px;"
                         " padding: 0 4px; }\n");
    ss += QStringLiteral("QPushButton { background: ") + c["purple-mid"]
       + QStringLiteral("; color: white; border: none; border-radius: 6px;"
                        " padding: 6px 14px; }\n");
    ss += QStringLiteral("QPushButton:hover { background: ") + c["purple-light"]
       + QStringLiteral("; }\n");
    ss += QStringLiteral("QDialogButtonBox QPushButton { min-width: 80px; }\n");
    setStyleSheet(ss);
}

// ── Load / Save ──────────────────────────────────────────────────────
void SettingsDialog::loadFromConfig()
{
    if (m_homepageEdit)
        m_homepageEdit->setText(m_config[QStringLiteral("homepage")].toString(QStringLiteral("oyn://home")));
    if (m_themeCombo) {
        const int idx = m_themeCombo->findText(m_config[QStringLiteral("theme")].toString());
        if (idx >= 0) m_themeCombo->setCurrentIndex(idx);
    }
    if (m_searchEngineCombo) {
        const int idx = m_searchEngineCombo->findText(
            m_config[QStringLiteral("search_engine")].toString());
        if (idx >= 0) m_searchEngineCombo->setCurrentIndex(idx);
    }
    if (m_treeTabsCheck)
        m_treeTabsCheck->setChecked(m_config[QStringLiteral("tree_tabs")].toBool());
    if (m_restoreSessionCheck)
        m_restoreSessionCheck->setChecked(m_config[QStringLiteral("restore_session")].toBool());
    if (m_securityPromptsCheck)
        m_securityPromptsCheck->setChecked(
            m_config[QStringLiteral("security_prompts")].toBool(true));

    // AI settings
    auto &ai = AiManager::instance();
    if (m_aiEndpointEdit) m_aiEndpointEdit->setText(ai.endpoint());
    if (m_aiModelCombo) m_aiModelCombo->setCurrentText(ai.model());
    if (m_aiBackendCombo) m_aiBackendCombo->setCurrentIndex(static_cast<int>(ai.backend()));
}

void SettingsDialog::saveToConfig()
{
    if (m_homepageEdit)
        m_config[QStringLiteral("homepage")] = m_homepageEdit->text();
    if (m_themeCombo)
        m_config[QStringLiteral("theme")] = m_themeCombo->currentText();
    if (m_searchEngineCombo)
        m_config[QStringLiteral("search_engine")] = m_searchEngineCombo->currentText();
    if (m_treeTabsCheck)
        m_config[QStringLiteral("tree_tabs")] = m_treeTabsCheck->isChecked();
    if (m_restoreSessionCheck)
        m_config[QStringLiteral("restore_session")] = m_restoreSessionCheck->isChecked();
    if (m_maxTabsSpin)
        m_config[QStringLiteral("max_tabs")] = m_maxTabsSpin->value();
    if (m_securityPromptsCheck)
        m_config[QStringLiteral("security_prompts")] = m_securityPromptsCheck->isChecked();
    if (m_animationsCheck)
        m_config[QStringLiteral("animations")] = m_animationsCheck->isChecked();
    if (m_doNotTrackCheck)
        m_config[QStringLiteral("do_not_track")] = m_doNotTrackCheck->isChecked();
    if (m_clearOnExitCheck)
        m_config[QStringLiteral("clear_on_exit")] = m_clearOnExitCheck->isChecked();

    // AI
    auto &ai = AiManager::instance();
    AiManager::Backend be = AiManager::Backend::Ollama;
    if (m_aiBackendCombo) {
        const int idx = m_aiBackendCombo->currentIndex();
        if (idx == 1) be = AiManager::Backend::OpenAI;
        else if (idx == 2) be = AiManager::Backend::Fallback;
    }
    ai.configure(be,
                 m_aiEndpointEdit ? m_aiEndpointEdit->text() : QString(),
                 m_aiApiKeyEdit ? m_aiApiKeyEdit->text() : QString(),
                 m_aiModelCombo ? m_aiModelCombo->currentText() : QString());
    emit aiConfigChanged();

    // Sync
    if (m_syncTokenEdit)
        m_config[QStringLiteral("sync_token")] = m_syncTokenEdit->text();
    if (m_syncRepoEdit)
        m_config[QStringLiteral("sync_repo")] = m_syncRepoEdit->text();
    if (m_autoSyncCheck)
        m_config[QStringLiteral("auto_sync")] = m_autoSyncCheck->isChecked();
    if (m_syncIntervalSpin)
        m_config[QStringLiteral("sync_interval")] = m_syncIntervalSpin->value();
}

QJsonObject SettingsDialog::updatedConfig() const
{
    return m_config;
}
