#include "dialog/dlgtutorialhome.h"

#include <QCoreApplication>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

#include "moc_dlgtutorialhome.cpp"
#include "util/parented_ptr.h"

namespace {
constexpr int kHomeWidth = 1080;
constexpr int kHomeHeight = 760;
constexpr int kHomeMinimumWidth = 760;
constexpr int kHomeMinimumHeight = 560;
} // namespace

TutorialHomePage::TutorialHomePage(QWidget* parent)
        : QWidget(parent),
          m_pUpdateStatus(nullptr) {
    setObjectName(QStringLiteral("tutorialHomePage"));
    setMinimumSize(kHomeMinimumWidth, kHomeMinimumHeight);
    resize(kHomeWidth, kHomeHeight);

    setStyleSheet(QStringLiteral(R"(
        QWidget#tutorialHomePage {
            background: #0b0d12;
            color: #f7f8fb;
        }
        QLabel#brand {
            color: #a78bfa;
            font-size: 13px;
            font-weight: 800;
            letter-spacing: 2px;
        }
        QLabel#title {
            color: #ffffff;
            font-size: 38px;
            font-weight: 800;
        }
        QLabel#subtitle {
            color: #a5adbd;
            font-size: 16px;
        }
        QPushButton#updatesButton {
            background: #191d27;
            border: 1px solid #303645;
            border-radius: 10px;
            color: #e8eaf0;
            font-size: 14px;
            font-weight: 700;
            padding: 10px 16px;
        }
        QPushButton#updatesButton:hover {
            background: #232938;
            border-color: #7c5cff;
        }
        QLabel#updateStatus {
            color: #b9c1d1;
            font-size: 13px;
            padding: 8px 0;
        }
        QFrame#freePlayCard {
            background: #161a23;
            border: 1px solid #303746;
            border-radius: 18px;
        }
        QLabel#freePlayTitle {
            color: #ffffff;
            font-size: 22px;
            font-weight: 800;
        }
        QLabel#freePlayDescription {
            color: #aab2c2;
            font-size: 14px;
        }
        QPushButton#freePlayButton {
            background: #6847ed;
            border: 1px solid #896fff;
            border-radius: 11px;
            color: #ffffff;
            font-size: 15px;
            font-weight: 800;
            padding: 12px 20px;
        }
        QPushButton#freePlayButton:hover {
            background: #795af2;
        }
        QScrollArea {
            background: transparent;
            border: 0;
        }
        QWidget#scrollContent {
            background: transparent;
        }
        QToolButton[sectionHeader="true"] {
            background: #141821;
            border: 1px solid #262c39;
            border-radius: 14px;
            color: #ffffff;
            font-size: 18px;
            font-weight: 800;
            padding: 16px 18px;
            text-align: left;
        }
        QToolButton[sectionHeader="true"]:hover {
            background: #1a1f2b;
            border-color: #6847ed;
        }
        QFrame[sectionBody="true"] {
            background: #10131a;
            border: 1px solid #202632;
            border-radius: 14px;
        }
        QFrame[lessonCard="true"] {
            background: #191d27;
            border: 1px solid #292f3c;
            border-radius: 12px;
        }
        QLabel[levelBadge="true"] {
            background: #2b2152;
            border: 1px solid #6847ed;
            border-radius: 9px;
            color: #cfc2ff;
            font-size: 12px;
            font-weight: 900;
            padding: 7px 9px;
        }
        QLabel[lessonTitle="true"] {
            color: #ffffff;
            font-size: 16px;
            font-weight: 800;
        }
        QLabel[lessonDescription="true"] {
            color: #aab2c2;
            font-size: 13px;
        }
        QLabel[lessonMeta="true"] {
            color: #a78bfa;
            font-size: 12px;
            font-weight: 750;
        }
        QPushButton[startLesson="true"] {
            background: #6847ed;
            border: 1px solid #896fff;
            border-radius: 9px;
            color: white;
            font-size: 13px;
            font-weight: 800;
            padding: 10px 15px;
        }
        QPushButton[startLesson="true"]:hover {
            background: #795af2;
        }
        QLabel[comingSoon="true"] {
            background: #202530;
            border: 1px solid #343b4a;
            border-radius: 8px;
            color: #7f899a;
            font-size: 12px;
            font-weight: 750;
            padding: 8px 11px;
        }
    )"));

    auto pRootLayout = make_parented<QVBoxLayout>(this);
    pRootLayout->setContentsMargins(32, 28, 32, 28);
    pRootLayout->setSpacing(22);

    auto pTopBarWidget = make_parented<QWidget>(this);
    auto pTopBar = make_parented<QHBoxLayout>(pTopBarWidget);
    pTopBar->setContentsMargins(0, 0, 0, 0);
    auto pBrand = make_parented<QLabel>(tr("MIXXX  /  LEARN"), pTopBarWidget);
    pBrand->setObjectName(QStringLiteral("brand"));
    pTopBar->addWidget(pBrand);
    pTopBar->addStretch();

    auto pUpdatesButton = make_parented<QPushButton>(tr("Updates"), pTopBarWidget);
    pUpdatesButton->setObjectName(QStringLiteral("updatesButton"));
    pUpdatesButton->setAccessibleName(tr("Updates"));
    pUpdatesButton->setCursor(Qt::PointingHandCursor);
    connect(pUpdatesButton,
            &QPushButton::clicked,
            this,
            &TutorialHomePage::showUpdateStatus);
    pTopBar->addWidget(pUpdatesButton);
    pRootLayout->addWidget(pTopBarWidget);

    auto pUpdateStatus = make_parented<QLabel>(this);
    m_pUpdateStatus = pUpdateStatus.get();
    m_pUpdateStatus->setObjectName(QStringLiteral("updateStatus"));
    m_pUpdateStatus->setWordWrap(true);
    m_pUpdateStatus->hide();
    pRootLayout->addWidget(pUpdateStatus);

    auto pTitle = make_parented<QLabel>(tr("Learn to DJ, one skill at a time"), this);
    pTitle->setObjectName(QStringLiteral("title"));
    pRootLayout->addWidget(pTitle);

    auto pSubtitle = make_parented<QLabel>(
            tr("A step-by-step DJ course. Each lesson watches what you do, checks the skill, and automatically moves forward when you get it right."), this);
    pSubtitle->setObjectName(QStringLiteral("subtitle"));
    pSubtitle->setWordWrap(true);
    pRootLayout->addWidget(pSubtitle);

    auto pFreePlayCard = make_parented<QFrame>(this);
    pFreePlayCard->setObjectName(QStringLiteral("freePlayCard"));
    auto pFreePlayLayout = make_parented<QHBoxLayout>(pFreePlayCard);
    pFreePlayLayout->setContentsMargins(22, 18, 18, 18);
    pFreePlayLayout->setSpacing(18);

    auto pFreePlayCopyWidget = make_parented<QWidget>(pFreePlayCard);
    auto pFreePlayCopy = make_parented<QVBoxLayout>(pFreePlayCopyWidget);
    pFreePlayCopy->setContentsMargins(0, 0, 0, 0);
    pFreePlayCopy->setSpacing(4);
    auto pFreePlayTitle = make_parented<QLabel>(tr("Free Play"), pFreePlayCard);
    pFreePlayTitle->setObjectName(QStringLiteral("freePlayTitle"));
    auto pFreePlayDescription = make_parented<QLabel>(
            tr("Open the full DJ workspace with no guided steps."), pFreePlayCard);
    pFreePlayDescription->setObjectName(QStringLiteral("freePlayDescription"));
    pFreePlayCopy->addWidget(pFreePlayTitle);
    pFreePlayCopy->addWidget(pFreePlayDescription);
    pFreePlayLayout->addWidget(pFreePlayCopyWidget, 1);

    auto pFreePlayButton = make_parented<QPushButton>(tr("Open decks  →"), pFreePlayCard);
    pFreePlayButton->setObjectName(QStringLiteral("freePlayButton"));
    pFreePlayButton->setAccessibleName(tr("Free Play"));
    pFreePlayButton->setCursor(Qt::PointingHandCursor);
    connect(pFreePlayButton,
            &QPushButton::clicked,
            this,
            [this] {
                emit openDjWorkspaceRequested(QString());
            });
    pFreePlayLayout->addWidget(pFreePlayButton);
    pRootLayout->addWidget(pFreePlayCard);

    auto pScrollArea = make_parented<QScrollArea>(this);
    pScrollArea->setObjectName(QStringLiteral("tutorialScrollArea"));
    pScrollArea->setWidgetResizable(true);
    pScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto pScrollContent = make_parented<QWidget>(pScrollArea);
    pScrollContent->setObjectName(QStringLiteral("scrollContent"));
    auto pTutorialLayout = make_parented<QVBoxLayout>(pScrollContent);
    pTutorialLayout->setContentsMargins(0, 0, 8, 0);
    pTutorialLayout->setSpacing(14);

    addTutorialSection(pTutorialLayout,
            tr("1  ·  Foundations"),
            tr("Load music and understand the two-deck workspace."),
            {{QStringLiteral("level-zero"), QStringLiteral("01"),
                     tr("Your first mix"),
                     tr("Load two songs, read the waveforms, press play, and move the essential faders."),
                     tr("DECK BASICS"), tr("6 MIN"), true}});
    addTutorialSection(pTutorialLayout,
            tr("2  ·  Mixing controls"),
            tr("Shape volume and frequency to move between songs."),
            {{QStringLiteral("crossfader"), QStringLiteral("02"),
                     tr("Crossfader control"),
                     tr("Hear the left, center, and right positions, then perform a smooth blend."),
                     tr("BLENDING"), tr("5 MIN"), true},
                    {QStringLiteral("bass-eq"), QStringLiteral("03"),
                            tr("EQ and filter knobs"),
                            tr("Learn what HIGH, MID, LOW, and the filter remove from a song."),
                            tr("TONE CONTROL"), tr("7 MIN"), true}});
    addTutorialSection(pTutorialLayout,
            tr("3  ·  Timing and preparation"),
            tr("Prepare the next track and make both songs move together."),
            {{QStringLiteral("cueing"), QStringLiteral("04"),
                     tr("Cue the next track"),
                     tr("Preview Deck 2, set a cue point, and return to it before the mix."),
                     tr("PREPARATION"), tr("6 MIN"), true},
                    {QStringLiteral("beatmatching"), QStringLiteral("05"),
                            tr("Match the tempo"),
                            tr("Use SYNC and the tempo fader to understand BPM alignment."),
                            tr("RHYTHM"), tr("7 MIN"), true},
                    {QStringLiteral("looping"), QStringLiteral("06"),
                            tr("Build and release a loop"),
                            tr("Count a phrase, create a loop, resize it, exit, and reloop."),
                            tr("PHRASING"), tr("7 MIN"), true}});
    addTutorialSection(pTutorialLayout,
            tr("4  ·  Creative transitions"),
            tr("Combine the fundamentals into repeatable performance moves."),
            {{QStringLiteral("filter-sweep"), QStringLiteral("07"),
                     tr("Filter sweep transition"),
                     tr("Clear frequency space while bringing the next track into the mix."),
                     tr("TRANSITION"), tr("COMING NEXT"), false},
                    {QStringLiteral("echo-out"), QStringLiteral("08"),
                            tr("Echo-out transition"),
                            tr("Exit a song cleanly at the end of a musical phrase."),
                            tr("TRANSITION"), tr("COMING NEXT"), false},
                    {QStringLiteral("starships-one-more-time"), QStringLiteral("09"),
                            tr("Wordplay: Starships × One More Time"),
                            tr("Use matching words and phrase timing to connect two recognizable moments."),
                            tr("WORDPLAY"), tr("COMING NEXT"), false}});

    pTutorialLayout->addStretch();
    pScrollArea->setWidget(pScrollContent);
    pRootLayout->addWidget(pScrollArea, 1);
}

void TutorialHomePage::addTutorialSection(QVBoxLayout* pLayout,
        const QString& title,
        const QString& description,
        const QList<TutorialEntry>& tutorials) {
    const bool expandedByDefault = pLayout->count() == 0;
    auto pSection = make_parented<QFrame>(pLayout->parentWidget());
    auto pSectionLayout = make_parented<QVBoxLayout>(pSection);
    pSectionLayout->setContentsMargins(0, 0, 0, 0);
    pSectionLayout->setSpacing(8);

    auto pHeader = make_parented<QToolButton>(pSection);
    pHeader->setObjectName(QStringLiteral("tutorialSectionHeader"));
    pHeader->setProperty("sectionHeader", true);
    pHeader->setAccessibleName(title);
    pHeader->setAccessibleDescription(description);
    pHeader->setText(QStringLiteral("%1\n%2").arg(title, description));
    pHeader->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    pHeader->setArrowType(
            expandedByDefault ? Qt::DownArrow : Qt::RightArrow);
    pHeader->setCheckable(true);
    pHeader->setChecked(expandedByDefault);
    pHeader->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    pHeader->setCursor(Qt::PointingHandCursor);
    pSectionLayout->addWidget(pHeader);

    auto pBody = make_parented<QFrame>(pSection);
    pBody->setObjectName(QStringLiteral("tutorialSectionBody"));
    pBody->setProperty("sectionBody", true);
    pBody->setVisible(expandedByDefault);
    auto pBodyLayout = make_parented<QVBoxLayout>(pBody);
    pBodyLayout->setContentsMargins(12, 12, 12, 12);
    pBodyLayout->setSpacing(9);

    for (const TutorialEntry& tutorial : tutorials) {
        auto pCard = make_parented<QFrame>(pBody);
        pCard->setProperty("lessonCard", true);
        pCard->setProperty("tutorialId", tutorial.id);
        auto pCardLayout = make_parented<QHBoxLayout>(pCard);
        pCardLayout->setContentsMargins(14, 11, 11, 11);
        pCardLayout->setSpacing(13);

        auto pLevel = make_parented<QLabel>(tutorial.level, pCard);
        pLevel->setProperty("levelBadge", true);
        pLevel->setAlignment(Qt::AlignCenter);
        pCardLayout->addWidget(pLevel);

        auto pCopyWidget = make_parented<QWidget>(pCard);
        auto pCopy = make_parented<QVBoxLayout>(pCopyWidget);
        pCopy->setContentsMargins(0, 0, 0, 0);
        pCopy->setSpacing(3);
        auto pTitle = make_parented<QLabel>(tutorial.title, pCopyWidget);
        pTitle->setProperty("lessonTitle", true);
        auto pDescription =
                make_parented<QLabel>(tutorial.description, pCopyWidget);
        pDescription->setProperty("lessonDescription", true);
        pDescription->setWordWrap(true);
        auto pMeta = make_parented<QLabel>(
                QStringLiteral("%1   ·   %2").arg(tutorial.skill, tutorial.duration),
                pCopyWidget);
        pMeta->setProperty("lessonMeta", true);
        pCopy->addWidget(pTitle);
        pCopy->addWidget(pDescription);
        pCopy->addWidget(pMeta);
        pCardLayout->addWidget(pCopyWidget, 1);

        if (tutorial.available) {
            auto pStart = make_parented<QPushButton>(tr("Start lesson  →"), pCard);
            pStart->setObjectName(QStringLiteral("startLessonButton"));
            pStart->setProperty("startLesson", true);
            pStart->setProperty("tutorialId", tutorial.id);
            pStart->setAccessibleName(tr("Start: %1").arg(tutorial.title));
            pStart->setCursor(Qt::PointingHandCursor);
            connect(pStart,
                    &QPushButton::clicked,
                    this,
                    [this, tutorialId = tutorial.id] {
                        emit openDjWorkspaceRequested(tutorialId);
                    });
            pCardLayout->addWidget(pStart);
        } else {
            auto pComingSoon = make_parented<QLabel>(tr("Coming soon"), pCard);
            pComingSoon->setProperty("comingSoon", true);
            pCardLayout->addWidget(pComingSoon);
        }
        pBodyLayout->addWidget(pCard);
    }
    pSectionLayout->addWidget(pBody);

    connect(pHeader, &QToolButton::toggled, pBody, &QWidget::setVisible);
    connect(pHeader,
            &QToolButton::toggled,
            this,
            &TutorialHomePage::updateSectionArrow);
    pLayout->addWidget(pSection);
}

void TutorialHomePage::showUpdateStatus() {
    m_pUpdateStatus->setText(
            tr("You are running the local Mixxx %1 build. Pull the fork's upstream "
               "remote and rebuild to install source updates.")
                    .arg(QCoreApplication::applicationVersion()));
    m_pUpdateStatus->show();
}

void TutorialHomePage::updateSectionArrow(bool expanded) {
    auto* pHeader = qobject_cast<QToolButton*>(sender());
    if (pHeader) {
        pHeader->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    }
}
