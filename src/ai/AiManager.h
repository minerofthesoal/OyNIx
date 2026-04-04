#pragma once

#include <QObject>
#include <QJsonArray>
#include <QJsonObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;

/**
 * AiManager — HTTP-based LLM integration.
 * Supports Ollama (local), OpenAI-compatible APIs, and a built-in fallback.
 * Manages model selection, conversation history, and async inference.
 */
class AiManager : public QObject
{
    Q_OBJECT

public:
    enum class Backend { Ollama, OpenAI, Fallback };
    Q_ENUM(Backend)

    enum class Status { Ready, Loading, Downloading, Error, Fallback };
    Q_ENUM(Status)

    static AiManager &instance();

    // Configuration
    void configure(Backend backend, const QString &endpoint, const QString &apiKey,
                   const QString &model);
    void setBackend(Backend backend);
    void setModel(const QString &model);
    void setEndpoint(const QString &endpoint);
    void setApiKey(const QString &apiKey);

    [[nodiscard]] Backend backend() const { return m_backend; }
    [[nodiscard]] Status status() const { return m_status; }
    [[nodiscard]] QString model() const { return m_model; }
    [[nodiscard]] QString endpoint() const { return m_endpoint; }
    [[nodiscard]] QStringList availableModels() const { return m_availableModels; }
    [[nodiscard]] QJsonObject getConfig() const;

    // Inference
    void chat(const QString &userMessage);
    void summarizePage(const QString &pageContent, const QString &pageTitle);
    void explainCode(const QString &code);
    void analyzeSearchResults(const QJsonArray &results, const QString &query);
    void clearHistory();

    // Model management
    void refreshModels();
    void pullModel(const QString &modelName);

signals:
    void responseReceived(const QString &response);
    void responseStreaming(const QString &partialResponse);
    void statusChanged(AiManager::Status status, const QString &message);
    void modelsRefreshed(const QStringList &models);
    void modelPullProgress(const QString &model, int percent);
    void errorOccurred(const QString &error);

private:
    explicit AiManager(QObject *parent = nullptr);
    ~AiManager() override;

    void loadConfig();
    void saveConfig();
    QString configFilePath() const;

    void sendOllamaRequest(const QString &prompt, bool stream = false);
    void sendOpenAiRequest(const QString &prompt, bool stream = false);
    QString fallbackResponse(const QString &prompt) const;

    void handleOllamaResponse(QNetworkReply *reply);
    void handleOpenAiResponse(QNetworkReply *reply);
    void handleOllamaModels(QNetworkReply *reply);

    void addToHistory(const QString &role, const QString &content);
    QString buildContextPrompt(const QString &userMessage) const;

    QNetworkAccessManager *m_nam = nullptr;
    Backend m_backend = Backend::Ollama;
    Status m_status = Status::Fallback;
    QString m_endpoint;
    QString m_apiKey;
    QString m_model;
    QStringList m_availableModels;
    QJsonArray m_conversationHistory;

    static constexpr int MaxHistoryMessages = 20;
    static constexpr int MaxContentLength = 4000;
};
