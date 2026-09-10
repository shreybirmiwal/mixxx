#include "tutorial/tutorialcatalog.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QtMath>

namespace mixxx::tutorial {
namespace {

struct TrackRecord {
    QString id;
    QString title;
    QString artist;
    QString location;
    QString license;
    double bpm{0.0};
    double durationSeconds{0.0};
};

void setError(QString* pError, const QString& value) {
    if (pError) {
        *pError = value;
    }
}

} // namespace

Catalog Catalog::load(const QString& path, QString* pError) {
    Catalog catalog;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(pError, QStringLiteral("Cannot open tutorial catalog: %1").arg(path));
        return catalog;
    }

    QJsonParseError parseError;
    const QJsonDocument document =
            QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(pError,
                QStringLiteral("Invalid tutorial catalog JSON: %1")
                        .arg(parseError.errorString()));
        return catalog;
    }

    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("schemaVersion")).toInt() != 1) {
        setError(pError, QStringLiteral("Unsupported tutorial catalog schema"));
        return catalog;
    }

    const QDir catalogDirectory = QFileInfo(path).absoluteDir();
    QHash<QString, TrackRecord> tracks;
    const QJsonArray trackValues = root.value(QStringLiteral("tracks")).toArray();
    for (const QJsonValue& value : trackValues) {
        const QJsonObject object = value.toObject();
        TrackRecord record;
        record.id = object.value(QStringLiteral("id")).toString();
        record.title = object.value(QStringLiteral("title")).toString();
        record.artist = object.value(QStringLiteral("artist")).toString();
        record.location = catalogDirectory.absoluteFilePath(
                object.value(QStringLiteral("file")).toString());
        record.license = object.value(QStringLiteral("license")).toString();
        record.bpm = object.value(QStringLiteral("bpm")).toDouble();
        record.durationSeconds =
                object.value(QStringLiteral("durationSeconds")).toDouble();
        if (record.id.isEmpty() || record.title.isEmpty() ||
                record.artist.isEmpty() || record.license.isEmpty() ||
                record.durationSeconds <= 0.0 ||
                !QFileInfo::exists(record.location) || tracks.contains(record.id)) {
            setError(pError,
                    QStringLiteral("Invalid or missing tutorial track: %1")
                            .arg(record.id));
            return catalog;
        }
        tracks.insert(record.id, record);
    }
    if (tracks.isEmpty()) {
        setError(pError, QStringLiteral("Tutorial catalog contains no tracks"));
        return catalog;
    }

    const QJsonObject tutorials = root.value(QStringLiteral("tutorials")).toObject();
    for (auto tutorial = tutorials.constBegin(); tutorial != tutorials.constEnd();
            ++tutorial) {
        QList<TrackAssignment> assignments;
        QSet<int> assignedDecks;
        const QJsonArray decks = tutorial.value()
                                         .toObject()
                                         .value(QStringLiteral("decks"))
                                         .toArray();
        for (const QJsonValue& value : decks) {
            const QJsonObject object = value.toObject();
            const int deck = object.value(QStringLiteral("deck")).toInt();
            const QString trackId =
                    object.value(QStringLiteral("track")).toString();
            const double startSeconds =
                    object.value(QStringLiteral("startSeconds")).toDouble(-1.0);
            const auto track = tracks.constFind(trackId);
            if (deck < 1 || deck > 4 || assignedDecks.contains(deck) ||
                    track == tracks.constEnd() || startSeconds < 0.0 ||
                    startSeconds >= track->durationSeconds) {
                setError(pError,
                        QStringLiteral("Invalid deck assignment in tutorial: %1")
                                .arg(tutorial.key()));
                return catalog;
            }
            assignedDecks.insert(deck);
            assignments.append({deck,
                    track->id,
                    track->title,
                    track->artist,
                    track->location,
                    track->license,
                    track->bpm,
                    track->durationSeconds,
                    startSeconds});
        }
        if (assignments.isEmpty()) {
            setError(pError,
                    QStringLiteral("Tutorial has no deck assignments: %1")
                            .arg(tutorial.key()));
            return catalog;
        }
        catalog.m_assignments.insert(tutorial.key(), assignments);
    }
    if (catalog.m_assignments.isEmpty()) {
        setError(pError, QStringLiteral("Tutorial catalog contains no tutorials"));
        return catalog;
    }

    catalog.m_valid = true;
    setError(pError, {});
    return catalog;
}

QList<TrackAssignment> Catalog::assignmentsFor(
        const QString& tutorialId) const {
    return m_assignments.value(tutorialId);
}

QString Catalog::assignmentSummary(const QString& tutorialId) const {
    QStringList values;
    for (const TrackAssignment& assignment : assignmentsFor(tutorialId)) {
        const int seconds = qRound(assignment.startSeconds);
        values.append(QStringLiteral("D%1  %2  @ %3:%4")
                              .arg(assignment.deck)
                              .arg(assignment.title)
                              .arg(seconds / 60, 2, 10, QLatin1Char('0'))
                              .arg(seconds % 60, 2, 10, QLatin1Char('0')));
    }
    return values.join(QStringLiteral("   |   "));
}

} // namespace mixxx::tutorial
