#include "tutorial/tutorialrecording.h"

#include <algorithm>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>
#include <QUuid>

namespace mixxx::tutorial {
namespace {

void setError(QString* pError, const QString& error) {
    if (pError) {
        *pError = error;
    }
}

QString friendlyGroup(const QString& group) {
    static const QRegularExpression channelExpression(
            QStringLiteral("\\[Channel(\\d+)\\]"));
    const auto channelMatch = channelExpression.match(group);
    if (channelMatch.hasMatch()) {
        return QStringLiteral("Deck %1").arg(channelMatch.captured(1));
    }
    QString value = group;
    value.remove(QLatin1Char('['));
    value.remove(QLatin1Char(']'));
    return value;
}

QString friendlyControl(const ConfigKey& key) {
    const QString& control = key.item;
    if (key.group.startsWith(QStringLiteral("[EqualizerRack"))) {
        static const QHash<QString, QString> equalizerNames{
                {QStringLiteral("parameter1"), QStringLiteral("Low EQ")},
                {QStringLiteral("parameter2"), QStringLiteral("Mid EQ")},
                {QStringLiteral("parameter3"), QStringLiteral("High EQ")},
        };
        const auto equalizerName = equalizerNames.constFind(control);
        if (equalizerName != equalizerNames.constEnd()) {
            return *equalizerName;
        }
    }
    if (key.group.startsWith(QStringLiteral("[QuickEffectRack")) &&
            control == QStringLiteral("super1")) {
        return QStringLiteral("Filter / quick effect");
    }
    static const QHash<QString, QString> names{
            {QStringLiteral("play"), QStringLiteral("Play")},
            {QStringLiteral("cue_default"), QStringLiteral("Cue")},
            {QStringLiteral("rate"), QStringLiteral("Tempo fader")},
            {QStringLiteral("volume"), QStringLiteral("Channel fader")},
            {QStringLiteral("pregain"), QStringLiteral("Gain")},
            {QStringLiteral("filterHigh"), QStringLiteral("High EQ")},
            {QStringLiteral("filterMid"), QStringLiteral("Mid EQ")},
            {QStringLiteral("filterLow"), QStringLiteral("Low EQ")},
            {QStringLiteral("quick_effect_super1"), QStringLiteral("Quick effect")},
            {QStringLiteral("pfl"), QStringLiteral("Headphone cue")},
            {QStringLiteral("sync_enabled"), QStringLiteral("Sync")},
            {QStringLiteral("crossfader"), QStringLiteral("Crossfader")},
            {QStringLiteral("headMix"), QStringLiteral("Headphone mix")},
            {QStringLiteral("headGain"), QStringLiteral("Headphone level")},
            {QStringLiteral("loop_enabled"), QStringLiteral("Loop")},
            {QStringLiteral("loop_in"), QStringLiteral("Loop in")},
            {QStringLiteral("loop_out"), QStringLiteral("Loop out")},
            {QStringLiteral("LoadSelectedTrack"), QStringLiteral("Load selected track")},
            {QStringLiteral("MoveVertical"), QStringLiteral("Browse tracks")},
            {QStringLiteral("MoveFocusForward"), QStringLiteral("Move library focus")},
            {QStringLiteral("MoveFocusBackward"), QStringLiteral("Move library focus back")},
    };
    const auto known = names.constFind(control);
    if (known != names.constEnd()) {
        return *known;
    }
    QString value = control;
    value.replace(QLatin1Char('_'), QLatin1Char(' '));
    if (!value.isEmpty()) {
        value[0] = value[0].toUpper();
    }
    return value;
}

bool isMomentary(const QString& control) {
    return control.startsWith(QStringLiteral("cue_")) ||
            control == QStringLiteral("loop_in") ||
            control == QStringLiteral("loop_out") ||
            control == QStringLiteral("reloop_toggle") ||
            control == QStringLiteral("LoadSelectedTrack") ||
            control.startsWith(QStringLiteral("MoveFocus")) ||
            control.startsWith(QStringLiteral("beatjump")) ||
            (control.startsWith(QStringLiteral("hotcue_")) &&
                    control.endsWith(QStringLiteral("_activate")));
}

bool isContinuous(const QString& control) {
    return control == QStringLiteral("rate") ||
            control == QStringLiteral("volume") ||
            control == QStringLiteral("pregain") ||
            control == QStringLiteral("filterHigh") ||
            control == QStringLiteral("filterMid") ||
            control == QStringLiteral("filterLow") ||
            control == QStringLiteral("quick_effect_super1") ||
            control == QStringLiteral("crossfader") ||
            control == QStringLiteral("headMix") ||
            control == QStringLiteral("headGain") ||
            control == QStringLiteral("super1") ||
            control.startsWith(QStringLiteral("parameter"));
}

} // namespace

Recording Recording::create(const QString& title) {
    Recording recording;
    recording.m_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    recording.m_title = title;
    recording.m_createdAt = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    return recording;
}

void Recording::setDurationMs(qint64 durationMs) {
    m_durationMs = qMax<qint64>(0, durationMs);
}

bool Recording::shouldCapture(const ConfigKey& key, double value) {
    const bool deck = key.group.startsWith(QStringLiteral("[Channel"));
    const bool mixer = key.group == QStringLiteral("[Master]");
    const bool effects = key.group.startsWith(QStringLiteral("[EffectRack"));
    const bool equalizer = key.group.startsWith(QStringLiteral("[EqualizerRack"));
    const bool quickEffect = key.group.startsWith(QStringLiteral("[QuickEffectRack"));
    const bool sampler = key.group.startsWith(QStringLiteral("[Sampler"));
    const bool library = key.group == QStringLiteral("[Library]");
    if (!deck && !mixer && !effects && !equalizer && !quickEffect && !sampler &&
            !library) {
        return false;
    }
    if (isMomentary(key.item)) {
        return value > 0.0;
    }
    if (effects) {
        return key.item.contains(QStringLiteral("enabled"), Qt::CaseInsensitive) ||
                key.item.contains(QStringLiteral("meta"), Qt::CaseInsensitive) ||
                key.item.contains(QStringLiteral("parameter"), Qt::CaseInsensitive);
    }
    if (equalizer) {
        return key.item == QStringLiteral("parameter1") ||
                key.item == QStringLiteral("parameter2") ||
                key.item == QStringLiteral("parameter3");
    }
    if (quickEffect) {
        return key.item == QStringLiteral("super1");
    }
    if (sampler) {
        return key.item == QStringLiteral("play") ||
                key.item == QStringLiteral("volume") ||
                key.item.startsWith(QStringLiteral("hotcue_"));
    }
    if (library) {
        return key.item == QStringLiteral("MoveVertical") ||
                key.item.startsWith(QStringLiteral("MoveFocus"));
    }
    static const QSet<QString> controls{
            QStringLiteral("play"),
            QStringLiteral("rate"),
            QStringLiteral("volume"),
            QStringLiteral("pregain"),
            QStringLiteral("filterHigh"),
            QStringLiteral("filterMid"),
            QStringLiteral("filterLow"),
            QStringLiteral("quick_effect_super1"),
            QStringLiteral("pfl"),
            QStringLiteral("sync_enabled"),
            QStringLiteral("quantize"),
            QStringLiteral("slip_enabled"),
            QStringLiteral("reverse"),
            QStringLiteral("loop_enabled"),
            QStringLiteral("reloop_toggle"),
            QStringLiteral("crossfader"),
            QStringLiteral("headMix"),
            QStringLiteral("headGain"),
            QStringLiteral("LoadSelectedTrack"),
    };
    return controls.contains(key.item) || key.item.startsWith(QStringLiteral("cue_")) ||
            key.item.startsWith(QStringLiteral("loop_")) ||
            key.item.startsWith(QStringLiteral("hotcue_")) ||
            key.item.startsWith(QStringLiteral("beatjump"));
}

QString Recording::describeAction(const ConfigKey& key, double value) {
    const QString target = QStringLiteral("%1 %2")
                                   .arg(friendlyGroup(key.group),
                                           friendlyControl(key));
    if (isMomentary(key.item)) {
        return QStringLiteral("Press %1").arg(target);
    }
    if (isContinuous(key.item)) {
        return QStringLiteral("Move %1 to %2")
                .arg(target, QString::number(value, 'f', 2));
    }
    return QStringLiteral("Turn %1 %2").arg(target, value > 0.0 ?
                    QStringLiteral("on") : QStringLiteral("off"));
}

int Recording::capture(qint64 timestampMs, const ConfigKey& key, double value) {
    if (!shouldCapture(key, value)) {
        return -1;
    }
    timestampMs = qMax<qint64>(0, timestampMs);
    const QString action = describeAction(key, value);
    if (!m_steps.isEmpty()) {
        RecordedStep& previous = m_steps.last();
        if (previous.group == key.group && previous.control == key.item &&
                timestampMs - previous.timestampMs <= 650) {
            previous.value = value;
            previous.action = action;
            m_durationMs = qMax(m_durationMs, timestampMs);
            return m_steps.size() - 1;
        }
    }
    m_steps.append({timestampMs,
            key.group,
            key.item,
            value,
            action,
            QString()});
    m_durationMs = qMax(m_durationMs, timestampMs);
    return m_steps.size() - 1;
}

int Recording::addNote(qint64 timestampMs) {
    m_steps.append({qMax<qint64>(0, timestampMs),
            QString(),
            QString(),
            0.0,
            QStringLiteral("Instructor note"),
            QString()});
    sortSteps();
    for (int index = 0; index < m_steps.size(); ++index) {
        const RecordedStep& step = m_steps.at(index);
        if (step.timestampMs == timestampMs && step.group.isEmpty()) {
            return index;
        }
    }
    return -1;
}

void Recording::sortSteps() {
    std::stable_sort(m_steps.begin(), m_steps.end(), [](const auto& left, const auto& right) {
        return left.timestampMs < right.timestampMs;
    });
}

bool Recording::save(const QString& path, QString* pError) const {
    QJsonArray steps;
    for (const RecordedStep& step : m_steps) {
        steps.append(QJsonObject{{QStringLiteral("timestampMs"), step.timestampMs},
                {QStringLiteral("group"), step.group},
                {QStringLiteral("control"), step.control},
                {QStringLiteral("value"), step.value},
                {QStringLiteral("action"), step.action},
                {QStringLiteral("instruction"), step.instruction}});
    }
    const QJsonObject root{{QStringLiteral("schemaVersion"), 1},
            {QStringLiteral("id"), m_id},
            {QStringLiteral("title"), m_title},
            {QStringLiteral("createdAt"), m_createdAt},
            {QStringLiteral("updatedAt"), QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)},
            {QStringLiteral("durationMs"), m_durationMs},
            {QStringLiteral("steps"), steps}};
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        setError(pError, QStringLiteral("Cannot create tutorial folder"));
        return false;
    }
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) ||
            file.write(QJsonDocument(root).toJson(QJsonDocument::Indented)) < 0 ||
            !file.commit()) {
        setError(pError, QStringLiteral("Cannot save tutorial: %1").arg(path));
        return false;
    }
    setError(pError, {});
    return true;
}

Recording Recording::load(const QString& path, QString* pError) {
    Recording recording;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        setError(pError, QStringLiteral("Cannot open tutorial: %1").arg(path));
        return recording;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        setError(pError, QStringLiteral("Invalid tutorial JSON"));
        return recording;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("schemaVersion")).toInt() != 1) {
        setError(pError, QStringLiteral("Unsupported tutorial schema"));
        return recording;
    }
    recording.m_id = root.value(QStringLiteral("id")).toString();
    recording.m_title = root.value(QStringLiteral("title")).toString();
    recording.m_createdAt = root.value(QStringLiteral("createdAt")).toString();
    recording.m_durationMs = root.value(QStringLiteral("durationMs")).toInteger();
    for (const QJsonValue& value : root.value(QStringLiteral("steps")).toArray()) {
        const QJsonObject object = value.toObject();
        recording.m_steps.append({object.value(QStringLiteral("timestampMs")).toInteger(),
                object.value(QStringLiteral("group")).toString(),
                object.value(QStringLiteral("control")).toString(),
                object.value(QStringLiteral("value")).toDouble(),
                object.value(QStringLiteral("action")).toString(),
                object.value(QStringLiteral("instruction")).toString()});
    }
    if (recording.m_id.isEmpty() || recording.m_title.isEmpty()) {
        setError(pError, QStringLiteral("Tutorial is missing an id or title"));
        return {};
    }
    recording.sortSteps();
    setError(pError, {});
    return recording;
}

} // namespace mixxx::tutorial
