#pragma once

#include <QList>
#include <QPointer>
#include <QString>

class QWidget;

namespace mixxx::tutorial {

struct WidgetSelector {
    QString objectName;
    QString within;
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

    static QList<WidgetSelector> loadProfile(const QString& filePath,
            const QString& profileId,
            QString* pError = nullptr);

  private:
    struct WidgetState {
        QPointer<QWidget> pWidget;
        bool wasHidden;
    };

    void hideMatchingWidgets(const WidgetSelector& selector);

    QWidget* m_pSkinRoot;
    QList<WidgetState> m_widgetStates;
};

} // namespace mixxx::tutorial
