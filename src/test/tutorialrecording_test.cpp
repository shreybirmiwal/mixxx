#include "tutorial/tutorialrecording.h"

#include <gtest/gtest.h>

#include <QTemporaryDir>

TEST(TutorialRecordingTest, CapturesAndCoalescesControllerGestures) {
    auto recording = mixxx::tutorial::Recording::create(
            QStringLiteral("Controller lesson"));
    const ConfigKey lowEq(QStringLiteral("[Channel1]"),
            QStringLiteral("filterLow"));

    EXPECT_EQ(recording.capture(1000, lowEq, 0.7), 0);
    EXPECT_EQ(recording.capture(1200, lowEq, 0.2), 0);
    ASSERT_EQ(recording.steps().size(), 1);
    EXPECT_EQ(recording.steps().first().timestampMs, 1000);
    EXPECT_DOUBLE_EQ(recording.steps().first().value, 0.2);
    EXPECT_TRUE(recording.steps().first().action.contains(
            QStringLiteral("Deck 1 Low EQ")));

    EXPECT_EQ(recording.capture(2000, lowEq, 0.5), 1);
    EXPECT_EQ(recording.steps().size(), 2);
}

TEST(TutorialRecordingTest, IgnoresPlaybackTelemetryAndButtonReleases) {
    auto recording = mixxx::tutorial::Recording::create(
            QStringLiteral("Filtered lesson"));

    EXPECT_EQ(recording.capture(100,
                      ConfigKey(QStringLiteral("[Channel1]"),
                              QStringLiteral("playposition")),
                      0.4),
            -1);
    EXPECT_EQ(recording.capture(200,
                      ConfigKey(QStringLiteral("[Channel1]"),
                              QStringLiteral("cue_default")),
                      0.0),
            -1);
    EXPECT_EQ(recording.capture(300,
                      ConfigKey(QStringLiteral("[Channel1]"),
                              QStringLiteral("cue_default")),
                      1.0),
            0);
    EXPECT_EQ(recording.steps().size(), 1);
}

TEST(TutorialRecordingTest, CapturesFlx4EqAndFilterMappings) {
    auto recording = mixxx::tutorial::Recording::create(
            QStringLiteral("FLX4 lesson"));

    EXPECT_EQ(recording.capture(100,
                      ConfigKey(QStringLiteral("[EqualizerRack1_[Channel1]_Effect1]"),
                              QStringLiteral("parameter1")),
                      0.25),
            0);
    EXPECT_EQ(recording.capture(1000,
                      ConfigKey(QStringLiteral("[QuickEffectRack1_[Channel2]]"),
                              QStringLiteral("super1")),
                      0.75),
            1);
    ASSERT_EQ(recording.steps().size(), 2);
    EXPECT_TRUE(recording.steps().at(0).action.contains(
            QStringLiteral("Deck 1 Low EQ")));
    EXPECT_TRUE(recording.steps().at(1).action.contains(
            QStringLiteral("Deck 2 Filter / quick effect")));
}

TEST(TutorialRecordingTest, CapturesFlx4BrowseAndLoadMappings) {
    auto recording = mixxx::tutorial::Recording::create(
            QStringLiteral("FLX4 library lesson"));

    EXPECT_EQ(recording.capture(100,
                      ConfigKey(QStringLiteral("[Library]"),
                              QStringLiteral("MoveVertical")),
                      1.0),
            0);
    EXPECT_EQ(recording.capture(1000,
                      ConfigKey(QStringLiteral("[Channel1]"),
                              QStringLiteral("LoadSelectedTrack")),
                      1.0),
            1);
    ASSERT_EQ(recording.steps().size(), 2);
    EXPECT_TRUE(recording.steps().at(0).action.contains(
            QStringLiteral("Library Browse tracks")));
    EXPECT_TRUE(recording.steps().at(1).action.contains(
            QStringLiteral("Deck 1 Load selected track")));
}

TEST(TutorialRecordingTest, SavesAndLoadsEditableSteps) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("lesson.json"));
    auto recording = mixxx::tutorial::Recording::create(
            QStringLiteral("Saved lesson"));
    recording.capture(1250,
            ConfigKey(QStringLiteral("[Master]"),
                    QStringLiteral("crossfader")),
            0.5);
    recording.steps().first().instruction =
            QStringLiteral("Hold the blend in the middle.");

    QString error;
    ASSERT_TRUE(recording.save(path, &error)) << error.toStdString();
    const auto loaded = mixxx::tutorial::Recording::load(path, &error);
    ASSERT_TRUE(loaded.isValid()) << error.toStdString();
    EXPECT_EQ(loaded.title(), QStringLiteral("Saved lesson"));
    ASSERT_EQ(loaded.steps().size(), 1);
    EXPECT_EQ(loaded.steps().first().timestampMs, 1250);
    EXPECT_EQ(loaded.steps().first().instruction,
            QStringLiteral("Hold the blend in the middle."));
}
