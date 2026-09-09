#include "tutorial/tutorialvisibility.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QWidget>
#include <utility>

namespace mixxx::tutorial {

VisibilityController::VisibilityController(QWidget* pSkinRoot)
        : m_pSkinRoot(pSkinRoot) {
}

bool VisibilityController::applyProfile(const QString& filePath,
        const QString& profileId,
        QString* pError) {
    restore();
    if (profileId.isEmpty()) {
        return true;
    }

    QString error;
    const QList<WidgetSelector> selectors = loadProfile(filePath, profileId, &error);
    if (!error.isEmpty()) {
        if (pError) {
            *pError = error;
        }
        return false;
    }

    for (const WidgetSelector& selector : selectors) {
        hideMatchingWidgets(selector);
    }
    return true;
}

void VisibilityController::restore() {
    for (const WidgetState& state : std::as_const(m_widgetStates)) {
        if (state.pWidget) {
            if (state.pOpacityEffect) {
                if (state.ownsOpacityEffect &&
                        state.pWidget->graphicsEffect() == state.pOpacityEffect) {
                    state.pWidget->setGraphicsEffect(nullptr);
                } else {
                    state.pOpacityEffect->setOpacity(state.previousOpacity);
                }
            }
            state.pWidget->setEnabled(state.wasEnabled);
            state.pWidget->setAttribute(
                    Qt::WA_TransparentForMouseEvents, !state.acceptedMouseEvents);
        }
    }
    m_widgetStates.clear();
}

QList<WidgetSelector> VisibilityController::loadProfile(const QString& filePath,
        const QString& profileId,
        QString* pError) {
    if (pError) {
        pError->clear();
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (pError) {
            *pError = QStringLiteral("Cannot open tutorial visibility profiles: %1")
                              .arg(filePath);
        }
        return {};
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (pError) {
            *pError = QStringLiteral("Invalid tutorial visibility JSON: %1")
                              .arg(parseError.errorString());
        }
        return {};
    }

    const QJsonObject profiles = document.object().value(QStringLiteral("profiles")).toObject();
    if (!profiles.contains(profileId)) {
        if (pError) {
            *pError = QStringLiteral("Tutorial visibility profile '%1' was not found")
                              .arg(profileId);
        }
        return {};
    }

    const QJsonArray hiddenWidgets =
            profiles.value(profileId).toObject().value(QStringLiteral("hiddenWidgets")).toArray();
    const QJsonArray widgetStates =
            profiles.value(profileId).toObject().value(QStringLiteral("widgetStates")).toArray();
    QList<WidgetSelector> selectors;
    selectors.reserve(hiddenWidgets.size() + widgetStates.size());
    const auto appendSelector = [&selectors](const QJsonValue& value) {
        WidgetSelector selector;
        if (value.isString()) {
            selector.objectName = value.toString();
        } else if (value.isObject()) {
            const QJsonObject object = value.toObject();
            selector.objectName = object.value(QStringLiteral("objectName")).toString();
            selector.within = object.value(QStringLiteral("within")).toString();
            selector.tooltipId = object.value(QStringLiteral("tooltipId")).toString();
            selector.controlKey = object.value(QStringLiteral("controlKey")).toString();
            selector.widgetType = object.value(QStringLiteral("widgetType")).toString();
        }
        if (selector.isValid()) {
            selectors.append(selector);
        }
    };
    for (const QJsonValue& value : hiddenWidgets) {
        appendSelector(value);
    }
    for (const QJsonValue& value : widgetStates) {
        if (value.isObject() &&
                !value.toObject().value(QStringLiteral("visible")).toBool(true)) {
            appendSelector(value);
        }
    }
    return selectors;
}

bool VisibilityController::matchesSelector(
        QWidget* pWidget, const WidgetSelector& selector) const {
    if (!selector.objectName.isEmpty() &&
            pWidget->objectName() != selector.objectName) {
        return false;
    }
    if (!selector.tooltipId.isEmpty() &&
            pWidget->property("mixxxTooltipId").toString() != selector.tooltipId) {
        return false;
    }
    if (!selector.controlKey.isEmpty() &&
            !pWidget->property("mixxxControlKeys")
                     .toStringList()
                     .contains(selector.controlKey)) {
        return false;
    }
    if (!selector.widgetType.isEmpty() &&
            pWidget->property("mixxxSkinWidgetType").toString() != selector.widgetType) {
        return false;
    }
    return true;
}

void VisibilityController::hideMatchingWidgets(const WidgetSelector& selector) {
    if (!m_pSkinRoot) {
        return;
    }

    QList<QWidget*> scopes;
    if (selector.within.isEmpty()) {
        scopes.append(m_pSkinRoot);
    } else {
        if (m_pSkinRoot->objectName() == selector.within) {
            scopes.append(m_pSkinRoot);
        }
        scopes.append(m_pSkinRoot->findChildren<QWidget*>(selector.within));
    }

    QSet<QWidget*> matches;
    for (QWidget* pScope : std::as_const(scopes)) {
        if (matchesSelector(pScope, selector)) {
            matches.insert(pScope);
        }
        const QList<QWidget*> children = pScope->findChildren<QWidget*>();
        for (QWidget* pChild : children) {
            if (matchesSelector(pChild, selector)) {
                matches.insert(pChild);
            }
        }
    }

    for (QWidget* pWidget : std::as_const(matches)) {
        bool alreadyTracked = false;
        for (const WidgetState& state : std::as_const(m_widgetStates)) {
            if (state.pWidget == pWidget) {
                alreadyTracked = true;
                break;
            }
        }
        if (!alreadyTracked) {
            auto* pOpacityEffect =
                    qobject_cast<QGraphicsOpacityEffect*>(pWidget->graphicsEffect());
            bool ownsOpacityEffect = false;
            if (!pOpacityEffect && pWidget->graphicsEffect()) {
                qWarning() << "Cannot visually hide widget with an existing graphics effect:"
                           << pWidget->objectName();
                continue;
            }
            if (!pOpacityEffect) {
                pOpacityEffect = new QGraphicsOpacityEffect(pWidget);
                pWidget->setGraphicsEffect(pOpacityEffect);
                ownsOpacityEffect = true;
            }
            m_widgetStates.append({QPointer<QWidget>(pWidget),
                    QPointer<QGraphicsOpacityEffect>(pOpacityEffect),
                    pOpacityEffect->opacity(),
                    ownsOpacityEffect,
                    pWidget->isEnabled(),
                    !pWidget->testAttribute(Qt::WA_TransparentForMouseEvents)});
            pOpacityEffect->setOpacity(0.0);
            pWidget->setEnabled(false);
            pWidget->setAttribute(Qt::WA_TransparentForMouseEvents, true);
        }
    }
}

} // namespace mixxx::tutorial
