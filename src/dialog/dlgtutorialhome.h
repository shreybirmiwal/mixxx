#pragma once

#include <QDialog>
#include <QString>
#include <QStringList>

class QVBoxLayout;

class DlgTutorialHome final : public QDialog {
    Q_OBJECT

  public:
    explicit DlgTutorialHome(QWidget* parent = nullptr);

  private slots:
    void showUpdateStatus();
    void updateSectionArrow(bool expanded);

  private:
    void addTutorialSection(QVBoxLayout* pLayout,
            const QString& title,
            const QString& description,
            const QStringList& tutorials);
};
