#pragma once

#include <QHash>
#include <QList>
#include <QString>

namespace mixxx::tutorial {

struct TrackAssignment {
    int deck{0};
    QString id;
    QString title;
    QString artist;
    QString location;
    QString license;
    double bpm{0.0};
    double durationSeconds{0.0};
    double startSeconds{0.0};
};

class Catalog final {
  public:
    static Catalog load(const QString& path, QString* pError = nullptr);

    bool isValid() const {
        return m_valid;
    }
    QList<TrackAssignment> assignmentsFor(const QString& tutorialId) const;
    QString assignmentSummary(const QString& tutorialId) const;

  private:
    bool m_valid{false};
    QHash<QString, QList<TrackAssignment>> m_assignments;
};

} // namespace mixxx::tutorial
