#include "tutorial/tutorialvisibilitypanel.h"

#include <QAbstractItemView>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QWidget>

#include "tutorial/tutorialvisibility.h"

namespace mixxx::tutorial {

VisibilityPanel::VisibilityPanel(QWidget* parent)
        : QDockWidget(tr("Admin: lesson visibility"), parent) {
    setObjectName(QStringLiteral("TutorialVisibilityAdmin"));
    setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable |
            QDockWidget::DockWidgetFloatable);
    setMinimumWidth(390);

    auto* pContainer = new QWidget(this);
    auto* pLayout = new QVBoxLayout(pContainer);
    pLayout->setContentsMargins(10, 10, 10, 10);
    pLayout->setSpacing(8);

    auto* pIntro = new QLabel(
            tr("Builder mode: uncheck anything to remove it without changing "
               "the original layout. Checking an item turns its parent path on."),
            pContainer);
    pIntro->setWordWrap(true);
    pLayout->addWidget(pIntro);

    auto* pButtonRow = new QHBoxLayout();
    auto* pAllOn = new QPushButton(tr("All on"), pContainer);
    auto* pAllOff = new QPushButton(tr("All off"), pContainer);
    pAllOn->setObjectName(QStringLiteral("tutorialAdminAllOn"));
    pAllOff->setObjectName(QStringLiteral("tutorialAdminAllOff"));
    pButtonRow->addWidget(pAllOn);
    pButtonRow->addWidget(pAllOff);
    pButtonRow->addStretch();
    pLayout->addLayout(pButtonRow);

    m_pSearch = new QLineEdit(pContainer);
    m_pSearch->setObjectName(QStringLiteral("tutorialAdminSearch"));
    m_pSearch->setPlaceholderText(tr("Search name, tooltip, key, or type…"));
    pLayout->addWidget(m_pSearch);

    m_pCountLabel = new QLabel(pContainer);
    pLayout->addWidget(m_pCountLabel);

    m_pTree = new QTreeWidget(pContainer);
    m_pTree->setObjectName(QStringLiteral("tutorialAdminWidgetTree"));
    m_pTree->setColumnCount(2);
    m_pTree->setHeaderLabels({tr("On / item"), tr("Skin metadata")});
    m_pTree->setAlternatingRowColors(true);
    m_pTree->setUniformRowHeights(true);
    m_pTree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_pTree->header()->resizeSection(0, 205);
    pLayout->addWidget(m_pTree, 1);

    setWidget(pContainer);
    setStyleSheet(QStringLiteral(
            "QDockWidget { color: #f6f4ff; }"
            "QDockWidget::title { background: #171420; padding: 8px; font-weight: 700; }"
            "QWidget { background: #111018; color: #eeeaf8; }"
            "QLineEdit, QTreeWidget { background: #09080d; border: 1px solid #393344; }"
            "QPushButton { background: #6847ed; border: 0; border-radius: 5px; "
            "padding: 6px 12px; font-weight: 700; }"
            "QPushButton:hover { background: #795af2; }"));

    connect(pAllOn, &QPushButton::clicked, this, [this] {
        if (m_pController) {
            m_pController->setAllWidgetsVisible(true);
            updateChecks();
        }
    });
    connect(pAllOff, &QPushButton::clicked, this, [this] {
        if (m_pController) {
            m_pController->setAllWidgetsVisible(false);
            updateChecks();
        }
    });
    connect(m_pSearch, &QLineEdit::textChanged, this, [this](const QString& query) {
        for (int i = 0; i < m_pTree->topLevelItemCount(); ++i) {
            applyFilter(m_pTree->topLevelItem(i), query.trimmed());
        }
    });
    connect(m_pTree,
            &QTreeWidget::itemChanged,
            this,
            [this](QTreeWidgetItem* pItem, int column) {
                if (m_updating || column != 0 || !m_pController) {
                    return;
                }
                QWidget* pWidget = m_widgets.value(pItem);
                if (!pWidget) {
                    return;
                }
                m_pController->setWidgetTreeVisible(
                        pWidget, pItem->checkState(0) == Qt::Checked);
                updateChecks();
            });
}

void VisibilityPanel::setController(VisibilityController* pController) {
    m_pController = pController;
    m_pSkinRoot = pController ? pController->controllableWidgets().value(0) : nullptr;
    rebuild();
}

void VisibilityPanel::rebuild() {
    m_updating = true;
    m_widgets.clear();
    m_pTree->clear();
    if (m_pController && m_pSkinRoot) {
        addWidgetTree(m_pSkinRoot, nullptr);
    }
    m_pCountLabel->setText(tr("%1 controllable items").arg(m_widgets.size()));
    m_pTree->expandToDepth(1);
    m_updating = false;
    updateChecks();
}

void VisibilityPanel::addWidgetTree(
        QWidget* pWidget, QTreeWidgetItem* pParentItem) {
    if (!pWidget || !m_pController) {
        return;
    }

    QTreeWidgetItem* pNextParent = pParentItem;
    if (m_pController->isControllableWidget(pWidget)) {
        auto* pItem = pParentItem ? new QTreeWidgetItem(pParentItem)
                                  : new QTreeWidgetItem(m_pTree);
        pItem->setText(0, widgetLabel(pWidget));
        pItem->setText(1, widgetDetails(pWidget));
        pItem->setFlags(pItem->flags() | Qt::ItemIsUserCheckable);
        pItem->setCheckState(0,
                m_pController->isWidgetExplicitlyVisible(pWidget) ? Qt::Checked
                                                                  : Qt::Unchecked);
        pItem->setToolTip(0, pItem->text(0));
        pItem->setToolTip(1, pItem->text(1));
        m_widgets.insert(pItem, pWidget);
        pNextParent = pItem;
    }

    const QList<QWidget*> children =
            pWidget->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget* pChild : children) {
        addWidgetTree(pChild, pNextParent);
    }
}

void VisibilityPanel::updateChecks() {
    if (!m_pController) {
        return;
    }
    m_updating = true;
    const QSignalBlocker blocker(m_pTree);
    for (auto it = m_widgets.constBegin(); it != m_widgets.constEnd(); ++it) {
        if (it.value()) {
            it.key()->setCheckState(0,
                    m_pController->isWidgetExplicitlyVisible(it.value()) ? Qt::Checked
                                                                         : Qt::Unchecked);
        }
    }
    m_updating = false;
}

bool VisibilityPanel::applyFilter(QTreeWidgetItem* pItem, const QString& query) {
    bool childMatches = false;
    for (int i = 0; i < pItem->childCount(); ++i) {
        childMatches = applyFilter(pItem->child(i), query) || childMatches;
    }
    const bool selfMatches = query.isEmpty() ||
            pItem->text(0).contains(query, Qt::CaseInsensitive) ||
            pItem->text(1).contains(query, Qt::CaseInsensitive);
    const bool matches = selfMatches || childMatches;
    pItem->setHidden(!matches);
    if (!query.isEmpty() && childMatches) {
        pItem->setExpanded(true);
    }
    return matches;
}

QString VisibilityPanel::widgetLabel(QWidget* pWidget) const {
    if (pWidget == m_pSkinRoot) {
        return tr("Workspace");
    }
    if (!pWidget->objectName().isEmpty()) {
        return pWidget->objectName();
    }
    const QString tooltipId = pWidget->property("mixxxTooltipId").toString();
    if (!tooltipId.isEmpty()) {
        return tooltipId;
    }
    const QStringList controlKeys =
            pWidget->property("mixxxControlKeys").toStringList();
    if (!controlKeys.isEmpty()) {
        return controlKeys.first();
    }
    const QString widgetType =
            pWidget->property("mixxxSkinWidgetType").toString();
    return widgetType.isEmpty() ? tr("Unnamed widget") : widgetType;
}

QString VisibilityPanel::widgetDetails(QWidget* pWidget) const {
    QStringList details;
    const QString widgetType =
            pWidget->property("mixxxSkinWidgetType").toString();
    const QString tooltipId = pWidget->property("mixxxTooltipId").toString();
    const QStringList controlKeys =
            pWidget->property("mixxxControlKeys").toStringList();
    if (!widgetType.isEmpty()) {
        details.append(widgetType);
    }
    if (!tooltipId.isEmpty()) {
        details.append(QStringLiteral("tip: %1").arg(tooltipId));
    }
    if (!controlKeys.isEmpty()) {
        details.append(controlKeys.join(QStringLiteral(", ")));
    }
    return details.join(QStringLiteral(" · "));
}

} // namespace mixxx::tutorial
