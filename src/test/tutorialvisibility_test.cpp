#include "tutorial/tutorialvisibility.h"

#include <gtest/gtest.h>

#include <QFile>
#include <QStringList>
#include <QTemporaryDir>
#include <QWidget>

namespace {

QString writeProfiles(QTemporaryDir* pDirectory) {
    const QString path = pDirectory->filePath(QStringLiteral("profiles.json"));
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return {};
    }
    file.write(R"({
        "profiles": {
            "focused": {
                "hiddenWidgets": [
                    "ClockWidget",
                    { "within": "Deck1", "objectName": "PlayDeck" }
                ],
                "widgetStates": [
                    { "within": "Deck1", "tooltipId": "starrating", "visible": false },
                    { "controlKey": "[Channel1],rate", "visible": false },
                    { "widgetType": "Battery", "visible": true }
                ]
            }
        }
    })");
    file.close();
    return path;
}

} // namespace

TEST(TutorialVisibilityTest, LoadsGlobalAndScopedSelectors) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeProfiles(&directory);
    ASSERT_FALSE(path.isEmpty());

    QString error;
    const auto selectors = mixxx::tutorial::VisibilityController::loadProfile(
            path, QStringLiteral("focused"), &error);

    EXPECT_TRUE(error.isEmpty());
    ASSERT_EQ(selectors.size(), 4);
    EXPECT_EQ(selectors.at(0).objectName, QStringLiteral("ClockWidget"));
    EXPECT_TRUE(selectors.at(0).within.isEmpty());
    EXPECT_EQ(selectors.at(1).objectName, QStringLiteral("PlayDeck"));
    EXPECT_EQ(selectors.at(1).within, QStringLiteral("Deck1"));
    EXPECT_EQ(selectors.at(2).tooltipId, QStringLiteral("starrating"));
    EXPECT_EQ(selectors.at(3).controlKey, QStringLiteral("[Channel1],rate"));
}

TEST(TutorialVisibilityTest, HidesMatchesWithinScopeAndRestoresState) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeProfiles(&directory);
    ASSERT_FALSE(path.isEmpty());

    QWidget skin;
    QWidget clock(&skin);
    clock.setObjectName(QStringLiteral("ClockWidget"));
    QWidget deck1(&skin);
    deck1.setObjectName(QStringLiteral("Deck1"));
    QWidget deck2(&skin);
    deck2.setObjectName(QStringLiteral("Deck2"));
    QWidget play1(&deck1);
    play1.setObjectName(QStringLiteral("PlayDeck"));
    QWidget play2(&deck2);
    play2.setObjectName(QStringLiteral("PlayDeck"));
    QWidget rating(&deck1);
    rating.setProperty("mixxxTooltipId", QStringLiteral("starrating"));
    QWidget rate(&deck1);
    rate.setProperty("mixxxControlKeys", QStringList{QStringLiteral("[Channel1],rate")});

    const bool clockWasHidden = clock.isHidden();
    const bool play1WasHidden = play1.isHidden();
    const bool play2WasHidden = play2.isHidden();

    mixxx::tutorial::VisibilityController controller(&skin);
    QString error;
    ASSERT_TRUE(controller.applyProfile(path, QStringLiteral("focused"), &error));
    EXPECT_TRUE(error.isEmpty());
    EXPECT_TRUE(clock.isHidden());
    EXPECT_TRUE(play1.isHidden());
    EXPECT_EQ(play2.isHidden(), play2WasHidden);
    EXPECT_TRUE(clock.sizePolicy().retainSizeWhenHidden());
    EXPECT_TRUE(play1.sizePolicy().retainSizeWhenHidden());
    EXPECT_TRUE(rating.isHidden());
    EXPECT_TRUE(rate.isHidden());

    controller.restore();
    EXPECT_EQ(clock.isHidden(), clockWasHidden);
    EXPECT_EQ(play1.isHidden(), play1WasHidden);
    EXPECT_EQ(play2.isHidden(), play2WasHidden);
    EXPECT_FALSE(clock.sizePolicy().retainSizeWhenHidden());
    EXPECT_FALSE(play1.sizePolicy().retainSizeWhenHidden());
}

TEST(TutorialVisibilityTest, ReportsMissingProfile) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeProfiles(&directory);
    ASSERT_FALSE(path.isEmpty());

    QString error;
    const auto selectors = mixxx::tutorial::VisibilityController::loadProfile(
            path, QStringLiteral("missing"), &error);
    EXPECT_TRUE(selectors.isEmpty());
    EXPECT_FALSE(error.isEmpty());
}

TEST(TutorialVisibilityTest, AdminCanBlankWorkspaceAndRestoreOneBranch) {
    QWidget skin;
    QWidget deck1(&skin);
    QWidget deck2(&skin);
    QWidget play1(&deck1);
    QWidget play2(&deck2);
    deck1.setProperty("mixxxSkinWidgetType", QStringLiteral("WidgetGroup"));
    deck2.setProperty("mixxxSkinWidgetType", QStringLiteral("WidgetGroup"));
    play1.setProperty("mixxxSkinWidgetType", QStringLiteral("PushButton"));
    play2.setProperty("mixxxSkinWidgetType", QStringLiteral("PushButton"));

    mixxx::tutorial::VisibilityController controller(&skin);
    EXPECT_EQ(controller.controllableWidgets().size(), 5);

    controller.setAllWidgetsVisible(false);
    EXPECT_FALSE(controller.isWidgetExplicitlyVisible(&skin));
    EXPECT_FALSE(controller.isWidgetExplicitlyVisible(&play1));
    EXPECT_TRUE(skin.isHidden());
    EXPECT_TRUE(skin.sizePolicy().retainSizeWhenHidden());

    controller.setWidgetVisible(&play1, true);
    EXPECT_TRUE(controller.isWidgetExplicitlyVisible(&skin));
    EXPECT_TRUE(controller.isWidgetExplicitlyVisible(&deck1));
    EXPECT_TRUE(controller.isWidgetExplicitlyVisible(&play1));
    EXPECT_FALSE(controller.isWidgetExplicitlyVisible(&deck2));
    EXPECT_FALSE(controller.isWidgetExplicitlyVisible(&play2));
    EXPECT_TRUE(deck2.isHidden());
    EXPECT_TRUE(deck2.sizePolicy().retainSizeWhenHidden());

    controller.setWidgetTreeVisible(&deck1, false);
    EXPECT_FALSE(controller.isWidgetExplicitlyVisible(&deck1));
    EXPECT_FALSE(controller.isWidgetExplicitlyVisible(&play1));

    controller.setWidgetTreeVisible(&deck1, true);
    EXPECT_TRUE(controller.isWidgetExplicitlyVisible(&deck1));
    EXPECT_TRUE(controller.isWidgetExplicitlyVisible(&play1));
}
