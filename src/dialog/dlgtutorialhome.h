#pragma once

#include <QString>
#include <QList>
#include <QWidget>

class QLabel;
class QVBoxLayout;

class TutorialHomePage final : public QWidget {
    Q_OBJECT

  public:
    explicit TutorialHomePage(QWidget* parent = nullptr);

  signals:
    void openDjWorkspaceRequested(const QString& tutorialId);

  private slots:
    void showUpdateStatus();
    void updateSectionArrow(bool expanded);

  private:
    struct TutorialEntry {
        QString id;
        QString title;
    };

    void addTutorialSection(QVBoxLayout* pLayout,
            const QString& title,
            const QString& description,
            const QList<TutorialEntry>& tutorials);

    QLabel* m_pUpdateStatus;
};
