#pragma once

#include <QList>
#include <QString>

#include "preferences/configobject.h"

namespace mixxx::tutorial {

struct RecordedStep {
    qint64 timestampMs{0};
    QString group;
    QString control;
    double value{0.0};
    QString action;
    QString instruction;
};

class Recording final {
  public:
    static Recording create(const QString& title);
    static Recording load(const QString& path, QString* pError = nullptr);

    bool save(const QString& path, QString* pError = nullptr) const;
    bool isValid() const {
        return !m_id.isEmpty();
    }

    const QString& id() const {
        return m_id;
    }
    const QString& title() const {
        return m_title;
    }
    void setTitle(const QString& title) {
        m_title = title;
    }
    qint64 durationMs() const {
        return m_durationMs;
    }
    void setDurationMs(qint64 durationMs);
    const QList<RecordedStep>& steps() const {
        return m_steps;
    }
    QList<RecordedStep>& steps() {
        return m_steps;
    }

    int capture(qint64 timestampMs, const ConfigKey& key, double value);
    int addNote(qint64 timestampMs);
    void sortSteps();

    static bool shouldCapture(const ConfigKey& key, double value);
    static QString describeAction(const ConfigKey& key, double value);

  private:
    QString m_id;
    QString m_title;
    QString m_createdAt;
    qint64 m_durationMs{0};
    QList<RecordedStep> m_steps;
};

} // namespace mixxx::tutorial
