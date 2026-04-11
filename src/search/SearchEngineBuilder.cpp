#include "SearchEngineBuilder.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QLineEdit>
#include <QComboBox>
#include <QTabWidget>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QDesktopServices>
#include <QUrl>

// ── Constructor / Destructor ─────────────────────────────────────────
SearchEngineBuilder::SearchEngineBuilder(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Search Engine Builder"));
    resize(520, 440);
    setupUi();
    applyStyles();
    loadFromDisk();
}

SearchEngineBuilder::~SearchEngineBuilder() = default;

// ── UI Setup ─────────────────────────────────────────────────────────
void SearchEngineBuilder::setupUi()
{
    auto *layout = new QVBoxLayout(this);

    // Existing engines
    auto *existingGroup = new QGroupBox(QStringLiteral("Custom Search Engines"), this);
    auto *existingLayout = new QHBoxLayout(existingGroup);
    m_engineList = new QComboBox(this);
    m_engineList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    existingLayout->addWidget(m_engineList, 1);

    auto *removeBtn = new QPushButton(QStringLiteral("Remove"), this);
    connect(removeBtn, &QPushButton::clicked, this, &SearchEngineBuilder::removeSelectedEngine);
    existingLayout->addWidget(removeBtn);
    layout->addWidget(existingGroup);

    // Tabs
    m_tabs = new QTabWidget(this);
    m_tabs->addTab(createVisualBuilder(), QStringLiteral("Visual Builder"));
    m_tabs->addTab(createImportTab(),     QStringLiteral("Import"));
    m_tabs->addTab(createCodeTab(),       QStringLiteral("Code"));
    layout->addWidget(m_tabs);

    // Test area
    auto *testRow = new QHBoxLayout;
    m_testResult = new QLabel(this);
    m_testResult->setStyleSheet(QStringLiteral("color: #A8A0B8; font-size: 11px;"));
    testRow->addWidget(m_testResult, 1);

    auto *testBtn = new QPushButton(QStringLiteral("Test"), this);
    connect(testBtn, &QPushButton::clicked, this, &SearchEngineBuilder::testEngine);
    testRow->addWidget(testBtn);
    layout->addLayout(testRow);

    // Buttons
    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttonBox, &QDialogButtonBox::accepted, this, [this]() {
        saveToDisk();
        accept();
    });
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttonBox);
}

QWidget *SearchEngineBuilder::createVisualBuilder()
{
    auto *widget = new QWidget(this);
    auto *form = new QFormLayout(widget);
    form->setContentsMargins(12, 12, 12, 12);
    form->setSpacing(10);

    m_nameEdit = new QLineEdit(this);
    m_nameEdit->setPlaceholderText(QStringLiteral("My Search Engine"));
    form->addRow(QStringLiteral("Name:"), m_nameEdit);

    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText(QStringLiteral("https://example.com/search?q={query}"));
    form->addRow(QStringLiteral("Search URL:"), m_urlEdit);

    m_iconEdit = new QLineEdit(this);
    m_iconEdit->setPlaceholderText(QStringLiteral("https://example.com/favicon.ico"));
    form->addRow(QStringLiteral("Icon URL:"), m_iconEdit);

    m_suggestEdit = new QLineEdit(this);
    m_suggestEdit->setPlaceholderText(QStringLiteral("https://example.com/suggest?q={query}"));
    form->addRow(QStringLiteral("Suggestions URL:"), m_suggestEdit);

    auto *addBtn = new QPushButton(QStringLiteral("Add Engine"), this);
    connect(addBtn, &QPushButton::clicked, this, &SearchEngineBuilder::addEngine);
    form->addRow(QString(), addBtn);

    auto *helpLabel = new QLabel(QStringLiteral(
        "Use {query} or %s as placeholder for the search term."), this);
    helpLabel->setStyleSheet(QStringLiteral("color: #605878; font-size: 11px;"));
    helpLabel->setWordWrap(true);
    form->addRow(QString(), helpLabel);

    return widget;
}

QWidget *SearchEngineBuilder::createImportTab()
{
    auto *widget = new QWidget(this);
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(12, 12, 12, 12);

    auto *label = new QLabel(QStringLiteral(
        "Paste OpenSearch XML or JSON definition:"), this);
    label->setStyleSheet(QStringLiteral("color: #A8A0B8;"));
    layout->addWidget(label);

    m_importEdit = new QTextEdit(this);
    m_importEdit->setPlaceholderText(QStringLiteral(
        "<OpenSearchDescription>\n  <ShortName>...</ShortName>\n  <Url template=\"...\"/>\n</OpenSearchDescription>"));
    layout->addWidget(m_importEdit, 1);

    auto *importBtn = new QPushButton(QStringLiteral("Import"), this);
    connect(importBtn, &QPushButton::clicked, this, [this]() {
        const QString text = m_importEdit->toPlainText().trimmed();
        if (text.isEmpty()) return;

        // Try JSON
        auto doc = QJsonDocument::fromJson(text.toUtf8());
        if (doc.isObject()) {
            auto obj = doc.object();
            m_engines.append(obj);
            m_engineList->addItem(obj[QStringLiteral("name")].toString());
            return;
        }

        // Try OpenSearch XML (basic parsing)
        QJsonObject engine;
        QRegularExpression nameRx(QStringLiteral("<ShortName>(.+?)</ShortName>"));
        QRegularExpression urlRx(QStringLiteral("template=\"(.+?)\""));
        auto nameMatch = nameRx.match(text);
        auto urlMatch = urlRx.match(text);

        if (nameMatch.hasMatch()) engine[QStringLiteral("name")] = nameMatch.captured(1);
        if (urlMatch.hasMatch()) engine[QStringLiteral("url_template")] = urlMatch.captured(1);

        if (!engine.isEmpty()) {
            m_engines.append(engine);
            m_engineList->addItem(engine[QStringLiteral("name")].toString());
        }
    });
    layout->addWidget(importBtn);

    return widget;
}

QWidget *SearchEngineBuilder::createCodeTab()
{
    auto *widget = new QWidget(this);
    auto *layout = new QVBoxLayout(widget);
    layout->setContentsMargins(12, 12, 12, 12);

    auto *label = new QLabel(QStringLiteral("Custom search engine (JSON):"), this);
    label->setStyleSheet(QStringLiteral("color: #A8A0B8;"));
    layout->addWidget(label);

    m_codeEdit = new QTextEdit(this);
    m_codeEdit->setPlaceholderText(QStringLiteral(
        "{\n  \"name\": \"My Engine\",\n  \"url_template\": \"https://...\",\n  \"icon\": \"...\"\n}"));
    layout->addWidget(m_codeEdit, 1);

    return widget;
}

// ── Actions ──────────────────────────────────────────────────────────
void SearchEngineBuilder::addEngine()
{
    const QString name = m_nameEdit->text().trimmed();
    QString urlTemplate = m_urlEdit->text().trimmed();
    if (name.isEmpty() || urlTemplate.isEmpty()) return;

    // Normalize placeholder
    urlTemplate.replace(QStringLiteral("%s"), QStringLiteral("{query}"));

    QJsonObject engine;
    engine[QStringLiteral("name")]         = name;
    engine[QStringLiteral("url_template")] = urlTemplate;
    engine[QStringLiteral("icon")]         = m_iconEdit->text().trimmed();
    engine[QStringLiteral("suggest_url")]  = m_suggestEdit->text().trimmed();

    m_engines.append(engine);
    m_engineList->addItem(name);

    // Clear form
    m_nameEdit->clear();
    m_urlEdit->clear();
    m_iconEdit->clear();
    m_suggestEdit->clear();
}

void SearchEngineBuilder::testEngine()
{
    const int idx = m_engineList->currentIndex();
    if (idx < 0 || idx >= m_engines.size()) return;

    auto engine = m_engines[idx].toObject();
    QString url = engine[QStringLiteral("url_template")].toString();
    url.replace(QStringLiteral("{query}"), QStringLiteral("test+search"));

    m_testResult->setText(QStringLiteral("Test URL: %1").arg(url));
    QDesktopServices::openUrl(QUrl(url));
}

void SearchEngineBuilder::removeSelectedEngine()
{
    const int idx = m_engineList->currentIndex();
    if (idx < 0 || idx >= m_engines.size()) return;

    m_engines.removeAt(idx);
    m_engineList->removeItem(idx);
}

QJsonArray SearchEngineBuilder::customEngines() const { return m_engines; }

void SearchEngineBuilder::loadEngines(const QJsonArray &engines)
{
    m_engines = engines;
    m_engineList->clear();
    for (const auto &val : engines) {
        m_engineList->addItem(val.toObject()[QStringLiteral("name")].toString());
    }
}

// ── Persistence ──────────────────────────────────────────────────────
void SearchEngineBuilder::loadFromDisk()
{
    const QString path = QDir::homePath() + QStringLiteral("/.config/oynix/custom_engines.json");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return;
    auto doc = QJsonDocument::fromJson(f.readAll());
    if (doc.isArray()) loadEngines(doc.array());
}

void SearchEngineBuilder::saveToDisk()
{
    const QString dir = QDir::homePath() + QStringLiteral("/.config/oynix");
    QDir().mkpath(dir);
    QFile f(dir + QStringLiteral("/custom_engines.json"));
    if (f.open(QIODevice::WriteOnly))
        f.write(QJsonDocument(m_engines).toJson());
    emit enginesChanged(m_engines);
}

void SearchEngineBuilder::applyStyles()
{
    setStyleSheet(QStringLiteral(
        "QDialog { background: #08080d; }"
        "QGroupBox { color: #E8E0F0; border: 1px solid #2a2a40; border-radius: 6px;"
        "  margin-top: 12px; padding-top: 16px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 4px; }"
        "QLabel { color: #E8E0F0; }"
        "QLineEdit, QTextEdit { background: #12121e; color: #E8E0F0; border: 1px solid #2a2a40;"
        "  border-radius: 4px; padding: 6px; }"
        "QLineEdit:focus, QTextEdit:focus { border-color: #7B4FBF; }"
        "QComboBox { background: #12121e; color: #E8E0F0; border: 1px solid #2a2a40;"
        "  border-radius: 4px; padding: 4px 8px; }"
        "QTabWidget::pane { border: 1px solid #2a2a40; background: #0a0a12; }"
        "QTabBar::tab { background: #0e0e16; color: #A8A0B8; padding: 6px 14px; }"
        "QTabBar::tab:selected { background: #0a0a12; color: #E8E0F0; }"
        "QPushButton { background: #7B4FBF; color: white; border: none;"
        "  border-radius: 4px; padding: 6px 14px; }"
        "QPushButton:hover { background: #9B6FDF; }"
    ));
}
