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
            state.pWidget->setHidden(state.wasHidden);
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
    QList<WidgetSelector> selectors;
    selectors.reserve(hiddenWidgets.size());
    for (const QJsonValue& value : hiddenWidgets) {
        WidgetSelector selector;
        if (value.isString()) {
            selector.objectName = value.toString();
        } else if (value.isObject()) {
            const QJsonObject object = value.toObject();
            selector.objectName = object.value(QStringLiteral("objectName")).toString();
            selector.within = object.value(QStringLiteral("within")).toString();
        }
        if (!selector.objectName.isEmpty()) {
            selectors.append(selector);
        }
    }
    return selectors;
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
        if (pScope->objectName() == selector.objectName) {
            matches.insert(pScope);
        }
        const QList<QWidget*> children =
                pScope->findChildren<QWidget*>(selector.objectName);
        for (QWidget* pChild : children) {
            matches.insert(pChild);
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
            m_widgetStates.append({QPointer<QWidget>(pWidget), pWidget->isHidden()});
            pWidget->hide();
        }
    }
}

} // namespace mixxx::tutorial
