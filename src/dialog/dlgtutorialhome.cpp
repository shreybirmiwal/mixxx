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
            background: #6847ed;
            border: 1px solid #896fff;
            border-radius: 18px;
        }
        QLabel#freePlayTitle {
            color: #ffffff;
            font-size: 22px;
            font-weight: 800;
        }
        QLabel#freePlayDescription {
            color: #e5ddff;
            font-size: 14px;
        }
        QPushButton#freePlayButton {
            background: #ffffff;
            border: 0;
            border-radius: 11px;
            color: #2d1f67;
            font-size: 15px;
            font-weight: 800;
            padding: 12px 20px;
        }
        QPushButton#freePlayButton:hover {
            background: #f0ecff;
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
        QPushButton[tutorialCard="true"] {
            background: #191d27;
            border: 1px solid #292f3c;
            border-radius: 12px;
            color: #f6f7fa;
            font-size: 15px;
            font-weight: 700;
            padding: 15px 18px;
            text-align: left;
        }
        QPushButton[tutorialCard="true"]:hover {
            background: #242a38;
            border-color: #8b72ff;
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

    auto pTitle = make_parented<QLabel>(tr("Choose how you want to play"), this);
    pTitle->setObjectName(QStringLiteral("title"));
    pRootLayout->addWidget(pTitle);

    auto pSubtitle = make_parented<QLabel>(
            tr("Start with a guided lesson or jump straight onto the decks."), this);
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
            tr("Basics"),
            tr("Learn the controls that make every mix work."),
            {{QStringLiteral("crossfader"),
                     tr("Crossfader  —  Blend smoothly between two decks")},
                    {QStringLiteral("bass-eq"),
                            tr("Bass & EQ  —  Shape lows, mids, and highs")},
                    {QStringLiteral("beatmatching"),
                            tr("Beatmatching  —  Align tempo and phase")},
                    {QStringLiteral("cueing"),
                            tr("Cueing  —  Prepare the next track in headphones")}});
    addTutorialSection(pTutorialLayout,
            tr("Wordplay"),
            tr("Build transitions around lyrics and memorable phrases."),
            {{QStringLiteral("starships-one-more-time"),
                     tr("Starships × One More Time")},
                    {QStringLiteral("xyz"), tr("XYZ  —  Your next wordplay routine")},
                    {QStringLiteral("phrase-matching"),
                            tr("Phrase matching  —  Find the shared lyric moment")}});
    addTutorialSection(pTutorialLayout,
            tr("Transitions"),
            tr("Practice reliable ways to move between tracks."),
            {{QStringLiteral("filter-sweep"),
                     tr("Filter sweep  —  Clear space for the next track")},
                    {QStringLiteral("echo-out"),
                            tr("Echo out  —  Exit cleanly on the phrase")},
                    {QStringLiteral("stem-handoff"),
                            tr("Stem handoff  —  Trade drums, bass, melody, and vocals")}});

    pTutorialLayout->addStretch();
    pScrollArea->setWidget(pScrollContent);
    pRootLayout->addWidget(pScrollArea, 1);
}

void TutorialHomePage::addTutorialSection(QVBoxLayout* pLayout,
        const QString& title,
        const QString& description,
        const QList<TutorialEntry>& tutorials) {
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
    pHeader->setArrowType(Qt::DownArrow);
    pHeader->setCheckable(true);
    pHeader->setChecked(true);
    pHeader->setCursor(Qt::PointingHandCursor);
    pSectionLayout->addWidget(pHeader);

    auto pBody = make_parented<QFrame>(pSection);
    pBody->setObjectName(QStringLiteral("tutorialSectionBody"));
    pBody->setProperty("sectionBody", true);
    auto pBodyLayout = make_parented<QVBoxLayout>(pBody);
    pBodyLayout->setContentsMargins(12, 12, 12, 12);
    pBodyLayout->setSpacing(9);

    for (const TutorialEntry& tutorial : tutorials) {
        auto pTutorialButton = make_parented<QPushButton>(tutorial.title, pBody);
        pTutorialButton->setObjectName(QStringLiteral("tutorialButton"));
        pTutorialButton->setProperty("tutorialCard", true);
        pTutorialButton->setProperty("tutorialId", tutorial.id);
        pTutorialButton->setAccessibleName(tutorial.title);
        pTutorialButton->setCursor(Qt::PointingHandCursor);
        connect(pTutorialButton,
                &QPushButton::clicked,
                this,
                [this, tutorialId = tutorial.id] {
                    emit openDjWorkspaceRequested(tutorialId);
                });
        pBodyLayout->addWidget(pTutorialButton);
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
