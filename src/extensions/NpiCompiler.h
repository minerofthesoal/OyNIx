#pragma once

#include <QByteArray>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QString>
#include <QStringList>

class NpiCompiler : public QObject
{
    Q_OBJECT

public:
    explicit NpiCompiler(QObject *parent = nullptr);
    ~NpiCompiler() override;

    /// Compile an NPI extension from a manifest and a set of files.
    /// @param manifest  The parsed manifest.json object.
    /// @param files     Map of relative-path -> contents for every file to include.
    /// @param outputPath  Destination .npi file path.
    /// @return true on success.
    bool compile(const QJsonObject &manifest,
                 const QMap<QString, QByteArray> &files,
                 const QString &outputPath);

    /// Validate that a manifest contains all required fields.
    /// @return Empty list if valid; otherwise a list of error strings.
    [[nodiscard]] QStringList validate(const QJsonObject &manifest) const;
};
