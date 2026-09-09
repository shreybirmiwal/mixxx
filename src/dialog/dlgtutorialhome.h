#pragma once

#include <QString>
#include <QStringList>
#include <QWidget>

class QLabel;
class QVBoxLayout;

class TutorialHomePage final : public QWidget {
    Q_OBJECT

  public:
    explicit TutorialHomePage(QWidget* parent = nullptr);

  signals:
    void openDjWorkspaceRequested();

  private slots:
    void showUpdateStatus();
    void updateSectionArrow(bool expanded);

  private:
    void addTutorialSection(QVBoxLayout* pLayout,
            const QString& title,
            const QString& description,
            const QStringList& tutorials);

    QLabel* m_pUpdateStatus;
};
