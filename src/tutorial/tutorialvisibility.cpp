#include "tutorial/tutorialvisibility.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QWidget>
#include <utility>

#include "control/controlobject.h"
#include "widget/wbasewidget.h"

namespace mixxx::tutorial {

VisibilityController::VisibilityController(QWidget* pSkinRoot)
        : m_pSkinRoot(pSkinRoot) {
    const QList<QWidget*> widgets = controllableWidgets();
    for (QWidget* pWidget : widgets) {
        if (pWidget == m_pSkinRoot || pWidget->isVisibleTo(m_pSkinRoot)) {
            m_initiallyVisibleWidgets.insert(pWidget);
        }
    }
}

VisibilityController::~VisibilityController() {
    restore();
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
    if (!applyForcedControlStates(filePath, profileId, &error)) {
        if (pError) {
            *pError = error;
        }
        restore();
        return false;
    }
    refreshHiddenStates();
    return true;
}

void VisibilityController::restore() {
    restoreAppliedStates();
    releaseFrozenControls();
    restoreForcedControlStates();
    m_hiddenWidgets.clear();
    m_collapsedWidgets.clear();
}

bool VisibilityController::applyForcedControlStates(const QString& filePath,
        const QString& profileId,
        QString* pError) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        if (pError) {
            *pError = QStringLiteral("Cannot open tutorial visibility profiles: %1")
                              .arg(filePath);
        }
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (pError) {
            *pError = QStringLiteral("Invalid tutorial visibility JSON: %1")
                              .arg(parseError.errorString());
        }
        return false;
    }
    const QJsonObject profile = document.object()
                                        .value(QStringLiteral("profiles"))
                                        .toObject()
                                        .value(profileId)
                                        .toObject();
    const QJsonArray controlStates =
            profile.value(QStringLiteral("controlStates")).toArray();
    for (const QJsonValue& value : controlStates) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();
        const ConfigKey key = ConfigKey::parseCommaSeparated(
                object.value(QStringLiteral("controlKey")).toString());
        if (key.group.isEmpty() || key.item.isEmpty() ||
                !object.value(QStringLiteral("value")).isDouble() ||
                !ControlObject::exists(key)) {
            continue;
        }
        ControlObject* pControl =
                ControlObject::getControl(key, ControlFlag::NoWarnIfMissing);
        if (!pControl) {
            continue;
        }
        if (!m_previousControlValues.contains(key)) {
            m_previousControlValues.insert(key, pControl->get());
        }
        const double forcedValue =
                object.value(QStringLiteral("value")).toDouble();
        m_forcedControlValues.insert(key, forcedValue);
        pControl->setFrozenAtDefault(false);
        pControl->setAndConfirm(forcedValue);
    }
    return true;
}

void VisibilityController::restoreForcedControlStates() {
    for (auto it = m_previousControlValues.constBegin();
            it != m_previousControlValues.constEnd();
            ++it) {
        ControlObject* pControl =
                ControlObject::getControl(it.key(), ControlFlag::NoWarnIfMissing);
        if (pControl) {
            pControl->setFrozenAtDefault(false);
            pControl->setAndConfirm(it.value());
        }
    }
    m_previousControlValues.clear();
    m_forcedControlValues.clear();
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
            m_collapsedWidgets.remove(pCurrent);
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
            m_collapsedWidgets.remove(pCurrent);
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
            m_collapsedWidgets.remove(pCurrent);
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
    m_collapsedWidgets.clear();
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
            selector.retainSpace =
                    object.value(QStringLiteral("retainSpace")).toBool(true);
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
        if (!selector.retainSpace) {
            m_collapsedWidgets.insert(pWidget);
        }
    }
}

void VisibilityController::applyHiddenState(QWidget* pWidget) {
    const QSizePolicy previousSizePolicy = pWidget->sizePolicy();
    m_widgetStates.append(
            {QPointer<QWidget>(pWidget), previousSizePolicy, pWidget->isHidden()});
    QSizePolicy retainedSizePolicy = previousSizePolicy;
    retainedSizePolicy.setRetainSizeWhenHidden(
            !m_collapsedWidgets.contains(pWidget));
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
    refreshFrozenControls();
}

bool VisibilityController::isEffectivelyHidden(QWidget* pWidget) const {
    for (QWidget* pCurrent = pWidget; pCurrent;
            pCurrent = pCurrent->parentWidget()) {
        if (m_hiddenWidgets.contains(pCurrent)) {
            return true;
        }
        if (pCurrent == m_pSkinRoot) {
            break;
        }
    }
    return false;
}

void VisibilityController::refreshFrozenControls() {
    QHash<ConfigKey, int> editorCounts;
    QHash<ConfigKey, int> hiddenEditorCounts;
    for (QWidget* pWidget : std::as_const(m_initiallyVisibleWidgets)) {
        auto* pBaseWidget = dynamic_cast<WBaseWidget*>(pWidget);
        if (!pBaseWidget) {
            continue;
        }
        const QList<ConfigKey> keys = pBaseWidget->inputControlKeys();
        for (const ConfigKey& key : keys) {
            ++editorCounts[key];
            if (isEffectivelyHidden(pWidget)) {
                ++hiddenEditorCounts[key];
            }
        }
    }

    QSet<ConfigKey> keysToFreeze;
    for (auto it = editorCounts.constBegin(); it != editorCounts.constEnd(); ++it) {
        if (hiddenEditorCounts.value(it.key()) == it.value() &&
                !m_forcedControlValues.contains(it.key()) &&
                ControlObject::exists(it.key())) {
            keysToFreeze.insert(it.key());
        }
    }

    const QSet<ConfigKey> keysToRelease = m_frozenControlKeys - keysToFreeze;
    for (const ConfigKey& key : keysToRelease) {
        ControlObject* pControl =
                ControlObject::getControl(key, ControlFlag::NoWarnIfMissing);
        if (pControl) {
            pControl->setFrozenAtDefault(false);
        }
    }
    const QSet<ConfigKey> newKeys = keysToFreeze - m_frozenControlKeys;
    for (const ConfigKey& key : newKeys) {
        ControlObject* pControl =
                ControlObject::getControl(key, ControlFlag::NoWarnIfMissing);
        if (pControl) {
            pControl->setFrozenAtDefault(true);
        }
    }
    m_frozenControlKeys = keysToFreeze;

    for (auto it = m_forcedControlValues.constBegin();
            it != m_forcedControlValues.constEnd();
            ++it) {
        ControlObject* pControl =
                ControlObject::getControl(it.key(), ControlFlag::NoWarnIfMissing);
        if (pControl) {
            pControl->setFrozenAtDefault(false);
            pControl->setAndConfirm(it.value());
        }
    }
}

void VisibilityController::releaseFrozenControls() {
    for (const ConfigKey& key : std::as_const(m_frozenControlKeys)) {
        ControlObject* pControl =
                ControlObject::getControl(key, ControlFlag::NoWarnIfMissing);
        if (pControl) {
            pControl->setFrozenAtDefault(false);
        }
    }
    m_frozenControlKeys.clear();
}

} // namespace mixxx::tutorial
