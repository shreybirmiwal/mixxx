#pragma once

#include <QList>
#include <QPointer>
#include <QSet>
#include <QSizePolicy>
#include <QString>

class QWidget;

namespace mixxx::tutorial {

struct WidgetSelector {
    QString objectName;
    QString within;
    QString tooltipId;
    QString controlKey;
    QString widgetType;

    bool isValid() const {
        return !objectName.isEmpty() || !tooltipId.isEmpty() ||
                !controlKey.isEmpty() || !widgetType.isEmpty();
    }
};

/// Applies builder-defined visibility profiles to an already loaded Mixxx skin.
/// Every changed widget is restored to its previous hidden state before another
/// profile is applied or the tutorial returns to the home page.
class VisibilityController final {
  public:
    explicit VisibilityController(QWidget* pSkinRoot);

    bool applyProfile(const QString& filePath,
            const QString& profileId,
            QString* pError = nullptr);
    void restore();

    /// Builder/admin controls for changing a loaded skin without affecting its
    /// geometry. Enabling a nested widget also enables its ancestors so the
    /// requested item can actually be seen.
    QList<QWidget*> controllableWidgets() const;
    bool isControllableWidget(QWidget* pWidget) const;
    bool isWidgetExplicitlyVisible(QWidget* pWidget) const;
    void setWidgetVisible(QWidget* pWidget, bool visible);
    void setWidgetTreeVisible(QWidget* pWidget, bool visible);
    void setAllWidgetsVisible(bool visible);

    static QList<WidgetSelector> loadProfile(const QString& filePath,
            const QString& profileId,
            QString* pError = nullptr);

  private:
    struct WidgetState {
        QPointer<QWidget> pWidget;
        QSizePolicy previousSizePolicy;
        bool wasHidden;
    };

    bool matchesSelector(QWidget* pWidget, const WidgetSelector& selector) const;
    void hideMatchingWidgets(const WidgetSelector& selector);
    void applyHiddenState(QWidget* pWidget);
    void restoreAppliedStates();
    void refreshHiddenStates();

    QWidget* m_pSkinRoot;
    QSet<QWidget*> m_hiddenWidgets;
    QList<WidgetState> m_widgetStates;
};

} // namespace mixxx::tutorial
