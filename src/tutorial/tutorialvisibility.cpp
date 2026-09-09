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
    refreshHiddenStates();
    return true;
}

void VisibilityController::restore() {
    restoreAppliedStates();
    m_hiddenWidgets.clear();
}

void VisibilityController::restoreAppliedStates() {
    for (const WidgetState& state : std::as_const(m_widgetStates)) {
        if (state.pWidget) {
            state.pWidget->setSizePolicy(state.previousSizePolicy);
            state.pWidget->setHidden(state.wasHidden);
        }
    }
    m_widgetStates.clear();
}

QList<QWidget*> VisibilityController::controllableWidgets() const {
    QList<QWidget*> widgets;
    if (!m_pSkinRoot) {
        return widgets;
    }
    widgets.append(m_pSkinRoot);
    const QList<QWidget*> children = m_pSkinRoot->findChildren<QWidget*>();
    for (QWidget* pWidget : children) {
        if (isControllableWidget(pWidget)) {
            widgets.append(pWidget);
        }
    }
    return widgets;
}

bool VisibilityController::isControllableWidget(QWidget* pWidget) const {
    if (!pWidget) {
        return false;
    }
    return pWidget == m_pSkinRoot ||
            pWidget->property("mixxxSkinWidgetType").isValid() ||
            pWidget->property("mixxxTooltipId").isValid() ||
            pWidget->property("mixxxControlKeys").isValid();
}

bool VisibilityController::isWidgetExplicitlyVisible(QWidget* pWidget) const {
    return pWidget && !m_hiddenWidgets.contains(pWidget);
}

void VisibilityController::setWidgetVisible(QWidget* pWidget, bool visible) {
    if (!isControllableWidget(pWidget)) {
        return;
    }
    if (visible) {
        // A child cannot be painted through a hidden parent. Turn its
        // ancestor chain on while leaving every sibling's state untouched.
        for (QWidget* pCurrent = pWidget; pCurrent;
                pCurrent = pCurrent->parentWidget()) {
            m_hiddenWidgets.remove(pCurrent);
            if (pCurrent == m_pSkinRoot) {
                break;
            }
        }
    } else {
        m_hiddenWidgets.insert(pWidget);
    }
    refreshHiddenStates();
}

void VisibilityController::setWidgetTreeVisible(QWidget* pWidget, bool visible) {
    if (!isControllableWidget(pWidget)) {
        return;
    }
    if (visible) {
        for (QWidget* pCurrent = pWidget; pCurrent;
                pCurrent = pCurrent->parentWidget()) {
            m_hiddenWidgets.remove(pCurrent);
            if (pCurrent == m_pSkinRoot) {
                break;
            }
        }
    }
    const QList<QWidget*> descendants = pWidget->findChildren<QWidget*>();
    const auto updateWidget = [this, visible](QWidget* pCurrent) {
        if (!isControllableWidget(pCurrent)) {
            return;
        }
        if (visible) {
            m_hiddenWidgets.remove(pCurrent);
        } else {
            m_hiddenWidgets.insert(pCurrent);
        }
    };
    updateWidget(pWidget);
    for (QWidget* pDescendant : descendants) {
        updateWidget(pDescendant);
    }
    refreshHiddenStates();
}

void VisibilityController::setAllWidgetsVisible(bool visible) {
    m_hiddenWidgets.clear();
    if (!visible) {
        const QList<QWidget*> widgets = controllableWidgets();
        for (QWidget* pWidget : widgets) {
            m_hiddenWidgets.insert(pWidget);
        }
    }
    refreshHiddenStates();
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
        m_hiddenWidgets.insert(pWidget);
    }
}

void VisibilityController::applyHiddenState(QWidget* pWidget) {
    const QSizePolicy previousSizePolicy = pWidget->sizePolicy();
    m_widgetStates.append(
            {QPointer<QWidget>(pWidget), previousSizePolicy, pWidget->isHidden()});
    QSizePolicy retainedSizePolicy = previousSizePolicy;
    retainedSizePolicy.setRetainSizeWhenHidden(true);
    pWidget->setSizePolicy(retainedSizePolicy);
    pWidget->hide();
}

void VisibilityController::refreshHiddenStates() {
    restoreAppliedStates();
    for (QWidget* pWidget : std::as_const(m_hiddenWidgets)) {
        if (!pWidget) {
            continue;
        }
        bool hasHiddenAncestor = false;
        for (QWidget* pParent = pWidget->parentWidget(); pParent;
                pParent = pParent->parentWidget()) {
            if (m_hiddenWidgets.contains(pParent)) {
                hasHiddenAncestor = true;
                break;
            }
            if (pParent == m_pSkinRoot) {
                break;
            }
        }
        if (!hasHiddenAncestor) {
            applyHiddenState(pWidget);
        }
    }
}

} // namespace mixxx::tutorial
