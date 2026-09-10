#include "dialog/dlgtutorialhome.h"

#include <gtest/gtest.h>

#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalSpy>
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

TEST(TutorialHomePageTest, ContainsScrollableTutorialSectionsAndActions) {
    TutorialHomePage home;

    EXPECT_NE(home.findChild<QScrollArea*>(QStringLiteral("tutorialScrollArea")), nullptr);
    EXPECT_NE(home.findChild<QPushButton*>(QStringLiteral("updatesButton")), nullptr);
    EXPECT_NE(home.findChild<QPushButton*>(QStringLiteral("freePlayButton")), nullptr);
    EXPECT_NE(home.findChild<QPushButton*>(QStringLiteral("createTutorialButton")), nullptr);
    EXPECT_EQ(childrenWithProperty<QToolButton>(&home, "sectionHeader").size(), 4);
    const auto tutorials =
            childrenWithProperty<QFrame>(&home, "lessonCard");
    EXPECT_EQ(tutorials.size(), 10);
    QStringList tutorialIds;
    for (QFrame* pTutorial : tutorials) {
        tutorialIds.append(pTutorial->property("tutorialId").toString());
    }
    EXPECT_TRUE(tutorialIds.contains(QStringLiteral("level-zero")));
    EXPECT_TRUE(tutorialIds.contains(QStringLiteral("bass-eq")));
    EXPECT_TRUE(tutorialIds.contains(QStringLiteral("beatmatching")));
    EXPECT_TRUE(tutorialIds.contains(QStringLiteral("looping")));
    EXPECT_TRUE(tutorialIds.contains(QStringLiteral("channel-faders")));
    EXPECT_TRUE(tutorialIds.contains(QStringLiteral("filter-sweep")));
    EXPECT_EQ(childrenWithProperty<QPushButton>(&home, "startLesson").size(), 8);
    EXPECT_EQ(childrenWithProperty<QLabel>(&home, "comingSoon").size(), 2);
}

TEST(TutorialHomePageTest, CreateTutorialRequestsTheRecorder) {
    TutorialHomePage home;
    QSignalSpy spy(&home, &TutorialHomePage::createTutorialRequested);
    auto* pCreate =
            home.findChild<QPushButton*>(QStringLiteral("createTutorialButton"));
    ASSERT_NE(pCreate, nullptr);
    pCreate->click();
    EXPECT_EQ(spy.count(), 1);
}

TEST(TutorialHomePageTest, SectionHeadersToggleTheirContent) {
    TutorialHomePage home;
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

TEST(TutorialHomePageTest, FreePlayAndTutorialsRequestTheDjWorkspace) {
    TutorialHomePage freePlayHome;
    QSignalSpy freePlaySpy(
            &freePlayHome, &TutorialHomePage::openDjWorkspaceRequested);
    auto* pFreePlay =
            freePlayHome.findChild<QPushButton*>(QStringLiteral("freePlayButton"));
    ASSERT_NE(pFreePlay, nullptr);
    pFreePlay->click();
    EXPECT_EQ(freePlaySpy.count(), 1);
    const QList<QVariant> freePlayRequest = freePlaySpy.takeFirst();
    EXPECT_TRUE(freePlayRequest.at(0).toString().isEmpty());

    TutorialHomePage tutorialHome;
    QSignalSpy tutorialSpy(
            &tutorialHome, &TutorialHomePage::openDjWorkspaceRequested);
    const auto tutorials =
            childrenWithProperty<QPushButton>(&tutorialHome, "startLesson");
    ASSERT_FALSE(tutorials.isEmpty());
    const QString tutorialId = tutorials.first()->property("tutorialId").toString();
    EXPECT_FALSE(tutorialId.isEmpty());
    tutorials.first()->click();
    EXPECT_EQ(tutorialSpy.count(), 1);
    const QList<QVariant> learnRequest = tutorialSpy.takeFirst();
    EXPECT_EQ(learnRequest.at(0).toString(), tutorialId);
}

TEST(TutorialHomePageTest, UpdatesAreReportedInsideThePage) {
    TutorialHomePage home;
    auto* pUpdates = home.findChild<QPushButton*>(QStringLiteral("updatesButton"));
    auto* pStatus = home.findChild<QLabel*>(QStringLiteral("updateStatus"));
    ASSERT_NE(pUpdates, nullptr);
    ASSERT_NE(pStatus, nullptr);

    EXPECT_TRUE(pStatus->isHidden());
    pUpdates->click();
    EXPECT_FALSE(pStatus->isHidden());
    EXPECT_FALSE(pStatus->text().isEmpty());
}
