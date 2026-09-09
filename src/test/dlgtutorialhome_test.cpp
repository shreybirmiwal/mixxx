#include "dialog/dlgtutorialhome.h"

#include <gtest/gtest.h>

#include <QFrame>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>

namespace {

template<typename T>
QList<T*> childrenWithProperty(QWidget* pParent, const char* property) {
    QList<T*> matches;
    for (T* pChild : pParent->findChildren<T*>()) {
        if (pChild->property(property).toBool()) {
            matches.append(pChild);
        }
    }
    return matches;
}

} // namespace

TEST(DlgTutorialHomeTest, ContainsScrollableTutorialSectionsAndActions) {
    DlgTutorialHome home;

    EXPECT_NE(home.findChild<QScrollArea*>(QStringLiteral("tutorialScrollArea")), nullptr);
    EXPECT_NE(home.findChild<QPushButton*>(QStringLiteral("updatesButton")), nullptr);
    EXPECT_NE(home.findChild<QPushButton*>(QStringLiteral("freePlayButton")), nullptr);
    EXPECT_EQ(childrenWithProperty<QToolButton>(&home, "sectionHeader").size(), 3);
    EXPECT_EQ(childrenWithProperty<QPushButton>(&home, "tutorialCard").size(), 10);
}

TEST(DlgTutorialHomeTest, SectionHeadersToggleTheirContent) {
    DlgTutorialHome home;
    const auto headers = childrenWithProperty<QToolButton>(&home, "sectionHeader");
    const auto bodies = childrenWithProperty<QFrame>(&home, "sectionBody");
    ASSERT_FALSE(headers.isEmpty());
    ASSERT_EQ(headers.size(), bodies.size());

    EXPECT_FALSE(bodies.first()->isHidden());
    headers.first()->setChecked(false);
    EXPECT_TRUE(bodies.first()->isHidden());
    headers.first()->setChecked(true);
    EXPECT_FALSE(bodies.first()->isHidden());
}

TEST(DlgTutorialHomeTest, FreePlayAndTutorialsOpenTheDjWorkspace) {
    DlgTutorialHome freePlayHome;
    auto* pFreePlay =
            freePlayHome.findChild<QPushButton*>(QStringLiteral("freePlayButton"));
    ASSERT_NE(pFreePlay, nullptr);
    pFreePlay->click();
    EXPECT_EQ(freePlayHome.result(), QDialog::Accepted);

    DlgTutorialHome tutorialHome;
    const auto tutorials =
            childrenWithProperty<QPushButton>(&tutorialHome, "tutorialCard");
    ASSERT_FALSE(tutorials.isEmpty());
    tutorials.first()->click();
    EXPECT_EQ(tutorialHome.result(), QDialog::Accepted);
}
