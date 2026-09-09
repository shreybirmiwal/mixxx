#include "tutorial/tutorialvisibility.h"

#include <gtest/gtest.h>

#include <QFile>
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
    ASSERT_EQ(selectors.size(), 2);
    EXPECT_EQ(selectors.at(0).objectName, QStringLiteral("ClockWidget"));
    EXPECT_TRUE(selectors.at(0).within.isEmpty());
    EXPECT_EQ(selectors.at(1).objectName, QStringLiteral("PlayDeck"));
    EXPECT_EQ(selectors.at(1).within, QStringLiteral("Deck1"));
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

    controller.restore();
    EXPECT_EQ(clock.isHidden(), clockWasHidden);
    EXPECT_EQ(play1.isHidden(), play1WasHidden);
    EXPECT_EQ(play2.isHidden(), play2WasHidden);
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
