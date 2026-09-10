#include "tutorial/tutorialcatalog.h"

#include <gtest/gtest.h>

#include <QFile>
#include <QTemporaryDir>

namespace {

QString writeCatalog(QTemporaryDir* pDirectory, const QByteArray& assignment) {
    QFile audio(pDirectory->filePath(QStringLiteral("track.mp3")));
    if (!audio.open(QIODevice::WriteOnly)) {
        return {};
    }
    audio.write("audio");
    audio.close();

    QFile catalog(pDirectory->filePath(QStringLiteral("catalog.json")));
    if (!catalog.open(QIODevice::WriteOnly)) {
        return {};
    }
    catalog.write(QByteArray(R"({
        "schemaVersion": 1,
        "tracks": [{
            "id": "training", "title": "Training Track",
            "artist": "LeetDJ", "file": "track.mp3",
            "license": "CC0-1.0", "bpm": 124,
            "durationSeconds": 60
        }],
        "tutorials": { "first": { "decks": [)") +
            assignment + R"(] } }
    })");
    catalog.close();
    return catalog.fileName();
}

} // namespace

TEST(TutorialCatalogTest, ResolvesFixedTrackAndTimestamp) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeCatalog(&directory,
            R"({ "deck": 1, "track": "training", "startSeconds": 16 })");

    QString error;
    const auto catalog = mixxx::tutorial::Catalog::load(path, &error);
    EXPECT_TRUE(error.isEmpty());
    ASSERT_TRUE(catalog.isValid());
    const auto assignments = catalog.assignmentsFor(QStringLiteral("first"));
    ASSERT_EQ(assignments.size(), 1);
    EXPECT_EQ(assignments.first().deck, 1);
    EXPECT_EQ(assignments.first().title, QStringLiteral("Training Track"));
    EXPECT_DOUBLE_EQ(assignments.first().startSeconds, 16.0);
    EXPECT_EQ(catalog.assignmentSummary(QStringLiteral("first")),
            QStringLiteral("D1  Training Track  @ 00:16"));
}

TEST(TutorialCatalogTest, RejectsUnknownTrack) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeCatalog(&directory,
            R"({ "deck": 1, "track": "personal-song", "startSeconds": 0 })");

    QString error;
    const auto catalog = mixxx::tutorial::Catalog::load(path, &error);
    EXPECT_FALSE(catalog.isValid());
    EXPECT_FALSE(error.isEmpty());
}

TEST(TutorialCatalogTest, RejectsTimestampOutsideTrack) {
    QTemporaryDir directory;
    ASSERT_TRUE(directory.isValid());
    const QString path = writeCatalog(&directory,
            R"({ "deck": 1, "track": "training", "startSeconds": 60 })");

    QString error;
    const auto catalog = mixxx::tutorial::Catalog::load(path, &error);
    EXPECT_FALSE(catalog.isValid());
    EXPECT_FALSE(error.isEmpty());
}
