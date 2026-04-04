#include "AiManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QRegularExpression>

// ── Singleton ────────────────────────────────────────────────────────
AiManager &AiManager::instance()
{
    static AiManager s;
    return s;
}

AiManager::AiManager(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    loadConfig();
}

AiManager::~AiManager() { saveConfig(); }

// ── Config persistence ───────────────────────────────────────────────
QString AiManager::configFilePath() const
{
#ifdef Q_OS_WIN
    return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation)
           + QStringLiteral("/oynix/ai_config.json");
#else
    return QDir::homePath() + QStringLiteral("/.config/oynix/ai_config.json");
#endif
}

void AiManager::loadConfig()
{
    QFile f(configFilePath());
    if (!f.open(QIODevice::ReadOnly)) {
        // Defaults: Ollama on localhost
        m_backend  = Backend::Ollama;
        m_endpoint = QStringLiteral("http://localhost:11434");
        m_model    = QStringLiteral("tinyllama");
        m_status   = Status::Fallback;
        return;
    }
    auto doc = QJsonDocument::fromJson(f.readAll());
    auto obj = doc.object();
    const QString be = obj[QStringLiteral("backend")].toString(QStringLiteral("ollama"));
    if (be == QLatin1String("openai"))       m_backend = Backend::OpenAI;
    else if (be == QLatin1String("fallback")) m_backend = Backend::Fallback;
    else                                      m_backend = Backend::Ollama;

    m_endpoint = obj[QStringLiteral("endpoint")].toString(QStringLiteral("http://localhost:11434"));
    m_apiKey   = obj[QStringLiteral("api_key")].toString();
    m_model    = obj[QStringLiteral("model")].toString(QStringLiteral("tinyllama"));
    m_status   = Status::Fallback;
}

void AiManager::saveConfig()
{
    QDir().mkpath(QFileInfo(configFilePath()).path());
    QFile f(configFilePath());
    if (!f.open(QIODevice::WriteOnly)) return;
    QJsonObject obj;
    switch (m_backend) {
    case Backend::Ollama:   obj[QStringLiteral("backend")] = QStringLiteral("ollama"); break;
    case Backend::OpenAI:   obj[QStringLiteral("backend")] = QStringLiteral("openai"); break;
    case Backend::Fallback: obj[QStringLiteral("backend")] = QStringLiteral("fallback"); break;
    }
    obj[QStringLiteral("endpoint")] = m_endpoint;
    obj[QStringLiteral("api_key")]  = m_apiKey;
    obj[QStringLiteral("model")]    = m_model;
    f.write(QJsonDocument(obj).toJson());
}

QJsonObject AiManager::getConfig() const
{
    QJsonObject obj;
    obj[QStringLiteral("backend")]  = static_cast<int>(m_backend);
    obj[QStringLiteral("endpoint")] = m_endpoint;
    obj[QStringLiteral("model")]    = m_model;
    obj[QStringLiteral("status")]   = static_cast<int>(m_status);
    return obj;
}

// ── Configuration setters ────────────────────────────────────────────
void AiManager::configure(Backend backend, const QString &endpoint,
                           const QString &apiKey, const QString &model)
{
    m_backend  = backend;
    m_endpoint = endpoint;
    m_apiKey   = apiKey;
    m_model    = model;
    saveConfig();
    refreshModels();
}

void AiManager::setBackend(Backend b)  { m_backend = b; saveConfig(); }
void AiManager::setModel(const QString &m) { m_model = m; saveConfig(); }
void AiManager::setEndpoint(const QString &e) { m_endpoint = e; saveConfig(); }
void AiManager::setApiKey(const QString &k) { m_apiKey = k; saveConfig(); }

// ── History management ───────────────────────────────────────────────
void AiManager::addToHistory(const QString &role, const QString &content)
{
    QJsonObject msg;
    msg[QStringLiteral("role")]    = role;
    msg[QStringLiteral("content")] = content;
    m_conversationHistory.append(msg);

    // Trim to max
    while (m_conversationHistory.size() > MaxHistoryMessages)
        m_conversationHistory.removeFirst();
}

void AiManager::clearHistory()
{
    m_conversationHistory = QJsonArray();
}

QString AiManager::buildContextPrompt(const QString &userMessage) const
{
    QString context;
    for (const auto &val : m_conversationHistory) {
        auto msg = val.toObject();
        const QString role = msg[QStringLiteral("role")].toString();
        const QString content = msg[QStringLiteral("content")].toString();
        if (role == QLatin1String("user"))
            context += QStringLiteral("User: %1\n").arg(content);
        else
            context += QStringLiteral("Assistant: %1\n").arg(content);
    }
    context += QStringLiteral("User: %1\nAssistant:").arg(userMessage);
    return context;
}

// ── Chat ─────────────────────────────────────────────────────────────
void AiManager::chat(const QString &userMessage)
{
    addToHistory(QStringLiteral("user"), userMessage);

    switch (m_backend) {
    case Backend::Ollama:
        sendOllamaRequest(buildContextPrompt(userMessage));
        break;
    case Backend::OpenAI:
        sendOpenAiRequest(userMessage);
        break;
    case Backend::Fallback: {
        const QString resp = fallbackResponse(userMessage);
        addToHistory(QStringLiteral("assistant"), resp);
        emit responseReceived(resp);
        break;
    }
    }
}

void AiManager::summarizePage(const QString &pageContent, const QString &pageTitle)
{
    QString content = pageContent.left(MaxContentLength);
    const QString prompt = QStringLiteral(
        "Summarize the following web page titled \"%1\" in 3-5 concise bullet points:\n\n%2")
        .arg(pageTitle, content);
    chat(prompt);
}

void AiManager::explainCode(const QString &code)
{
    const QString prompt = QStringLiteral(
        "Explain the following code in simple terms:\n\n```\n%1\n```").arg(code.left(MaxContentLength));
    chat(prompt);
}

void AiManager::analyzeSearchResults(const QJsonArray &results, const QString &query)
{
    QString text = QStringLiteral("Search results for \"%1\":\n").arg(query);
    int i = 0;
    for (const auto &val : results) {
        if (++i > 10) break;
        auto obj = val.toObject();
        text += QStringLiteral("%1. %2 - %3\n").arg(i)
            .arg(obj[QStringLiteral("title")].toString(),
                 obj[QStringLiteral("url")].toString());
    }
    text += QStringLiteral("\nBriefly analyze which results are most relevant and why.");
    chat(text);
}

// ── Ollama backend ───────────────────────────────────────────────────
void AiManager::sendOllamaRequest(const QString &prompt, bool /*stream*/)
{
    const QString url = m_endpoint + QStringLiteral("/api/generate");
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QJsonObject body;
    body[QStringLiteral("model")]  = m_model;
    body[QStringLiteral("prompt")] = prompt;
    body[QStringLiteral("stream")] = false;

    m_status = Status::Loading;
    emit statusChanged(m_status, QStringLiteral("Generating response..."));

    auto *reply = m_nam->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleOllamaResponse(reply);
    });
}

void AiManager::handleOllamaResponse(QNetworkReply *reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        m_status = Status::Fallback;
        emit statusChanged(m_status, QStringLiteral("Ollama unavailable, using fallback"));
        // Fall back to rule-based
        const QString lastMsg = m_conversationHistory.last().toObject()
                                    [QStringLiteral("content")].toString();
        const QString resp = fallbackResponse(lastMsg);
        addToHistory(QStringLiteral("assistant"), resp);
        emit responseReceived(resp);
        return;
    }

    auto doc = QJsonDocument::fromJson(reply->readAll());
    const QString response = doc.object()[QStringLiteral("response")].toString();
    m_status = Status::Ready;
    emit statusChanged(m_status, QStringLiteral("Ready"));
    addToHistory(QStringLiteral("assistant"), response);
    emit responseReceived(response);
}

// ── OpenAI-compatible backend ────────────────────────────────────────
void AiManager::sendOpenAiRequest(const QString &prompt, bool /*stream*/)
{
    const QString url = m_endpoint + QStringLiteral("/v1/chat/completions");
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    if (!m_apiKey.isEmpty())
        req.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());

    // Build messages array
    QJsonArray messages;
    QJsonObject sysMsg;
    sysMsg[QStringLiteral("role")]    = QStringLiteral("system");
    sysMsg[QStringLiteral("content")] = QStringLiteral(
        "You are Nyx, the AI assistant built into OyNIx Browser. "
        "Be helpful, concise, and friendly.");
    messages.append(sysMsg);

    for (const auto &val : m_conversationHistory) {
        messages.append(val.toObject());
    }

    QJsonObject body;
    body[QStringLiteral("model")]    = m_model;
    body[QStringLiteral("messages")] = messages;
    body[QStringLiteral("stream")]   = false;

    m_status = Status::Loading;
    emit statusChanged(m_status, QStringLiteral("Generating response..."));

    auto *reply = m_nam->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        handleOpenAiResponse(reply);
    });
}

void AiManager::handleOpenAiResponse(QNetworkReply *reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        m_status = Status::Error;
        emit statusChanged(m_status, reply->errorString());
        emit errorOccurred(reply->errorString());
        return;
    }

    auto doc = QJsonDocument::fromJson(reply->readAll());
    auto choices = doc.object()[QStringLiteral("choices")].toArray();
    if (choices.isEmpty()) {
        emit errorOccurred(QStringLiteral("Empty response from API"));
        return;
    }

    const QString response = choices[0].toObject()
        [QStringLiteral("message")].toObject()
        [QStringLiteral("content")].toString();

    m_status = Status::Ready;
    emit statusChanged(m_status, QStringLiteral("Ready"));
    addToHistory(QStringLiteral("assistant"), response);
    emit responseReceived(response);
}

// ── Model management ─────────────────────────────────────────────────
void AiManager::refreshModels()
{
    if (m_backend == Backend::Ollama) {
        const QString url = m_endpoint + QStringLiteral("/api/tags");
        QNetworkRequest req{QUrl(url)};
        auto *reply = m_nam->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            handleOllamaModels(reply);
        });
    } else if (m_backend == Backend::OpenAI) {
        const QString url = m_endpoint + QStringLiteral("/v1/models");
        QNetworkRequest req{QUrl(url)};
        if (!m_apiKey.isEmpty())
            req.setRawHeader("Authorization", ("Bearer " + m_apiKey).toUtf8());
        auto *reply = m_nam->get(req);
        connect(reply, &QNetworkReply::finished, this, [this, reply]() {
            reply->deleteLater();
            if (reply->error() != QNetworkReply::NoError) return;
            auto doc = QJsonDocument::fromJson(reply->readAll());
            m_availableModels.clear();
            for (const auto &val : doc.object()[QStringLiteral("data")].toArray()) {
                m_availableModels << val.toObject()[QStringLiteral("id")].toString();
            }
            emit modelsRefreshed(m_availableModels);
        });
    }
}

void AiManager::handleOllamaModels(QNetworkReply *reply)
{
    reply->deleteLater();
    if (reply->error() != QNetworkReply::NoError) {
        m_status = Status::Fallback;
        emit statusChanged(m_status, QStringLiteral("Cannot reach Ollama"));
        return;
    }
    auto doc = QJsonDocument::fromJson(reply->readAll());
    m_availableModels.clear();
    for (const auto &val : doc.object()[QStringLiteral("models")].toArray()) {
        m_availableModels << val.toObject()[QStringLiteral("name")].toString();
    }
    m_status = Status::Ready;
    emit statusChanged(m_status, QStringLiteral("Connected to Ollama"));
    emit modelsRefreshed(m_availableModels);
}

void AiManager::pullModel(const QString &modelName)
{
    if (m_backend != Backend::Ollama) return;
    const QString url = m_endpoint + QStringLiteral("/api/pull");
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QJsonObject body;
    body[QStringLiteral("name")]   = modelName;
    body[QStringLiteral("stream")] = false;

    m_status = Status::Downloading;
    emit statusChanged(m_status, QStringLiteral("Pulling model %1...").arg(modelName));

    auto *reply = m_nam->post(req, QJsonDocument(body).toJson());
    connect(reply, &QNetworkReply::finished, this, [this, reply, modelName]() {
        reply->deleteLater();
        if (reply->error() == QNetworkReply::NoError) {
            m_status = Status::Ready;
            emit statusChanged(m_status, QStringLiteral("Model %1 ready").arg(modelName));
            refreshModels();
        } else {
            m_status = Status::Error;
            emit statusChanged(m_status, QStringLiteral("Failed to pull %1").arg(modelName));
        }
    });
}

// ── Fallback rule-based responses ────────────────────────────────────
QString AiManager::fallbackResponse(const QString &prompt) const
{
    const QString lower = prompt.toLower();

    if (lower.contains(QStringLiteral("hello")) || lower.contains(QStringLiteral("hi")))
        return QStringLiteral("Hello! I'm Nyx, the OyNIx Browser AI assistant. "
                              "I'm currently running in fallback mode. To enable full AI, "
                              "install Ollama and run a model like TinyLlama.");

    if (lower.contains(QStringLiteral("summarize")) || lower.contains(QStringLiteral("summary")))
        return QStringLiteral("I'm in fallback mode and can't summarize pages right now. "
                              "To enable AI summarization, configure an Ollama or OpenAI endpoint "
                              "in Settings > AI.");

    if (lower.contains(QStringLiteral("search")) || lower.contains(QStringLiteral("find")))
        return QStringLiteral("You can use the Nyx search bar (Ctrl+L) to search across your "
                              "bookmarks, history, and the web. Try typing your query there!");

    if (lower.contains(QStringLiteral("bookmark")))
        return QStringLiteral("To bookmark a page, click the star icon in the toolbar or press Ctrl+D. "
                              "View all bookmarks with Ctrl+B or go to oyn://bookmarks.");

    if (lower.contains(QStringLiteral("theme")) || lower.contains(QStringLiteral("dark")))
        return QStringLiteral("OyNIx comes with several themes: Nyx Dark, Midnight, Violet, "
                              "Amethyst, and Ember. Go to View > Themes to switch.");

    if (lower.contains(QStringLiteral("extension")) || lower.contains(QStringLiteral("npi")))
        return QStringLiteral("OyNIx supports NPI extensions. Go to Tools > Extensions to manage them, "
                              "or use the NPI Builder to create your own.");

    if (lower.contains(QStringLiteral("help")))
        return QStringLiteral("I can help with:\n"
                              "- Summarizing web pages\n"
                              "- Explaining code snippets\n"
                              "- Searching your browsing data\n"
                              "- Browser tips and shortcuts\n\n"
                              "For full AI capabilities, configure Ollama in Settings > AI.");

    return QStringLiteral("I'm Nyx, running in fallback mode. For full AI capabilities, "
                          "set up Ollama (http://localhost:11434) or an OpenAI-compatible API "
                          "in Settings > AI. I can still help with basic browser questions!");
}
