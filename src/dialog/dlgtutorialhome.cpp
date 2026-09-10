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
constexpr int kHomeMinimumWidth = 820;
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
            background: #090d12;
            color: #e6edf3;
        }
        QFrame#sidebar {
            background: #0d1219;
            border-right: 1px solid #1f2933;
        }
        QLabel#brand {
            color: #f0f6fc;
            font-size: 23px;
            font-weight: 900;
        }
        QLabel#brandAccent {
            color: #38d996;
            font-size: 23px;
            font-weight: 900;
        }
        QLabel#betaBadge {
            background: #123126;
            border: 1px solid #235841;
            border-radius: 5px;
            color: #62e6ad;
            font-size: 9px;
            font-weight: 800;
            padding: 3px 6px;
        }
        QLabel#sidebarCaption {
            color: #667382;
            font-size: 10px;
            font-weight: 800;
        }
        QFrame#navSelected {
            background: #14251f;
            border: 1px solid #214b39;
            border-radius: 8px;
        }
        QLabel#navSelectedLabel {
            color: #76e8b7;
            font-size: 13px;
            font-weight: 750;
        }
        QLabel[navItem="true"] {
            color: #748190;
            font-size: 13px;
            padding: 9px 11px;
        }
        QFrame#sidebarStats {
            background: #101720;
            border: 1px solid #202b37;
            border-radius: 9px;
        }
        QLabel#sidebarStatsTitle {
            color: #d8e0e8;
            font-size: 12px;
            font-weight: 750;
        }
        QLabel#sidebarStatsCopy, QLabel#sidebarFooter {
            color: #718090;
            font-size: 11px;
        }
        QPushButton#freePlayButton {
            background: #18212c;
            border: 1px solid #344151;
            border-radius: 8px;
            color: #dce5ee;
            font-size: 12px;
            font-weight: 750;
            padding: 9px 12px;
            text-align: left;
        }
        QPushButton#freePlayButton:hover {
            background: #202c38;
            border-color: #526276;
        }
        QWidget#mainPanel {
            background: #090d12;
        }
        QLabel#eyebrow {
            color: #38d996;
            font-size: 10px;
            font-weight: 850;
        }
        QLabel#title {
            color: #f0f6fc;
            font-size: 28px;
            font-weight: 850;
        }
        QLabel#subtitle {
            color: #8492a1;
            font-size: 13px;
        }
        QPushButton#updatesButton {
            background: transparent;
            border: 1px solid #293440;
            border-radius: 7px;
            color: #9ca9b7;
            font-size: 11px;
            font-weight: 700;
            padding: 7px 11px;
        }
        QPushButton#updatesButton:hover {
            background: #151c24;
            border-color: #445363;
            color: #dce5ee;
        }
        QLabel#updateStatus {
            background: #101b18;
            border: 1px solid #214b39;
            border-radius: 7px;
            color: #8eddb9;
            font-size: 11px;
            padding: 8px 10px;
        }
        QFrame#courseSummary {
            background: #0f151d;
            border: 1px solid #202a35;
            border-radius: 9px;
        }
        QLabel[summaryValue="true"] {
            color: #e6edf3;
            font-size: 15px;
            font-weight: 800;
        }
        QLabel[summaryLabel="true"] {
            color: #687686;
            font-size: 10px;
            font-weight: 700;
        }
        QScrollArea {
            background: transparent;
            border: 0;
        }
        QWidget#scrollContent {
            background: transparent;
        }
        QScrollBar:vertical {
            background: #0b1016;
            border: 0;
            margin: 0;
            width: 8px;
        }
        QScrollBar::handle:vertical {
            background: #344250;
            border-radius: 4px;
            min-height: 40px;
        }
        QScrollBar::handle:vertical:hover {
            background: #475869;
        }
        QScrollBar::add-line:vertical,
        QScrollBar::sub-line:vertical {
            height: 0;
        }
        QScrollBar::add-page:vertical,
        QScrollBar::sub-page:vertical {
            background: transparent;
        }
        QToolButton[sectionHeader="true"] {
            background: #0f151d;
            border: 1px solid #222d38;
            border-radius: 9px;
            color: #dfe7ef;
            font-size: 14px;
            font-weight: 750;
            padding: 12px 14px;
            text-align: left;
        }
        QToolButton[sectionHeader="true"]:hover {
            background: #131b24;
            border-color: #344352;
        }
        QFrame[sectionBody="true"] {
            background: transparent;
            border: 0;
        }
        QFrame[lessonCard="true"] {
            background: #0f151d;
            border: 1px solid #202b36;
            border-radius: 8px;
        }
        QFrame[lessonCard="true"]:hover {
            background: #121a23;
            border-color: #344452;
        }
        QLabel[levelBadge="true"] {
            background: #13271f;
            border: 1px solid #286148;
            border-radius: 17px;
            color: #64e3aa;
            font-size: 11px;
            font-weight: 850;
        }
        QLabel[lessonTitle="true"] {
            color: #e6edf3;
            font-size: 13px;
            font-weight: 750;
        }
        QLabel[lessonDescription="true"] {
            color: #7c8997;
            font-size: 11px;
        }
        QLabel[lessonMeta="true"] {
            color: #4eaf84;
            font-size: 9px;
            font-weight: 800;
        }
        QPushButton[startLesson="true"] {
            background: #153d2e;
            border: 1px solid #2c7a58;
            border-radius: 7px;
            color: #80ebbc;
            font-size: 11px;
            font-weight: 800;
            padding: 8px 13px;
        }
        QPushButton[startLesson="true"]:hover {
            background: #1d503c;
            border-color: #48ae80;
        }
        QLabel[comingSoon="true"] {
            color: #596674;
            font-size: 10px;
            font-weight: 750;
            padding: 7px 10px;
        }
    )"));

    auto pRootLayout = make_parented<QHBoxLayout>(this);
    pRootLayout->setContentsMargins(0, 0, 0, 0);
    pRootLayout->setSpacing(0);

    auto pSidebar = make_parented<QFrame>(this);
    pSidebar->setObjectName(QStringLiteral("sidebar"));
    pSidebar->setFixedWidth(205);
    auto pSidebarLayout = make_parented<QVBoxLayout>(pSidebar);
    pSidebarLayout->setContentsMargins(19, 22, 19, 18);
    pSidebarLayout->setSpacing(10);

    auto pBrandRow = make_parented<QWidget>(pSidebar);
    auto pBrandLayout = make_parented<QHBoxLayout>(pBrandRow);
    pBrandLayout->setContentsMargins(4, 0, 0, 0);
    pBrandLayout->setSpacing(0);
    auto pBrand = make_parented<QLabel>(tr("Leet"), pBrandRow);
    pBrand->setObjectName(QStringLiteral("brand"));
    auto pBrandAccent = make_parented<QLabel>(tr("DJ"), pBrandRow);
    pBrandAccent->setObjectName(QStringLiteral("brandAccent"));
    auto pBeta = make_parented<QLabel>(tr("BETA"), pBrandRow);
    pBeta->setObjectName(QStringLiteral("betaBadge"));
    pBrandLayout->addWidget(pBrand);
    pBrandLayout->addWidget(pBrandAccent);
    pBrandLayout->addSpacing(8);
    pBrandLayout->addWidget(pBeta);
    pBrandLayout->addStretch();
    pSidebarLayout->addWidget(pBrandRow);
    pSidebarLayout->addSpacing(24);

    auto pLearnCaption = make_parented<QLabel>(tr("LEARN"), pSidebar);
    pLearnCaption->setObjectName(QStringLiteral("sidebarCaption"));
    pSidebarLayout->addWidget(pLearnCaption);

    auto pSelectedNav = make_parented<QFrame>(pSidebar);
    pSelectedNav->setObjectName(QStringLiteral("navSelected"));
    auto pSelectedNavLayout = make_parented<QHBoxLayout>(pSelectedNav);
    pSelectedNavLayout->setContentsMargins(11, 9, 11, 9);
    auto pSelectedNavLabel = make_parented<QLabel>(tr("Roadmap"), pSelectedNav);
    pSelectedNavLabel->setObjectName(QStringLiteral("navSelectedLabel"));
    pSelectedNavLayout->addWidget(pSelectedNavLabel);
    pSidebarLayout->addWidget(pSelectedNav);

    auto pPracticeNav = make_parented<QLabel>(tr("Practice"), pSidebar);
    pPracticeNav->setProperty("navItem", true);
    auto pProgressNav = make_parented<QLabel>(tr("Progress"), pSidebar);
    pProgressNav->setProperty("navItem", true);
    pSidebarLayout->addWidget(pPracticeNav);
    pSidebarLayout->addWidget(pProgressNav);
    pSidebarLayout->addStretch();

    auto pStats = make_parented<QFrame>(pSidebar);
    pStats->setObjectName(QStringLiteral("sidebarStats"));
    auto pStatsLayout = make_parented<QVBoxLayout>(pStats);
    pStatsLayout->setContentsMargins(11, 10, 11, 10);
    pStatsLayout->setSpacing(3);
    auto pStatsTitle = make_parented<QLabel>(tr("8 interactive lessons"), pStats);
    pStatsTitle->setObjectName(QStringLiteral("sidebarStatsTitle"));
    auto pStatsCopy = make_parented<QLabel>(tr("A guided path from first track to first transition."), pStats);
    pStatsCopy->setObjectName(QStringLiteral("sidebarStatsCopy"));
    pStatsCopy->setWordWrap(true);
    pStatsLayout->addWidget(pStatsTitle);
    pStatsLayout->addWidget(pStatsCopy);
    pSidebarLayout->addWidget(pStats);

    auto pFreePlayButton = make_parented<QPushButton>(tr("Free Play"), pSidebar);
    pFreePlayButton->setObjectName(QStringLiteral("freePlayButton"));
    pFreePlayButton->setAccessibleName(tr("Free Play"));
    pFreePlayButton->setCursor(Qt::PointingHandCursor);
    connect(pFreePlayButton,
            &QPushButton::clicked,
            this,
            [this] {
                emit openDjWorkspaceRequested(QString());
            });
    pSidebarLayout->addWidget(pFreePlayButton);
    auto pSidebarFooter = make_parented<QLabel>(tr("Built for deliberate practice"), pSidebar);
    pSidebarFooter->setObjectName(QStringLiteral("sidebarFooter"));
    pSidebarLayout->addWidget(pSidebarFooter);
    pRootLayout->addWidget(pSidebar);

    auto pMainPanel = make_parented<QWidget>(this);
    pMainPanel->setObjectName(QStringLiteral("mainPanel"));
    auto pMainLayout = make_parented<QVBoxLayout>(pMainPanel);
    pMainLayout->setContentsMargins(30, 24, 30, 24);
    pMainLayout->setSpacing(10);

    auto pTopBarWidget = make_parented<QWidget>(pMainPanel);
    auto pTopBar = make_parented<QHBoxLayout>(pTopBarWidget);
    pTopBar->setContentsMargins(0, 0, 0, 0);
    auto pHeadingWidget = make_parented<QWidget>(pTopBarWidget);
    auto pHeadingLayout = make_parented<QVBoxLayout>(pHeadingWidget);
    pHeadingLayout->setContentsMargins(0, 0, 0, 0);
    pHeadingLayout->setSpacing(3);
    auto pEyebrow = make_parented<QLabel>(tr("BEGINNER PATH"), pHeadingWidget);
    pEyebrow->setObjectName(QStringLiteral("eyebrow"));
    auto pTitle = make_parented<QLabel>(tr("DJ Foundations"), pHeadingWidget);
    pTitle->setObjectName(QStringLiteral("title"));
    auto pSubtitle = make_parented<QLabel>(
            tr("Learn one skill at a time. LeetDJ checks each move as you practice."), pHeadingWidget);
    pSubtitle->setObjectName(QStringLiteral("subtitle"));
    pSubtitle->setWordWrap(true);
    pHeadingLayout->addWidget(pEyebrow);
    pHeadingLayout->addWidget(pTitle);
    pHeadingLayout->addWidget(pSubtitle);
    pTopBar->addWidget(pHeadingWidget, 1);

    auto pUpdatesButton = make_parented<QPushButton>(tr("Check for updates"), pTopBarWidget);
    pUpdatesButton->setObjectName(QStringLiteral("updatesButton"));
    pUpdatesButton->setAccessibleName(tr("Updates"));
    pUpdatesButton->setCursor(Qt::PointingHandCursor);
    connect(pUpdatesButton,
            &QPushButton::clicked,
            this,
            &TutorialHomePage::showUpdateStatus);
    pTopBar->addWidget(pUpdatesButton);
    pMainLayout->addWidget(pTopBarWidget);

    auto pUpdateStatus = make_parented<QLabel>(pMainPanel);
    m_pUpdateStatus = pUpdateStatus.get();
    m_pUpdateStatus->setObjectName(QStringLiteral("updateStatus"));
    m_pUpdateStatus->setWordWrap(true);
    m_pUpdateStatus->hide();
    pMainLayout->addWidget(pUpdateStatus);

    auto pCourseSummary = make_parented<QFrame>(pMainPanel);
    pCourseSummary->setObjectName(QStringLiteral("courseSummary"));
    auto pCourseSummaryLayout = make_parented<QHBoxLayout>(pCourseSummary);
    pCourseSummaryLayout->setContentsMargins(15, 10, 15, 10);
    pCourseSummaryLayout->setSpacing(30);
    const auto addSummary = [&pCourseSummaryLayout, &pCourseSummary](
                                    const QString& value, const QString& label) {
        auto pWidget = make_parented<QWidget>(pCourseSummary);
        auto pLayout = make_parented<QVBoxLayout>(pWidget);
        pLayout->setContentsMargins(0, 0, 0, 0);
        pLayout->setSpacing(0);
        auto pValue = make_parented<QLabel>(value, pWidget);
        pValue->setProperty("summaryValue", true);
        auto pLabel = make_parented<QLabel>(label, pWidget);
        pLabel->setProperty("summaryLabel", true);
        pLayout->addWidget(pValue);
        pLayout->addWidget(pLabel);
        pCourseSummaryLayout->addWidget(pWidget);
    };
    addSummary(tr("10"), tr("SKILLS"));
    addSummary(tr("8"), tr("INTERACTIVE"));
    addSummary(tr("~50 min"), tr("GUIDED PRACTICE"));
    pCourseSummaryLayout->addStretch();
    pMainLayout->addWidget(pCourseSummary);

    auto pScrollArea = make_parented<QScrollArea>(pMainPanel);
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
                    {QStringLiteral("channel-faders"), QStringLiteral("03"),
                            tr("Channel fader balance"),
                            tr("Control each deck's loudness independently and build a balanced mix."),
                            tr("LEVELS"), tr("6 MIN"), true},
                    {QStringLiteral("bass-eq"), QStringLiteral("04"),
                            tr("EQ and filter knobs"),
                            tr("Learn what HIGH, MID, LOW, and the filter remove from a song."),
                            tr("TONE CONTROL"), tr("7 MIN"), true}});
    addTutorialSection(pTutorialLayout,
            tr("3  ·  Timing and preparation"),
            tr("Prepare the next track and make both songs move together."),
            {{QStringLiteral("cueing"), QStringLiteral("05"),
                     tr("Cue the next track"),
                     tr("Preview Deck 2, set a cue point, and return to it before the mix."),
                     tr("PREPARATION"), tr("6 MIN"), true},
                    {QStringLiteral("beatmatching"), QStringLiteral("06"),
                            tr("Match the tempo"),
                            tr("Use SYNC and the tempo fader to understand BPM alignment."),
                            tr("RHYTHM"), tr("7 MIN"), true},
                    {QStringLiteral("looping"), QStringLiteral("07"),
                            tr("Build and release a loop"),
                            tr("Count a phrase, create a loop, resize it, exit, and reloop."),
                            tr("PHRASING"), tr("7 MIN"), true}});
    addTutorialSection(pTutorialLayout,
            tr("4  ·  Creative transitions"),
            tr("Combine the fundamentals into repeatable performance moves."),
            {{QStringLiteral("filter-sweep"), QStringLiteral("08"),
                     tr("Filter sweep transition"),
                     tr("Clear frequency space while bringing the next track into the mix."),
                     tr("TRANSITION"), tr("6 MIN"), true},
                    {QStringLiteral("echo-out"), QStringLiteral("09"),
                            tr("Echo-out transition"),
                            tr("Exit a song cleanly at the end of a musical phrase."),
                            tr("TRANSITION"), tr("COMING NEXT"), false},
                    {QStringLiteral("starships-one-more-time"), QStringLiteral("10"),
                            tr("Wordplay: Starships × One More Time"),
                            tr("Use matching words and phrase timing to connect two recognizable moments."),
                            tr("WORDPLAY"), tr("COMING NEXT"), false}});

    pTutorialLayout->addStretch();
    pScrollArea->setWidget(pScrollContent);
    pMainLayout->addWidget(pScrollArea, 1);
    pRootLayout->addWidget(pMainPanel, 1);
}

void TutorialHomePage::addTutorialSection(QVBoxLayout* pLayout,
        const QString& title,
        const QString& description,
        const QList<TutorialEntry>& tutorials) {
    const bool expandedByDefault = true;
    auto pSection = make_parented<QFrame>(pLayout->parentWidget());
    auto pSectionLayout = make_parented<QVBoxLayout>(pSection);
    pSectionLayout->setContentsMargins(0, 0, 0, 0);
    pSectionLayout->setSpacing(8);

    auto pHeader = make_parented<QToolButton>(pSection);
    pHeader->setObjectName(QStringLiteral("tutorialSectionHeader"));
    pHeader->setProperty("sectionHeader", true);
    pHeader->setAccessibleName(title);
    pHeader->setAccessibleDescription(description);
    pHeader->setText(title);
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
        pLevel->setFixedSize(34, 34);
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
            tr("LeetDJ %1 is running from your local build. Source updates can be "
               "installed from this project's repository.")
                    .arg(QCoreApplication::applicationVersion()));
    m_pUpdateStatus->show();
}

void TutorialHomePage::updateSectionArrow(bool expanded) {
    auto* pHeader = qobject_cast<QToolButton*>(sender());
    if (pHeader) {
        pHeader->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    }
}
