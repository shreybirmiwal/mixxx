#pragma once

#include <QDockWidget>
#include <QHash>
#include <QPointer>

class QLabel;
class QLineEdit;
class QTreeWidget;
class QTreeWidgetItem;
class QWidget;

namespace mixxx::tutorial {

class VisibilityController;

/// Temporary builder UI for inspecting and changing every loaded skin widget.
class VisibilityPanel final : public QDockWidget {
  public:
    explicit VisibilityPanel(QWidget* parent = nullptr);

    void setController(VisibilityController* pController);

  private:
    void rebuild();
    void addWidgetTree(QWidget* pWidget, QTreeWidgetItem* pParentItem);
    void updateChecks();
    bool applyFilter(QTreeWidgetItem* pItem, const QString& query);
    QString widgetLabel(QWidget* pWidget) const;
    QString widgetDetails(QWidget* pWidget) const;

    VisibilityController* m_pController{nullptr};
    QWidget* m_pSkinRoot{nullptr};
    QLineEdit* m_pSearch{nullptr};
    QLabel* m_pCountLabel{nullptr};
    QTreeWidget* m_pTree{nullptr};
    QHash<QTreeWidgetItem*, QPointer<QWidget>> m_widgets;
    bool m_updating{false};
};

} // namespace mixxx::tutorial
