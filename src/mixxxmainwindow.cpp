#include "mixxxmainwindow.h"

#include <algorithm>
#include <utility>

#include <QAction>
#include <QCheckBox>
#include <QCloseEvent>
#include <QDebug>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontMetrics>
#include <QHash>
#include <QLabel>
#include <QOpenGLContext>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPushButton>
#include <QScreen>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QToolTip>
#include <QUrl>
#include <QtMath>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QGLFormat>
#endif

#if defined(__LINUX__) && !defined(__ANDROID__)
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#endif

#ifdef MIXXX_USE_QOPENGL
#include <QGuiApplication>

#include "widget/tooltipqopengl.h"
#include "widget/winitialglwidget.h"
#endif

#include "controllers/keyboard/keyboardeventfilter.h"
#include "coreservices.h"
#include "defs_urls.h"
#include "dialog/dlgabout.h"
#include "dialog/dlgdevelopertools.h"
#include "dialog/dlgkeywheel.h"
#include "dialog/dlgtutorialhome.h"
#include "moc_mixxxmainwindow.cpp"
#include "preferences/dialog/dlgpreferences.h"
#ifdef __BROADCAST__
#include "broadcast/broadcastmanager.h"
#endif
#include "control/controlindicatortimer.h"
#include "control/controlproxy.h"
#include "library/library.h"
#include "library/library_decl.h"
#include "library/library_prefs.h"
#ifdef __ENGINEPRIME__
#include "library/export/libraryexporter.h"
#endif
#include "library/library_prefs.h"
#include "library/trackcollection.h"
#include "library/trackcollectionmanager.h"
#include "mixer/basetrackplayer.h"
#include "mixer/playerinfo.h"
#include "mixer/playermanager.h"
#include "recording/recordingmanager.h"
#include "skin/legacy/launchimage.h"
#include "skin/skinloader.h"
#include "soundio/soundmanager.h"
#include "sources/soundsourceproxy.h"
#include "track/track.h"
#include "tutorial/tutorialvisibility.h"
#include "tutorial/tutorialvisibilitypanel.h"
#include "util/debug.h"
#include "util/desktophelper.h"
#include "util/menubarhelper.h"
#include "util/sandbox.h"
#include "util/scopedoverridecursor.h"
#include "util/timer.h"
#include "util/versionstore.h"
#include "waveform/guitick.h"
#include "waveform/sharedglcontext.h"
#include "waveform/visualsmanager.h"
#include "waveform/waveformwidgetfactory.h"
#include "widget/wglwidget.h"
#include "widget/wmainmenubar.h"

#ifdef __VINYLCONTROL__
#include "vinylcontrol/vinylcontrolmanager.h"
#endif

namespace {
const ConfigKey kHideMenuBarConfigKey = ConfigKey("[Config]", "hide_menubar");
const ConfigKey kMenuBarHintConfigKey = ConfigKey("[Config]", "show_menubar_hint");

enum class TutorialAction {
    Acknowledge,
    Timed,
    ControlChanged,
    ControlPositive,
    ControlBelow,
    ControlAbove,
    ControlNear,
    TrackReload,
    PlaybackWait,
};

struct TutorialGuideStep {
    TutorialGuideStep(QString title,
            QString detail,
            QString objectName = {},
            QString within = {},
            QString tooltipId = {},
            QString controlKey = {},
            QString widgetType = {},
            TutorialAction action = TutorialAction::Timed,
            QString actionGroup = {},
            QString actionItem = {},
            double changeThreshold = 0.01,
            int waitMs = 2200,
            bool pauseOnEnter = false,
            int listenAfterMs = 650,
            double expectedValue = 0.0,
            bool manualAdvance = false)
            : title(std::move(title)),
              detail(std::move(detail)),
              objectName(std::move(objectName)),
              within(std::move(within)),
              tooltipId(std::move(tooltipId)),
              controlKey(std::move(controlKey)),
              widgetType(std::move(widgetType)),
              action(action),
              actionGroup(std::move(actionGroup)),
              actionItem(std::move(actionItem)),
              changeThreshold(changeThreshold),
              waitMs(waitMs),
              pauseOnEnter(pauseOnEnter),
              listenAfterMs(listenAfterMs),
              expectedValue(expectedValue),
              manualAdvance(manualAdvance) {
    }

    QString title;
    QString detail;
    QString objectName;
    QString within;
    QString tooltipId;
    QString controlKey;
    QString widgetType;
    TutorialAction action;
    QString actionGroup;
    QString actionItem;
    double changeThreshold;
    int waitMs;
    bool pauseOnEnter;
    int listenAfterMs;
    double expectedValue;
    bool manualAdvance;
};

bool tutorialStepUsesNextButton(const TutorialGuideStep& step) {
    return step.action == TutorialAction::Acknowledge || step.manualAdvance;
}

QList<TutorialGuideStep> tutorialGuideSteps(const QString& tutorialId) {
    if (tutorialId == QStringLiteral("level-zero")) {
        return {
                {QObject::tr("This is your song library"),
                        QObject::tr("Every song in your library appears here. In the next steps, you will choose one song for each deck."),
                        QStringLiteral("LibraryContainer"),
                        {}, {}, {}, {}, TutorialAction::Acknowledge},
                {QObject::tr("Choose song 1 for the left deck"),
                        QObject::tr("Pick one song from the library, then drag and drop it directly into this glowing left title box. Its title, artwork, and waveform will appear here."),
                        QStringLiteral("TitleText"),
                        QStringLiteral("Deck1_Src"),
                        {}, {}, {}, TutorialAction::TrackReload,
                        QStringLiteral("[Channel1]"), QStringLiteral("track_samples"),
                        1.0, 0, true},
                {QObject::tr("Choose song 2 for the right deck"),
                        QObject::tr("Pick a different song, then drag and drop it into this glowing right title box. This lets you prepare the next song while the first one plays."),
                        QStringLiteral("TitleText"),
                        QStringLiteral("Deck2_Src"),
                        {}, {}, {}, TutorialAction::TrackReload,
                        QStringLiteral("[Channel2]"), QStringLiteral("track_samples"),
                        1.0, 0, true},
                {QObject::tr("Read the waveforms"),
                        QObject::tr("The colored shapes are pictures of the sound. The music moves through the center line, helping you see loud sections and upcoming changes."),
                        QStringLiteral("WaveformsContainer"),
                        {}, {}, {}, {}, TutorialAction::Acknowledge},
                {QObject::tr("Start your first track"),
                        QObject::tr("Press Play to start the left song. Leave it playing so you can hear what each control changes."),
                        QStringLiteral("PlayDeck"),
                        QStringLiteral("Deck1_Src"),
                        {}, {}, {}, TutorialAction::ControlPositive,
                        QStringLiteral("[Channel1]"), QStringLiteral("play"),
                        0.01, 0, true, 1800},
                {QObject::tr("Adjust the tempo"),
                        QObject::tr("Drag this fader gently. Moving away from the center changes how fast the song plays. Explore both directions, return it to center, then choose Next."),
                        QStringLiteral("RateSlider"),
                        QStringLiteral("Deck1_Src"),
                        {}, {}, {}, TutorialAction::ControlChanged,
                        QStringLiteral("[Channel1]"), QStringLiteral("rate"),
                        0.005, 0, true, 1400, 0.0, true},
                {QObject::tr("Set a deck's volume"),
                        QObject::tr("This vertical fader controls how loudly the left deck reaches the audience. Try a few levels, then choose Next when the balance makes sense."),
                        {},
                        {},
                        QStringLiteral("channel_volume"),
                        QStringLiteral("[Channel1],volume"),
                        {}, TutorialAction::ControlChanged,
                        QStringLiteral("[Channel1]"), QStringLiteral("volume"),
                        0.02, 0, true, 1400, 0.0, true},
                {QObject::tr("Start the right song"),
                        QObject::tr("Press Play on the right deck. A crossfader can only demonstrate a real blend when both songs are running."),
                        QStringLiteral("PlayDeck"),
                        QStringLiteral("Deck2_Src"),
                        {}, {}, {}, TutorialAction::ControlPositive,
                        QStringLiteral("[Channel2]"), QStringLiteral("play"),
                        0.01, 0, false, 1200},
                {QObject::tr("Blend with the crossfader"),
                        QObject::tr("Left plays only the left deck, right plays only the right deck, and the center blends both. Explore the full range, then choose Finish lesson."),
                        {},
                        {},
                        QStringLiteral("crossfader"),
                        {}, {}, TutorialAction::ControlChanged,
                        QStringLiteral("[Master]"), QStringLiteral("crossfader"),
                        0.04, 0, false, 1800, 0.0, true},
        };
    }
    if (tutorialId == QStringLiteral("crossfader")) {
        return {
                {QObject::tr("Meet the crossfader"),
                        QObject::tr("This horizontal fader decides which deck reaches the audience. Left favors Deck 1, right favors Deck 2, and the center blends both."),
                        {}, {}, QStringLiteral("crossfader"),
                        {}, {}, TutorialAction::Acknowledge},
                {QObject::tr("Start Deck 1"),
                        QObject::tr("Press Play on the left deck. Leave it running so every fader position has something to hear."),
                        QStringLiteral("PlayDeck"), QStringLiteral("Deck1_Src"),
                        {}, {}, {}, TutorialAction::ControlPositive,
                        QStringLiteral("[Channel1]"), QStringLiteral("play"),
                        0.01, 0, true, 1200},
                {QObject::tr("Start Deck 2"),
                        QObject::tr("Press Play on the right deck. Both songs are now running; the crossfader controls which one is audible."),
                        QStringLiteral("PlayDeck"), QStringLiteral("Deck2_Src"),
                        {}, {}, {}, TutorialAction::ControlPositive,
                        QStringLiteral("[Channel2]"), QStringLiteral("play"),
                        0.01, 0, false, 1200},
                {QObject::tr("Hear both tracks"),
                        QObject::tr("Listen to the center blend for a moment. Two full songs can sound crowded—that is why deliberate fader movement matters."),
                        QStringLiteral("WaveformsContainer"),
                        {}, {}, {}, {}, TutorialAction::PlaybackWait,
                        QStringLiteral("[Channel1]"), QStringLiteral("play"),
                        0.01, 2500, false, 300},
                {QObject::tr("Isolate Deck 1"),
                        QObject::tr("Move the crossfader nearly all the way left. You should hear the left song clearly while the right one disappears."),
                        {}, {}, QStringLiteral("crossfader"),
                        {}, {}, TutorialAction::ControlBelow,
                        QStringLiteral("[Master]"), QStringLiteral("crossfader"),
                        0.05, 0, false, 1500, -0.75},
                {QObject::tr("Return to the center"),
                        QObject::tr("Move the fader back to the center. Listen for the moment the second song becomes part of the mix."),
                        {}, {}, QStringLiteral("crossfader"),
                        {}, {}, TutorialAction::ControlNear,
                        QStringLiteral("[Master]"), QStringLiteral("crossfader"),
                        0.12, 0, false, 1500, 0.0},
                {QObject::tr("Isolate Deck 2"),
                        QObject::tr("Now move nearly all the way right. Deck 2 takes over and Deck 1 leaves the audience mix."),
                        {}, {}, QStringLiteral("crossfader"),
                        {}, {}, TutorialAction::ControlAbove,
                        QStringLiteral("[Master]"), QStringLiteral("crossfader"),
                        0.05, 0, false, 1500, 0.75},
                {QObject::tr("Finish a smooth blend"),
                        QObject::tr("Move slowly back to center. A controlled transition sounds intentional; avoid throwing the fader unless the style calls for it."),
                        {}, {}, QStringLiteral("crossfader"),
                        {}, {}, TutorialAction::ControlNear,
                        QStringLiteral("[Master]"), QStringLiteral("crossfader"),
                        0.12, 0, false, 1800, 0.0},
        };
    }
    if (tutorialId == QStringLiteral("bass-eq")) {
        return {
                {QObject::tr("Load a song"),
                        QObject::tr("Choose a song you know well and drag it onto the left deck. Hearing a familiar song makes the tone controls easier to understand."),
                        QStringLiteral("LibraryContainer"),
                        {}, {}, {}, {}, TutorialAction::TrackReload,
                        QStringLiteral("[Channel1]"), QStringLiteral("track_samples"),
                        1.0, 0, true},
                {QObject::tr("Start the music"),
                        QObject::tr("Press Play. Leave the song running while you test each knob so your ears can hear the change immediately."),
                        QStringLiteral("PlayDeck"),
                        QStringLiteral("Deck1_Src"),
                        {}, {}, {}, TutorialAction::ControlPositive,
                        QStringLiteral("[Channel1]"), QStringLiteral("play"),
                        0.01, 0, true, 1500},
                {QObject::tr("Wait for the bright sounds"),
                        QObject::tr("Keep listening until the next bright hi-hat or vocal phrase arrives. When the listening check completes, continue with Next step."),
                        QStringLiteral("WaveformsContainer"),
                        {}, {}, {}, {}, TutorialAction::PlaybackWait,
                        QStringLiteral("[Channel1]"), QStringLiteral("play"),
                        0.01, 4000, false, 300},
                {QObject::tr("High frequencies"),
                        QObject::tr("Click HIGH and drag down. It controls hi-hats, cymbals, and crisp vocals. Explore a few positions, return to center, then choose Next."),
                        {},
                        QStringLiteral("MixerChannel_2Decks_Left"),
                        QStringLiteral("filterHigh"),
                        {}, {}, TutorialAction::ControlChanged, {}, {},
                        0.03, 0, true, 2400, 0.0, true},
                {QObject::tr("Mid frequencies"),
                        QObject::tr("MID contains much of the vocal, melody, and body of a track. Turn it down, listen, return it to center, then choose Next."),
                        {},
                        QStringLiteral("MixerChannel_2Decks_Left"),
                        QStringLiteral("filterMid"),
                        {}, {}, TutorialAction::ControlChanged, {}, {},
                        0.03, 0, true, 2400, 0.0, true},
                {QObject::tr("Low frequencies"),
                        QObject::tr("LOW controls the kick drum and bass. Try lowering it, listen to the kick disappear, reset to center, then choose Next."),
                        {},
                        QStringLiteral("MixerChannel_2Decks_Left"),
                        QStringLiteral("filterLow"),
                        {}, {}, TutorialAction::ControlChanged, {}, {},
                        0.03, 0, true, 2400, 0.0, true},
                {QObject::tr("The filter knob"),
                        QObject::tr("Turn clockwise to remove lows and counterclockwise to remove highs. Explore both directions, return to neutral, then choose Next."),
                        {},
                        QStringLiteral("MixerChannel_2Decks_Left"),
                        QStringLiteral("QuickEffectRack_super1"),
                        {}, {}, TutorialAction::ControlChanged, {}, {},
                        0.03, 0, true, 2400, 0.0, true},
                {QObject::tr("Practice a clean swap"),
                        QObject::tr("Make one more deliberate LOW adjustment. Lowering the outgoing bass prevents two basslines from clashing. Choose Finish lesson when it sounds clear."),
                        {},
                        QStringLiteral("MixerChannel_2Decks_Left"),
                        QStringLiteral("filterLow"),
                        {}, {}, TutorialAction::ControlChanged, {}, {},
                        0.03, 0, true, 2400, 0.0, true},
        };
    }
    if (tutorialId == QStringLiteral("looping")) {
        return {
                {QObject::tr("Load one song"),
                        QObject::tr("Pick a song with a clear drum beat and drag it onto the left deck."),
                        QStringLiteral("LibraryContainer"),
                        {}, {}, {}, {}, TutorialAction::TrackReload,
                        QStringLiteral("[Channel1]"), QStringLiteral("track_samples"),
                        1.0, 0, true},
                {QObject::tr("Start the song"),
                        QObject::tr("Press Play and listen for the steady count: one, two, three, four."),
                        QStringLiteral("PlayDeck"),
                        QStringLiteral("Deck1_Src"),
                        {}, {}, {}, TutorialAction::ControlPositive,
                        QStringLiteral("[Channel1]"), QStringLiteral("play"),
                        0.01, 0, true, 1200},
                {QObject::tr("Choose the loop length"),
                        QObject::tr("This number is the loop length in beats. Try the available sizes, select 4 beats—one bar in most dance music—then choose Next."),
                        {},
                        QStringLiteral("Deck1_Src"),
                        QStringLiteral("beatloop_size"),
                        {}, {}, TutorialAction::ControlChanged,
                        QStringLiteral("[Channel1]"), QStringLiteral("beatloop_size"),
                        0.1, 0, true, 1200, 0.0, true},
                {QObject::tr("Wait for the phrase"),
                        QObject::tr("Let the track play while you count one, two, three, four. The lesson is waiting for a clean musical point before it asks you to loop."),
                        QStringLiteral("WaveformsContainer"),
                        {}, {}, {}, {}, TutorialAction::PlaybackWait,
                        QStringLiteral("[Channel1]"), QStringLiteral("play"),
                        0.01, 4000, false, 300},
                {QObject::tr("Turn the loop on"),
                        QObject::tr("Press this loop button near the start of a musical phrase. The selected number of beats repeats without stopping the music."),
                        QStringLiteral("LoopActivate"),
                        QStringLiteral("Deck1_Src"),
                        {}, {}, {}, TutorialAction::ControlPositive,
                        QStringLiteral("[Channel1]"), QStringLiteral("beatloop_activate"),
                        0.01, 0, false, 1200},
                {QObject::tr("Make the loop shorter"),
                        QObject::tr("Reduce the beat count while the loop is active. A shorter loop repeats faster and builds tension. Do it gradually so the change sounds intentional."),
                        {},
                        QStringLiteral("Deck1_Src"),
                        QStringLiteral("beatloop_size"),
                        {}, {}, TutorialAction::ControlChanged,
                        QStringLiteral("[Channel1]"), QStringLiteral("beatloop_size"),
                        0.1, 0, false, 1200},
                {QObject::tr("Make the loop longer"),
                        QObject::tr("Increase the beat count again. Longer loops preserve more of the musical phrase and usually sound calmer."),
                        {},
                        QStringLiteral("Deck1_Src"),
                        QStringLiteral("beatloop_size"),
                        {}, {}, TutorialAction::ControlChanged,
                        QStringLiteral("[Channel1]"), QStringLiteral("beatloop_size"),
                        0.1, 0, false, 1200},
                {QObject::tr("Exit the loop"),
                        QObject::tr("Press the lit loop button again. Playback continues forward from the current position—your track does not restart."),
                        QStringLiteral("LoopActivate"),
                        QStringLiteral("Deck1_Src"),
                        {}, {}, {}, TutorialAction::ControlChanged,
                        QStringLiteral("[Channel1]"), QStringLiteral("loop_enabled"),
                        0.01, 0, false},
                {QObject::tr("Bring a loop back"),
                        QObject::tr("RELOOP returns to the most recent loop after you exit it. Use it when you need more time to prepare the next song."),
                        QStringLiteral("Reloop"),
                        QStringLiteral("Deck1_Src"),
                        {}, {}, {}, TutorialAction::ControlPositive,
                        QStringLiteral("[Channel1]"), QStringLiteral("reloop_toggle"),
                        0.01, 0, false, 1500},
        };
    }
    if (tutorialId == QStringLiteral("cueing")) {
        return {
                {QObject::tr("Load the next song"),
                        QObject::tr("Drag a song into Deck 2. DJs prepare the next track on the right while the current track stays on the left."),
                        QStringLiteral("TitleText"), QStringLiteral("Deck2_Src"),
                        {}, {}, {}, TutorialAction::TrackReload,
                        QStringLiteral("[Channel2]"), QStringLiteral("track_samples"),
                        1.0, 0, true},
                {QObject::tr("Send Deck 2 to headphones"),
                        QObject::tr("Press the headphone/PFL button for Deck 2. This is the private preview path; the audience mix does not need to hear it."),
                        {}, QStringLiteral("MixerChannel_2Decks_Right"),
                        QStringLiteral("pfl"),
                        {}, {}, TutorialAction::ControlPositive,
                        QStringLiteral("[Channel2]"), QStringLiteral("pfl")},
                {QObject::tr("Preview the track"),
                        QObject::tr("Press Play on Deck 2. In a real setup you would hear this preview in headphones while preparing the starting point."),
                        QStringLiteral("PlayDeck"), QStringLiteral("Deck2_Src"),
                        {}, {}, {}, TutorialAction::ControlPositive,
                        QStringLiteral("[Channel2]"), QStringLiteral("play"),
                        0.01, 0, true, 1400},
                {QObject::tr("Find a clean downbeat"),
                        QObject::tr("Listen and watch the waveform for a strong first beat. The lesson waits while the song moves to a useful cue position."),
                        QStringLiteral("WaveformsContainer"),
                        {}, {}, {}, {}, TutorialAction::PlaybackWait,
                        QStringLiteral("[Channel2]"), QStringLiteral("play"),
                        0.01, 3000, false, 300},
                {QObject::tr("Set and return to Cue"),
                        QObject::tr("Press CUE on Deck 2. It marks or returns to the prepared point so the track is ready to launch consistently."),
                        QStringLiteral("CueDeck"), QStringLiteral("Deck2_Src"),
                        {}, {}, {}, TutorialAction::ControlPositive,
                        QStringLiteral("[Channel2]"), QStringLiteral("cue_default"),
                        0.01, 0, false, 1000},
                {QObject::tr("Launch from the cue point"),
                        QObject::tr("Press Play again. The song should begin from the position you prepared instead of an arbitrary place."),
                        QStringLiteral("PlayDeck"), QStringLiteral("Deck2_Src"),
                        {}, {}, {}, TutorialAction::ControlPositive,
                        QStringLiteral("[Channel2]"), QStringLiteral("play"),
                        0.01, 0, false, 1500},
                {QObject::tr("Leave headphone preview"),
                        QObject::tr("Turn Deck 2 PFL off. The preparation is complete and the deck is ready for the main mix."),
                        {}, QStringLiteral("MixerChannel_2Decks_Right"),
                        QStringLiteral("pfl"),
                        {}, {}, TutorialAction::ControlChanged,
                        QStringLiteral("[Channel2]"), QStringLiteral("pfl"),
                        0.5},
        };
    }
    if (tutorialId == QStringLiteral("beatmatching")) {
        return {
                {QObject::tr("Start Deck 1"),
                        QObject::tr("Press Play on Deck 1. This will be the reference track whose tempo Deck 2 follows."),
                        QStringLiteral("PlayDeck"), QStringLiteral("Deck1_Src"),
                        {}, {}, {}, TutorialAction::ControlPositive,
                        QStringLiteral("[Channel1]"), QStringLiteral("play"),
                        0.01, 0, true, 1000},
                {QObject::tr("Start Deck 2"),
                        QObject::tr("Press Play on Deck 2. The songs begin with different BPM values, so their beats drift apart."),
                        QStringLiteral("PlayDeck"), QStringLiteral("Deck2_Src"),
                        {}, {}, {}, TutorialAction::ControlPositive,
                        QStringLiteral("[Channel2]"), QStringLiteral("play"),
                        0.01, 0, false, 1000},
                {QObject::tr("Hear the drift"),
                        QObject::tr("Listen to the kick drums and watch both waveforms. Without matching, the rhythmic peaks stop lining up."),
                        QStringLiteral("WaveformsContainer"),
                        {}, {}, {}, {}, TutorialAction::PlaybackWait,
                        QStringLiteral("[Channel1]"), QStringLiteral("play"),
                        0.01, 3000, false, 300},
                {QObject::tr("Match Deck 2 with SYNC"),
                        QObject::tr("Press SYNC on Deck 2. It matches the tempo to the reference so both tracks share a BPM."),
                        QStringLiteral("SyncDeck"), QStringLiteral("Deck2_Src"),
                        {}, {}, {}, TutorialAction::ControlPositive,
                        QStringLiteral("[Channel2]"), QStringLiteral("sync_enabled"),
                        0.01, 0, false, 1600},
                {QObject::tr("Read the matched waveforms"),
                        QObject::tr("The tempo is matched. Watch the beat markers travel together and listen for a steadier combined rhythm."),
                        QStringLiteral("WaveformsContainer"),
                        {}, {}, {}, {}, TutorialAction::PlaybackWait,
                        QStringLiteral("[Channel1]"), QStringLiteral("play"),
                        0.01, 2500, false, 300},
                {QObject::tr("Turn SYNC off"),
                        QObject::tr("Press SYNC again. The matched speed remains, but Deck 2 is now available for a manual tempo adjustment."),
                        QStringLiteral("SyncDeck"), QStringLiteral("Deck2_Src"),
                        {}, {}, {}, TutorialAction::ControlChanged,
                        QStringLiteral("[Channel2]"), QStringLiteral("sync_enabled"),
                        0.5},
                {QObject::tr("Adjust tempo by hand"),
                        QObject::tr("Move Deck 2's tempo fader in both directions and watch its BPM change. Choose Next when you understand the relationship."),
                        QStringLiteral("RateSlider"), QStringLiteral("Deck2_Src"),
                        {}, {}, {}, TutorialAction::ControlChanged,
                        QStringLiteral("[Channel2]"), QStringLiteral("rate"),
                        0.005, 0, false, 1400, 0.0, true},
                {QObject::tr("Recover the match"),
                        QObject::tr("Press SYNC once more to recover a clean tempo match. Later lessons can teach matching it completely by ear."),
                        QStringLiteral("SyncDeck"), QStringLiteral("Deck2_Src"),
                        {}, {}, {}, TutorialAction::ControlPositive,
                        QStringLiteral("[Channel2]"), QStringLiteral("sync_enabled"),
                        0.01, 0, false, 1500},
        };
    }
    if (tutorialId == QStringLiteral("channel-faders")) {
        return {
                {QObject::tr("Meet the channel faders"),
                        QObject::tr("Each vertical channel fader controls one deck's loudness. Unlike the crossfader, channel faders let you set both decks independently."),
                        {}, {}, QStringLiteral("channel_volume"),
                        QStringLiteral("[Channel1],volume"), {},
                        TutorialAction::Acknowledge},
                {QObject::tr("Start Deck 1"),
                        QObject::tr("Press Play on the left deck. Keep it running while you learn its channel fader."),
                        QStringLiteral("PlayDeck"), QStringLiteral("Deck1_Src"),
                        {}, {}, {}, TutorialAction::ControlPositive,
                        QStringLiteral("[Channel1]"), QStringLiteral("play"),
                        0.01, 0, true, 900},
                {QObject::tr("Start Deck 2"),
                        QObject::tr("Press Play on the right deck. Both tracks are playing, so you can hear how the two channel levels interact."),
                        QStringLiteral("PlayDeck"), QStringLiteral("Deck2_Src"),
                        {}, {}, {}, TutorialAction::ControlPositive,
                        QStringLiteral("[Channel2]"), QStringLiteral("play"),
                        0.01, 0, false, 900},
                {QObject::tr("Lower Deck 1"),
                        QObject::tr("Pull Deck 1's channel fader below one-third. Its waveform keeps moving, but that deck becomes quiet in the audience mix."),
                        {}, {}, QStringLiteral("channel_volume"),
                        QStringLiteral("[Channel1],volume"), {},
                        TutorialAction::ControlBelow,
                        QStringLiteral("[Channel1]"), QStringLiteral("volume"),
                        0.02, 0, false, 900, 0.33},
                {QObject::tr("Explore Deck 1's level"),
                        QObject::tr("Move the fader through a few positions and listen to the balance. Take your time; choose Next when the relationship feels clear."),
                        {}, {}, QStringLiteral("channel_volume"),
                        QStringLiteral("[Channel1],volume"), {},
                        TutorialAction::ControlChanged,
                        QStringLiteral("[Channel1]"), QStringLiteral("volume"),
                        0.03, 0, false, 300, 0.0, true},
                {QObject::tr("Restore Deck 1"),
                        QObject::tr("Raise Deck 1 above the 80% mark so it returns to a strong working level."),
                        {}, {}, QStringLiteral("channel_volume"),
                        QStringLiteral("[Channel1],volume"), {},
                        TutorialAction::ControlAbove,
                        QStringLiteral("[Channel1]"), QStringLiteral("volume"),
                        0.02, 0, false, 900, 0.8},
                {QObject::tr("Balance Deck 2"),
                        QObject::tr("Now explore Deck 2's channel fader. Set the two tracks to a balance that sounds comfortable, then choose Finish lesson."),
                        {}, {}, QStringLiteral("channel_volume"),
                        QStringLiteral("[Channel2],volume"), {},
                        TutorialAction::ControlChanged,
                        QStringLiteral("[Channel2]"), QStringLiteral("volume"),
                        0.03, 0, false, 300, 0.0, true},
        };
    }
    if (tutorialId == QStringLiteral("filter-sweep")) {
        return {
                {QObject::tr("What the filter does"),
                        QObject::tr("The filter removes parts of a track with one knob. Turn right to remove low frequencies; turn left to remove high frequencies."),
                        {}, QStringLiteral("MixerChannel_2Decks_Left"),
                        QStringLiteral("QuickEffectRack_super1"),
                        {}, {}, TutorialAction::Acknowledge},
                {QObject::tr("Start Deck 1"),
                        QObject::tr("Press Play on Deck 1. This is the track you will sweep out of the mix."),
                        QStringLiteral("PlayDeck"), QStringLiteral("Deck1_Src"),
                        {}, {}, {}, TutorialAction::ControlPositive,
                        QStringLiteral("[Channel1]"), QStringLiteral("play"),
                        0.01, 0, true, 900},
                {QObject::tr("Start Deck 2"),
                        QObject::tr("Press Play on Deck 2. It will take over while the filter clears space in Deck 1."),
                        QStringLiteral("PlayDeck"), QStringLiteral("Deck2_Src"),
                        {}, {}, {}, TutorialAction::ControlPositive,
                        QStringLiteral("[Channel2]"), QStringLiteral("play"),
                        0.01, 0, false, 900},
                {QObject::tr("Favor Deck 1"),
                        QObject::tr("Move the crossfader nearly all the way left. This gives you a clear starting point for the transition."),
                        {}, {}, QStringLiteral("crossfader"),
                        {}, {}, TutorialAction::ControlBelow,
                        QStringLiteral("[Master]"), QStringLiteral("crossfader"),
                        0.05, 0, false, 700, -0.75},
                {QObject::tr("Explore the filter sweep"),
                        QObject::tr("Slowly turn Deck 1's filter to the right and back toward center. Listen for the bass disappearing. Choose Next when you are ready."),
                        {}, QStringLiteral("MixerChannel_2Decks_Left"),
                        QStringLiteral("QuickEffectRack_super1"),
                        {}, {}, TutorialAction::ControlChanged,
                        QStringLiteral("[QuickEffectRack1_[Channel1]]"),
                        QStringLiteral("super1"),
                        0.04, 0, false, 300, 0.0, true},
                {QObject::tr("Bring in Deck 2"),
                        QObject::tr("Move the crossfader to the center. Deck 2 joins while Deck 1 has less bass, creating space for both tracks."),
                        {}, {}, QStringLiteral("crossfader"),
                        {}, {}, TutorialAction::ControlNear,
                        QStringLiteral("[Master]"), QStringLiteral("crossfader"),
                        0.12, 0, false, 1100, 0.0},
                {QObject::tr("Complete the transition"),
                        QObject::tr("Move the crossfader nearly all the way right. Deck 2 now owns the mix and the filtered track can leave cleanly."),
                        {}, {}, QStringLiteral("crossfader"),
                        {}, {}, TutorialAction::ControlAbove,
                        QStringLiteral("[Master]"), QStringLiteral("crossfader"),
                        0.05, 0, false, 1500, 0.75},
        };
    }
    return {};
}

QHash<QString, double> tutorialStateRequirements(
        const QString& tutorialId, int step, bool stepCompleted) {
    QHash<QString, double> requirements;
    const QList<TutorialGuideStep> steps = tutorialGuideSteps(tutorialId);
    if (step < 0 || step >= steps.size()) {
        return requirements;
    }

    for (int previous = 0; previous < step; ++previous) {
        const TutorialGuideStep& earlier = steps.at(previous);
        if (earlier.action == TutorialAction::ControlPositive &&
                earlier.actionItem == QStringLiteral("play") &&
                !earlier.actionGroup.isEmpty()) {
            requirements.insert(earlier.actionGroup, 1.0);
        }
    }

    const TutorialGuideStep& current = steps.at(step);
    if (current.action == TutorialAction::ControlPositive &&
            current.actionItem == QStringLiteral("play") &&
            !current.actionGroup.isEmpty()) {
        if (stepCompleted) {
            requirements.insert(current.actionGroup, 1.0);
        } else {
            // The learner must be able to operate the currently highlighted
            // Play button, even if this deck was played earlier in the lesson.
            requirements.remove(current.actionGroup);
        }
    }

    if (current.pauseOnEnter && !stepCompleted) {
        for (auto it = requirements.begin(); it != requirements.end(); ++it) {
            it.value() = 0.0;
        }
    }
    if (current.actionItem == QStringLiteral("cue_default") && stepCompleted &&
            !current.actionGroup.isEmpty()) {
        requirements.insert(current.actionGroup, 0.0);
    }
    return requirements;
}
} // namespace

class TutorialFocusOverlay final : public QWidget {
  public:
    explicit TutorialFocusOverlay(QWidget* pParent)
            : QWidget(pParent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setFocusPolicy(Qt::NoFocus);
        pParent->installEventFilter(this);
        setGeometry(pParent->rect());
    }

    void setTarget(QWidget* pTarget,
            const QString& title,
            const QString& detail,
            int step,
            int stepCount) {
        m_pTarget = pTarget;
        m_title = title;
        m_detail = detail;
        m_step = step;
        m_stepCount = stepCount;
        m_result = false;
        setGeometry(parentWidget()->rect());
        show();
        raise();
        update();
    }

    void setResult(const QString& title, const QString& detail) {
        m_pTarget.clear();
        m_title = title;
        m_detail = detail;
        m_result = true;
        setGeometry(parentWidget()->rect());
        show();
        raise();
        update();
    }

  protected:
    bool eventFilter(QObject* pObject, QEvent* pEvent) override {
        if (pObject == parentWidget() && pEvent->type() == QEvent::Resize) {
            setGeometry(parentWidget()->rect());
            update();
        }
        return QWidget::eventFilter(pObject, pEvent);
    }

    void paintEvent(QPaintEvent*) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QRect focusRect;
        if (!m_result && m_pTarget) {
            const QPoint topLeft = mapFromGlobal(m_pTarget->mapToGlobal(QPoint(0, 0)));
            focusRect = QRect(topLeft, m_pTarget->size())
                                .adjusted(-9, -9, 9, 9)
                                .intersected(rect().adjusted(6, 6, -6, -6));
        }
        if (!m_result && focusRect.isEmpty()) {
            focusRect = QRect(width() / 2 - 50, height() / 2 - 30, 100, 60);
        }

        if (m_result) {
            painter.fillRect(rect(), QColor(2, 4, 9, 210));
        } else {
            QPainterPath shade;
            shade.setFillRule(Qt::OddEvenFill);
            shade.addRect(rect());
            shade.addRoundedRect(focusRect, 10, 10);
            painter.fillPath(shade, QColor(2, 4, 9, 188));

            painter.setPen(QPen(QColor(167, 139, 250), 3));
            painter.drawRoundedRect(focusRect, 10, 10);
        }

        const int bubbleWidth = qMin(430, qMax(280, width() - 32));
        QFont titleFont = font();
        titleFont.setBold(true);
        titleFont.setPointSize(titleFont.pointSize() + 3);
        QFont detailFont = font();
        detailFont.setPointSize(detailFont.pointSize() + 1);
        const QFontMetrics titleMetrics(titleFont);
        const QFontMetrics detailMetrics(detailFont);
        const QRect detailBounds = detailMetrics.boundingRect(
                QRect(0, 0, bubbleWidth - 36, 1000),
                Qt::TextWordWrap,
                m_detail);
        const int bubbleHeight =
                24 + titleMetrics.height() + 8 + detailBounds.height() + 20;

        int bubbleX = m_result ? (width() - bubbleWidth) / 2
                               : focusRect.center().x() - bubbleWidth / 2;
        int bubbleY = m_result ? (height() - bubbleHeight) / 2
                               : focusRect.bottom() + 20;
        if (bubbleY + bubbleHeight > height() - 16) {
            bubbleY = focusRect.top() - bubbleHeight - 20;
        }
        if (bubbleY < 16) {
            bubbleY = qBound(16,
                    focusRect.center().y() - bubbleHeight / 2,
                    qMax(16, height() - bubbleHeight - 16));
            bubbleX = focusRect.right() + 20;
            if (bubbleX + bubbleWidth > width() - 16) {
                bubbleX = focusRect.left() - bubbleWidth - 20;
            }
        }
        bubbleX = qBound(16, bubbleX, qMax(16, width() - bubbleWidth - 16));
        const QRect bubbleRect(bubbleX, bubbleY, bubbleWidth, bubbleHeight);

        painter.setPen(QPen(QColor(167, 139, 250), 2));
        if (!m_result) {
            painter.drawLine(bubbleRect.center(), focusRect.center());
        }
        painter.setBrush(QColor(20, 16, 36, 248));
        painter.drawRoundedRect(bubbleRect, 14, 14);

        const QRect content = bubbleRect.adjusted(18, 14, -18, -14);
        painter.setFont(titleFont);
        painter.setPen(QColor(255, 255, 255));
        const QString heading = m_result
                ? m_title
                : QStringLiteral("%1/%2  %3")
                          .arg(m_step + 1)
                          .arg(m_stepCount)
                          .arg(m_title);
        painter.drawText(content.left(),
                content.top() + titleMetrics.ascent(),
                heading);
        painter.setFont(detailFont);
        painter.setPen(QColor(224, 219, 243));
        painter.drawText(QRect(content.left(),
                                 content.top() + titleMetrics.height() + 8,
                                 content.width(),
                                 detailBounds.height()),
                Qt::TextWordWrap,
                m_detail);
    }

  private:
    QPointer<QWidget> m_pTarget;
    QString m_title;
    QString m_detail;
    int m_step{0};
    int m_stepCount{0};
    bool m_result{false};
};

MixxxMainWindow::MixxxMainWindow(std::shared_ptr<mixxx::CoreServices> pCoreServices)
        : m_pCoreServices(pCoreServices),
          m_pCentralWidget(nullptr),
          m_pLaunchImage(nullptr),
#ifndef __APPLE__
          m_prevState(Qt::WindowNoState),
#endif
          m_noVinylInputDialog(nullptr),
          m_noPassthroughInputDialog(nullptr),
          m_noMicInputDialog(nullptr),
          m_noAuxInputDialog(nullptr),
          m_pGuiTick(nullptr),
#ifdef __LINUX__
          m_supportsGlobalMenuBar(mixxx::desktopSupportsGlobalMenuBar()),
#endif
          m_inRebootMixxxView(false),
          m_pDeveloperToolsDlg(nullptr),
          m_pPrefDlg(nullptr),
          m_toolTipsCfg(mixxx::preferences::Tooltips::On) {
    DEBUG_ASSERT(pCoreServices);
    // These depend on the settings
#ifdef __LINUX__
    // If the desktop features a global menubar and we'll go fullscreen during
    // startup, set Qt::AA_DontUseNativeMenuBar so the menubar is placed in the
    // window like it's done in slotViewFullScreen(). On other desktops this
    // attribute has no effect. This is a safe alternative to setNativeMenuBar()
    // which can cause a crash when using menu shortcuts like Alt+F after resetting
    // the menubar. See https://github.com/mixxxdj/mixxx/issues/11320
    if (m_supportsGlobalMenuBar) {
        bool fullscreenPref = m_pCoreServices->getSettings()->getValue<bool>(
                ConfigKey("[Config]", "StartInFullscreen"));
        QApplication::setAttribute(
                Qt::AA_DontUseNativeMenuBar,
                CmdlineArgs::Instance().getStartInFullscreen() || fullscreenPref);
    }
#endif // __LINUX__

    connect(m_pCoreServices.get(),
            &mixxx::CoreServices::libraryScanSummary,
            this,
            &MixxxMainWindow::slotLibraryScanSummaryDlg);

    createMenuBar();
    m_pMenuBar->hide();

    m_pTutorialToolBar = make_parented<QToolBar>(tr("Tutorial navigation"), this);
    m_pTutorialToolBar->setObjectName(QStringLiteral("TutorialNavigation"));
    m_pTutorialToolBar->setMovable(false);
    m_pTutorialToolBar->setFloatable(false);
    m_pTutorialToolBar->setAllowedAreas(Qt::TopToolBarArea);
    m_pTutorialToolBar->setStyleSheet(QStringLiteral(
            "QToolBar { background: #0b0d12; border: 0; padding: 7px 10px; } "
            "QPushButton { background: #6847ed; border: 1px solid #896fff; "
            "border-radius: 8px; color: white; font-weight: 700; padding: 7px 13px; } "
            "QPushButton:hover { background: #795af2; } "
            "QPushButton#tutorialCheckNextButton[stepReady=\"true\"] { "
            "background: #17875e; border-color: #49d69c; } "
            "QLabel#levelZeroGuideLabel { color: #f7f8fb; font-size: 13px; "
            "font-weight: 650; padding: 4px 12px; }"));
    auto pBackToMenu =
            make_parented<QPushButton>(tr("←  Back to menu"), m_pTutorialToolBar);
    pBackToMenu->setObjectName(QStringLiteral("backToTutorialMenuButton"));
    pBackToMenu->setAccessibleName(tr("Back to menu"));
    connect(pBackToMenu.get(),
            &QPushButton::clicked,
            this,
            &MixxxMainWindow::showTutorialHome);
    m_pTutorialToolBar->addWidget(pBackToMenu);
    auto pAdminControls =
            make_parented<QPushButton>(tr("Admin: controls"), m_pTutorialToolBar);
    pAdminControls->setObjectName(QStringLiteral("tutorialAdminControlsButton"));
    pAdminControls->setAccessibleName(tr("Admin visibility controls"));
    m_pTutorialToolBar->addSeparator();
    m_pTutorialToolBar->addWidget(pAdminControls);

    m_pTutorialGuideLabel =
            make_parented<QLabel>(m_pTutorialToolBar);
    m_pTutorialGuideLabel->setObjectName(QStringLiteral("levelZeroGuideLabel"));
    m_pTutorialGuideLabel->setWordWrap(false);
    m_pTutorialGuideLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_pTutorialToolBar->addSeparator();
    m_pTutorialToolBar->addWidget(m_pTutorialGuideLabel);
    m_pTutorialGuideLabel->hide();
    m_pTutorialCheckNext = make_parented<QPushButton>(
            tr("Check step  →"), m_pTutorialToolBar);
    m_pTutorialCheckNext->setObjectName(
            QStringLiteral("tutorialCheckNextButton"));
    m_pTutorialCheckNext->setAccessibleName(tr("Check tutorial step"));
    m_pTutorialCheckNext->setProperty("stepReady", false);
    m_pTutorialCheckNextAction =
            m_pTutorialToolBar->addWidget(m_pTutorialCheckNext);
    m_pTutorialCheckNextAction->setVisible(false);
    connect(m_pTutorialCheckNext.get(), &QPushButton::clicked, this, [this] {
        const QList<TutorialGuideStep> steps =
                tutorialGuideSteps(m_activeTutorialId);
        if (m_tutorialGuideStep < 0 || m_tutorialGuideStep >= steps.size()) {
            return;
        }
        const TutorialGuideStep& guideStep = steps.at(m_tutorialGuideStep);
        if (guideStep.action == TutorialAction::Acknowledge) {
            m_tutorialStepCompleted = true;
            if (m_tutorialGuideStep + 1 >= steps.size()) {
                finishTutorialSession();
            } else {
                showTutorialGuideStep(m_tutorialGuideStep + 1);
            }
            return;
        }
        if (!m_tutorialStepCompleted) {
            m_pTutorialGuideLabel->setText(
                    tr("LEARN  ·  Try the highlighted control before continuing"));
            if (m_pTutorialFocusOverlay) {
                QWidget* pTarget = findTutorialGuideTarget(guideStep.objectName,
                        guideStep.within,
                        guideStep.tooltipId,
                        guideStep.controlKey,
                        guideStep.widgetType);
                m_pTutorialFocusOverlay->setTarget(pTarget,
                        tr("Not quite yet"),
                        tr("Move the highlighted control first. Once LeetDJ detects it, you can keep experimenting and choose Next when you are ready."),
                        m_tutorialGuideStep,
                        steps.size());
            }
            const QString tutorialId = m_activeTutorialId;
            const int currentStep = m_tutorialGuideStep;
            QTimer::singleShot(1400, this, [this, tutorialId, currentStep] {
                if (m_activeTutorialId != tutorialId ||
                        m_tutorialGuideStep != currentStep ||
                        m_tutorialStepCompleted) {
                    return;
                }
                const QList<TutorialGuideStep> currentSteps =
                        tutorialGuideSteps(m_activeTutorialId);
                if (currentStep < 0 || currentStep >= currentSteps.size()) {
                    return;
                }
                const TutorialGuideStep& current = currentSteps.at(currentStep);
                m_pTutorialGuideLabel->setText(
                        tr("LEARN  ·  %1 of %2  ·  %3")
                                .arg(currentStep + 1)
                                .arg(currentSteps.size())
                                .arg(current.title));
                if (m_pTutorialFocusOverlay) {
                    QWidget* pTarget = findTutorialGuideTarget(current.objectName,
                            current.within,
                            current.tooltipId,
                            current.controlKey,
                            current.widgetType);
                    m_pTutorialFocusOverlay->setTarget(pTarget,
                            current.title,
                            current.detail,
                            currentStep,
                            currentSteps.size());
                }
            });
            return;
        }
        if (m_tutorialGuideStep + 1 >= steps.size()) {
            finishTutorialSession();
        } else {
            showTutorialGuideStep(m_tutorialGuideStep + 1);
        }
    });
    m_pTutorialStepTimer = make_parented<QTimer>(this);
    m_pTutorialStepTimer->setInterval(100);
    connect(m_pTutorialStepTimer,
            &QTimer::timeout,
            this,
            &MixxxMainWindow::updateTutorialStepTimer);
    m_pTutorialStateTimer = make_parented<QTimer>(this);
    m_pTutorialStateTimer->setInterval(100);
    connect(m_pTutorialStateTimer,
            &QTimer::timeout,
            this,
            &MixxxMainWindow::updateTutorialStateGuards);
    addToolBar(Qt::TopToolBarArea, m_pTutorialToolBar);
    m_pTutorialToolBar->hide();

    connect(this, &MixxxMainWindow::skinLoaded, this, [this] {
        if (!m_showTutorialHomeWhenSkinLoaded) {
            return;
        }
        m_showTutorialHomeWhenSkinLoaded = false;
        QTimer::singleShot(0, this, &MixxxMainWindow::showTutorialHome);
    });

    m_pTutorialVisibilityPanel =
            make_parented<mixxx::tutorial::VisibilityPanel>(this);
    addDockWidget(Qt::RightDockWidgetArea, m_pTutorialVisibilityPanel);
    m_pTutorialVisibilityPanel->setFloating(true);
    m_pTutorialVisibilityPanel->resize(620, 800);
    m_pTutorialVisibilityPanel->hide();
    connect(pAdminControls.get(), &QPushButton::clicked, this, [this] {
        m_pTutorialVisibilityPanel->setVisible(
                !m_pTutorialVisibilityPanel->isVisible());
    });

    initializeWindow();

    // Show launch image immediately so the user knows Mixxx is starting
    m_pSkinLoader = std::make_unique<mixxx::skin::SkinLoader>(m_pCoreServices->getSettings());
    m_pLaunchImage = m_pSkinLoader->loadLaunchImage(this);
    m_pCentralWidget = (QWidget*)m_pLaunchImage;
    setCentralWidget(m_pCentralWidget);

    show();

    m_pGuiTick = new GuiTick();
    m_pVisualsManager = new VisualsManager();
}

#ifdef MIXXX_USE_QOPENGL
void MixxxMainWindow::initializeQOpenGL() {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    // Qt 6 will nno longer crash if no GL is available and
    // QGLFormat::hasOpenGL() has been removed.
    if (!CmdlineArgs::Instance().getSafeMode() && QGLFormat::hasOpenGL()) {
#else
    // With EGLFS there is always exactly one native window and one EGL window surface
    // OpenGL windows cannot be embedded into our QWidgets main window we already have.
    // https://doc.qt.io/qt-6/embedded-linux.html
    bool isEglfs = QGuiApplication::platformName() == "eglfs";

    if (!CmdlineArgs::Instance().getSafeMode() && !isEglfs) {
#endif
        QOpenGLContext context;
        context.setFormat(WaveformWidgetFactory::getSurfaceFormat(m_pCoreServices->getSettings()));
        if (context.create()) {
            std::pair version = context.format().version();
            qDebug().noquote()
                    << "QOpenGLContext created:"
                    << QGuiApplication::platformName()
                    << context.format().renderableType()
                    << QString("V%1.%2").arg(QString::number(version.first),
                               QString::number(version.second))
                    << context.format().profile();
            // This widget and its QOpenGLWindow will be used to query QOpenGL
            // information (version, driver, etc) in WaveformWidgetFactory.
            // The "SharedGLContext" terminology here doesn't really apply,
            // but allows us to take advantage of the existing classes.
            auto pWidget = make_parented<WInitialGLWidget>(this);
            pWidget->setGeometry(QRect(0, 0, 3, 3));
            SharedGLContext::setWidget(pWidget);
            // When the widget's QOpenGLWindow has been initialized, we continue
            // with the actual initialization
            connect(pWidget, &WInitialGLWidget::onInitialized, this, &MixxxMainWindow::initialize);
            pWidget->show();
            return;
        }
        qDebug() << "QOpenGLContext::create() failed";
    }
    qInfo() << "Initializing without OpenGL";
    initialize();
}
#endif

void MixxxMainWindow::initialize() {
    m_pCoreServices->getControlIndicatorTimer()->setLegacyVsyncEnabled(true);

    UserSettingsPointer pConfig = m_pCoreServices->getSettings();

    // Set the visibility of tooltips, default "1" = ON
    m_toolTipsCfg = pConfig->getValue(
            ConfigKey("[Controls]", "Tooltips"),
            mixxx::preferences::Tooltips::On);
#ifdef MIXXX_USE_QOPENGL
    ToolTipQOpenGL::singleton().setActive(
            m_toolTipsCfg == mixxx::preferences::Tooltips::On);
#endif

#ifdef __ENGINEPRIME__
    // Initialise library exporter
    // This has to be done before switching to fullscreen
    m_pLibraryExporter = m_pCoreServices->getLibrary()->makeLibraryExporter(this);
    connect(m_pCoreServices->getLibrary().get(),
            &Library::exportLibrary,
            m_pLibraryExporter.get(),
            &mixxx::LibraryExporter::slotRequestExport);
    connect(m_pCoreServices->getLibrary().get(),
            &Library::exportCrate,
            m_pLibraryExporter.get(),
            &mixxx::LibraryExporter::slotRequestExportWithInitialCrate);
    connect(m_pCoreServices->getLibrary().get(),
            &Library::exportPlaylist,
            m_pLibraryExporter.get(),
            &mixxx::LibraryExporter::slotRequestExportWithInitialPlaylist);
#endif

    // Turn on fullscreen mode
    // if we were told to start in fullscreen mode on the command-line
    // or if the user chose to always start in fullscreen mode.
    // The Fullscreen menu item is refreshed in connectMenuBar()
    bool fullscreenPref = m_pCoreServices->getSettings()->getValue<bool>(
            ConfigKey("[Config]", "StartInFullscreen"));
    if ((CmdlineArgs::Instance().getStartInFullscreen() || fullscreenPref) &&
            // could be we're fullscreen already after setGeomtery(previousGeometry)
            !isFullScreen()) {
        showFullScreen();
    }

    initializationProgressUpdate(65, tr("skin"));

    // Install an event filter to catch certain QT events, such as tooltips.
    // This allows us to turn off tooltips.
    installEventFilter(m_pCoreServices->getKeyboardEventFilter().get());

    auto pPlayerManager = m_pCoreServices->getPlayerManager();
    DEBUG_ASSERT(pPlayerManager);
    const QStringList visualGroups = pPlayerManager->getVisualPlayerGroups();
    for (const QString& group : visualGroups) {
        m_pVisualsManager->addDeck(group);
    }
    connect(pPlayerManager.get(),
            &PlayerManagerInterface::numberOfDecksChanged,
            this,
            [this](int decks) {
                for (int i = 0; i < decks; ++i) {
                    QString group = PlayerManager::groupForDeck(i);
                    m_pVisualsManager->addDeckIfNotExist(group);
                }
            });
    connect(pPlayerManager.get(),
            &PlayerManagerInterface::numberOfSamplersChanged,
            this,
            [this](int decks) {
                for (int i = 0; i < decks; ++i) {
                    QString group = PlayerManager::groupForSampler(i);
                    m_pVisualsManager->addDeckIfNotExist(group);
                }
            });

#ifndef MIXXX_USE_QOPENGL
    // Before creating the first skin we need to create a QGLWidget so that all
    // the QGLWidget's we create can use it as a shared QGLContext.
    if (!CmdlineArgs::Instance().getSafeMode() && QGLFormat::hasOpenGL()) {
        QGLFormat glFormat;
        glFormat.setDirectRendering(true);
        glFormat.setDoubleBuffer(true);
        glFormat.setDepth(false);
        // Disable waiting for vertical Sync
        // This can be enabled when using a single Threads for each QGLContext
        // Setting 1 causes QGLContext::swapBuffer to sleep until the next VSync
#if defined(__APPLE__)
        // On OS X, syncing to vsync has good performance FPS-wise and
        // eliminates tearing.
        glFormat.setSwapInterval(1);
#else
        // Otherwise, turn VSync off because it could cause horrible FPS on
        // Linux.
        // TODO(XXX): Make this configurable.
        // TODO(XXX): What should we do on Windows?
        glFormat.setSwapInterval(0);
#endif
        glFormat.setRgba(true);
        QGLFormat::setDefaultFormat(glFormat);

        WGLWidget* pContextWidget = new WGLWidget(this);
        pContextWidget->setGeometry(QRect(0, 0, 3, 3));
        pContextWidget->hide();
        SharedGLContext::setWidget(pContextWidget);
    }
#endif

    WaveformWidgetFactory::createInstance(); // takes a long time
    WaveformWidgetFactory::instance()->setConfig(m_pCoreServices->getSettings());
    WaveformWidgetFactory::instance()->startVSync(m_pGuiTick, m_pVisualsManager, false);

    connect(this,
            &MixxxMainWindow::skinLoaded,
            m_pCoreServices->getLibrary().get(),
            &Library::onSkinLoadFinished);

    connect(this,
            &MixxxMainWindow::skinLoaded,
            WaveformWidgetFactory::instance(),
            &WaveformWidgetFactory::slotSkinLoaded);

    // Initialize preference dialog
    m_pPrefDlg = new DlgPreferences(
            m_pCoreServices->getScreensaverManager(),
            m_pSkinLoader,
            m_pCoreServices->getSoundManager(),
            m_pCoreServices->getControllerManager(),
            m_pCoreServices->getVinylControlManager(),
            m_pCoreServices->getEffectsManager(),
            m_pCoreServices->getSettingsManager(),
            m_pCoreServices->getLibrary());
    m_pPrefDlg->setWindowIcon(QIcon(MIXXX_ICON_PATH));
    m_pPrefDlg->setHidden(true);
    connect(m_pPrefDlg,
            &DlgPreferences::tooltipModeChanged,
            this,
            &MixxxMainWindow::slotTooltipModeChanged);
    connect(m_pPrefDlg,
            &DlgPreferences::reloadUserInterface,
            this,
            &MixxxMainWindow::rebootMixxxView,
            Qt::DirectConnection);
#ifndef __APPLE__
    connect(m_pPrefDlg,
            &DlgPreferences::menuBarAutoHideChanged,
            this,
            &MixxxMainWindow::slotUpdateMenuBarAltKeyConnection,
            Qt::DirectConnection);
#endif

    // Connect signals to the menubar. Should be done before emit skinLoaded.
    connectMenuBar();

    QWidget* oldWidget = m_pCentralWidget;

    tryParseAndSetDefaultStyleSheet();

    if (!loadConfiguredSkin()) {
        reportCriticalErrorAndQuit(
                "default skin cannot be loaded - see <b>mixxx</b> trace for more information");
        m_pCentralWidget = oldWidget;
        //TODO (XXX) add dialog to warn user and launch skin choice page
    } else {
        m_pMenuBar->setStyleSheet(m_pCentralWidget->styleSheet());
    }

    // Check direct rendering and warn user if they don't have it
    if (!CmdlineArgs::Instance().getSafeMode()) {
        checkDirectRendering();
    }

    // Sound hardware setup
    // Try to open configured devices. If that fails, display dialogs
    // that allow to either retry, reconfigure devices or exit.
    bool retryClicked;
    do {
        retryClicked = false;
        SoundDeviceStatus result = m_pCoreServices->getSoundManager()->setupDevices();
        if (result == SoundDeviceStatus::ErrorDeviceCount ||
                result == SoundDeviceStatus::ErrorExcessiveOutputChannel) {
            if (soundDeviceBusyDlg(&retryClicked) != QDialog::Accepted) {
                exit(0);
            }
        } else if (result != SoundDeviceStatus::Ok) {
            if (soundDeviceErrorMsgDlg(result, &retryClicked) !=
                    QDialog::Accepted) {
                exit(0);
            }
        }
    } while (retryClicked);

    // Test for at least one output device. If none, display another dialog
    // that says "mixxx will barely work with no outs".
    // In case of persisting errors, the user has already received a message
    // above. So we can just check the output count here.
    while (m_pCoreServices->getSoundManager()
                    ->getConfig()
                    .getOutputs()
                    .isEmpty() &&
            !m_pCoreServices->getSoundManager()->pipewireSkipConfig()) {
        // Exit when we press the Exit button in the noSoundDlg dialog
        // only call it if result != OK
        bool continueClicked = false;
        if (noOutputDlg(&continueClicked) != QDialog::Accepted) {
            exit(0);
        }
        if (continueClicked) {
            break;
        }
    }

    // The user has either reconfigured devices or accepted no outputs,
    // so it's now safe to write the new config to disk.
    m_pCoreServices->getSoundManager()->getConfig().writeToDisk();

    // this has to be after the OpenGL widgets are created or depending on a
    // million different variables the first waveform may be horribly
    // corrupted. See bug 521509 -- bkgood ?? -- vrince
    setCentralWidget(m_pCentralWidget);

#ifndef __APPLE__
    // Ask for permission to auto-hide the menu bar if applicable.
#ifdef __LINUX__
    // This makes no sense when starting in windowed mode with a global menu,
    // we'll ask when going fullscreen.
    if (!m_supportsGlobalMenuBar || isFullScreen()) {
        alwaysHideMenuBarDlg();
        slotUpdateMenuBarAltKeyConnection();
    }
#else
    alwaysHideMenuBarDlg();
    slotUpdateMenuBarAltKeyConnection();
#endif
#endif

    // Show the menubar after the launch image is replaced by the skin widget,
    // otherwise it would shift the launch image shortly before the skin is visible.
    m_pMenuBar->show();

    // The launch image widget is automatically disposed, but we still have a
    // pointer to it.
    m_pLaunchImage = nullptr;

    connect(pPlayerManager.get(),
            &PlayerManager::noMicrophoneInputConfigured,
            this,
            &MixxxMainWindow::slotNoMicrophoneInputConfigured);
    connect(pPlayerManager.get(),
            &PlayerManager::noAuxiliaryInputConfigured,
            this,
            &MixxxMainWindow::slotNoAuxiliaryInputConfigured);
    connect(pPlayerManager.get(),
            &PlayerManager::noDeckPassthroughInputConfigured,
            this,
            &MixxxMainWindow::slotNoDeckPassthroughInputConfigured);
    connect(pPlayerManager.get(),
            &PlayerManager::noVinylControlInputConfigured,
            this,
            &MixxxMainWindow::slotNoVinylControlInputConfigured);

    connect(&PlayerInfo::instance(),
            &PlayerInfo::currentPlayingTrackChanged,
            this,
            &MixxxMainWindow::slotUpdateWindowTitle);

    // Start Auto DJ if the cmdline arg is passed.
    if (CmdlineArgs::Instance().getStartAutoDJ()) {
        qDebug("Enabling Auto DJ from CLI flag.");
        ControlObject::set(ConfigKey("[AutoDJ]", "enabled"), 1.0);
        // Switch to Auto DJ feature
        auto* pLibrary = m_pCoreServices->getLibrary().get();
        // Note: auto-scroll is disabled but that doesn't really matter here
        // because the sidebar is still in its initial state (top feature visible,
        // AutoDj is second from the top by default, all features collapsed).
        pLibrary->showAutoDJ();
    }
}

MixxxMainWindow::~MixxxMainWindow() {
    Timer t("~MixxxMainWindow");
    t.start();

    // Save the current window state (position, maximized, etc)
    // Note(ronso0): Unfortunately saveGeometry() also stores the fullscreen state.
    // On next start restoreGeometry would enable fullscreen mode even though that
    // might not be requested (no '--fullscreen' command line arg and
    // [Config],StartInFullscreen is '0'.
    // https://github.com/mixxxdj/mixxx/issues/10005
    // So let's quit fullscreen if StartInFullscreen is not checked in Preferences.
    bool fullscreenPref = m_pCoreServices->getSettings()->getValue<bool>(
            ConfigKey("[Config]", "StartInFullscreen"));
    if (isFullScreen() && !fullscreenPref) {
        // Simply maximize the window so we can store a geometry that fits the screen.
        // Don't call slotViewFullScreen(false) (calls showNormal()) because that
        // can make the main window incl. window decoration too large for the screen.
#ifndef __APPLE__
        // Before, store the expected window state so eventFilter() will ignore
        // the following QWindowChangeEvent and not recreate & re-sync the menu bar.
        m_prevState = Qt::WindowMaximized;
#endif
        showMaximized();
    }
    m_pCoreServices->getSettings()->set(ConfigKey("[MainWindow]", "geometry"),
            QString(saveGeometry().toBase64()));
    m_pCoreServices->getSettings()->set(ConfigKey("[MainWindow]", "state"),
            QString(saveState().toBase64()));

    if (m_pTutorialVisibility) {
        m_pTutorialVisibilityPanel->setController(nullptr);
        m_pTutorialVisibility->restore();
        m_pTutorialVisibility.reset();
    }

    // The DJ skin is detached while the embedded tutorial home is visible.
    // Restore it as the central widget so the standard skin teardown below
    // continues to own and dispose it correctly.
    if (m_pCentralWidget && centralWidget() != m_pCentralWidget) {
        QWidget* pTutorialHome = takeCentralWidget();
        setCentralWidget(m_pCentralWidget);
        delete pTutorialHome;
    }

    // GUI depends on KeyboardEventFilter, PlayerManager, Library
    qDebug() << t.elapsed(false).debugMillisWithUnit() << "deleting skin";
    // Clear widget pointer list and destroy all update connections before we
    // delete the main widget (ie. all WBaseWidgets) to prevent KeyboardEventFilter
    // accessing dangling pointers.
    m_pCoreServices->getKeyboardEventFilter()->clearWidgets();
    m_pCentralWidget = nullptr;
    QPointer<QWidget> pSkin(centralWidget());
    setCentralWidget(nullptr);
    if (!pSkin.isNull()) {
        QCoreApplication::sendPostedEvents(pSkin, QEvent::DeferredDelete);
    }
    // Our central widget is now deleted.
    VERIFY_OR_DEBUG_ASSERT(pSkin.isNull()) {
        qWarning() << "Central widget was not deleted by our sendPostedEvents trick.";
    }

    // Delete Controls created by skins
    qDeleteAll(m_skinCreatedControls);
    m_skinCreatedControls.clear();

    // TODO() Verify if this comment still applies:
    // WMainMenuBar holds references to controls so we need to delete it
    // before MixxxMainWindow is destroyed. QMainWindow calls deleteLater() in
    // setMenuBar() but we need to delete it now so we can ask for
    // DeferredDelete events to be processed for it. Once Mixxx shutdown lives
    // outside of MixxxMainWindow the parent relationship will directly destroy
    // the WMainMenuBar and this will no longer be a problem.
    qDebug() << t.elapsed(false).debugMillisWithUnit() << "deleting menubar";
    // Clear action pointer list before we delete the menubar
    // to prevent KeyboardEventFilter accessing dangling pointers.
    m_pCoreServices->getKeyboardEventFilter()->clearMenuBarActions();
    QPointer<WMainMenuBar> pMenuBar = m_pMenuBar.toWeakRef();
    DEBUG_ASSERT(menuBar() == m_pMenuBar.get());
    // We need to reset the parented pointer here that it does not become a
    // dangling pointer after the object has been deleted.
    m_pMenuBar = nullptr;
    setMenuBar(nullptr);
    if (!pMenuBar.isNull()) {
        QCoreApplication::sendPostedEvents(pMenuBar, QEvent::DeferredDelete);
    }
    // Our main menu is now deleted.
    VERIFY_OR_DEBUG_ASSERT(pMenuBar.isNull()) {
        qWarning() << "WMainMenuBar was not deleted by our sendPostedEvents trick.";
    }

    qDebug() << t.elapsed(false).debugMillisWithUnit() << "deleting DeveloperToolsDlg";
    delete m_pDeveloperToolsDlg;

#ifdef __ENGINEPRIME__
    qDebug() << t.elapsed(false).debugMillisWithUnit() << "deleting LibraryExporter";
    m_pLibraryExporter.reset();
#endif

    qDebug() << t.elapsed(false).debugMillisWithUnit() << "deleting DlgPreferences";
    delete m_pPrefDlg;

    m_pCoreServices->getControlIndicatorTimer()->setLegacyVsyncEnabled(false);

    qDebug() << t.elapsed(false).debugMillisWithUnit() << "deleting ControllerManager";

    WaveformWidgetFactory::destroy();

    delete m_pGuiTick;
    delete m_pVisualsManager;
}

void MixxxMainWindow::showTutorialHome() {
    if (m_pLaunchImage && centralWidget() == m_pLaunchImage) {
        m_showTutorialHomeWhenSkinLoaded = true;
        return;
    }
    if (!m_pCentralWidget || m_pTutorialHomePage) {
        return;
    }

    resetTutorialSession();
    if (m_pTutorialVisibility) {
        m_pTutorialVisibilityPanel->setController(nullptr);
        m_pTutorialVisibility->restore();
        m_pTutorialVisibility.reset();
    }
    m_activeTutorialId.clear();
#ifdef MIXXX_USE_QOPENGL
    ToolTipQOpenGL::singleton().setActive(
            m_toolTipsCfg == mixxx::preferences::Tooltips::On);
#endif
    if (m_pTutorialFocusOverlay) {
        m_pTutorialFocusOverlay->hide();
    }
    m_pTutorialGuideLabel->hide();
    m_pTutorialCheckNextAction->setVisible(false);
    m_pTutorialVisibilityPanel->hide();
    m_pTutorialToolBar->hide();
    m_pMenuBar->setEnabled(true);
    for (QAction* pAction : m_pMenuBar->actions()) {
        pAction->setVisible(true);
    }
    m_pMenuBar->show();
    setWindowTitle(tr("LeetDJ"));

    QWidget* pDjWorkspace = takeCentralWidget();
    VERIFY_OR_DEBUG_ASSERT(pDjWorkspace == m_pCentralWidget) {
        if (pDjWorkspace) {
            setCentralWidget(pDjWorkspace);
        }
        return;
    }
    pDjWorkspace->hide();
    pDjWorkspace->setParent(this);

    auto pTutorialHome = make_parented<TutorialHomePage>(this);
    m_pTutorialHomePage = pTutorialHome.get();
    connect(pTutorialHome.get(),
            &TutorialHomePage::openDjWorkspaceRequested,
            this,
            &MixxxMainWindow::showDjWorkspace);
    setCentralWidget(pTutorialHome);
    m_pTutorialHomePage->show();
}

void MixxxMainWindow::showDjWorkspace(const QString& tutorialId) {
    if (!m_pCentralWidget || !m_pTutorialHomePage) {
        return;
    }

    QWidget* pTutorialHome = takeCentralWidget();
    m_pTutorialHomePage.clear();
    m_pCentralWidget->setParent(this);
    setCentralWidget(m_pCentralWidget);
    m_pCentralWidget->show();

    m_activeTutorialId = tutorialId;
    if (!m_activeTutorialId.isEmpty()) {
        QToolTip::hideText();
#ifdef MIXXX_USE_QOPENGL
        ToolTipQOpenGL::singleton().setActive(false);
#endif
    }
    m_pTutorialVisibility =
            std::make_unique<mixxx::tutorial::VisibilityController>(m_pCentralWidget);
    const QString profilePath = QDir(m_pCoreServices->getSettings()->getResourcePath())
                                        .filePath(QStringLiteral(
                                                "tutorials/visibility_profiles.json"));
    QString error;
    QString visibilityProfileId = tutorialId;
    if (tutorialId == QStringLiteral("crossfader") ||
            tutorialId == QStringLiteral("channel-faders")) {
        visibilityProfileId = QStringLiteral("level-zero");
    } else if (tutorialId == QStringLiteral("filter-sweep")) {
        visibilityProfileId = QStringLiteral("bass-eq");
    }
    if (!m_pTutorialVisibility->applyProfile(
                profilePath, visibilityProfileId, &error)) {
        qWarning() << error;
    }
    m_pTutorialVisibilityPanel->setController(m_pTutorialVisibility.get());
    if (QScreen* pScreen = screen()) {
        const QRect available = pScreen->availableGeometry();
        m_pTutorialVisibilityPanel->move(
                qBound(available.left(),
                        frameGeometry().left() + 20,
                        available.right() -
                                m_pTutorialVisibilityPanel->width()),
                available.top() + 40);
    }
    const bool hasGuide = !tutorialGuideSteps(tutorialId).isEmpty();
    m_pMenuBar->setEnabled(!hasGuide);
    for (QAction* pAction : m_pMenuBar->actions()) {
        pAction->setVisible(!hasGuide);
    }
    m_pMenuBar->setVisible(!hasGuide);
    m_pTutorialVisibilityPanel->setVisible(!hasGuide);
    if (!hasGuide) {
        m_pTutorialVisibilityPanel->raise();
    }
    m_pTutorialToolBar->show();
    m_pTutorialGuideLabel->setVisible(hasGuide);
    m_pTutorialCheckNextAction->setVisible(false);
    if (hasGuide) {
        if (!m_pTutorialFocusOverlay ||
                m_pTutorialFocusOverlay->parentWidget() != m_pCentralWidget) {
            m_pTutorialFocusOverlay = new TutorialFocusOverlay(m_pCentralWidget);
        }
        m_pTutorialFocusOverlay->show();
        loadTutorialDemoTracks(tutorialId);
        QTimer::singleShot(600, this, [this] {
            showTutorialGuideStep(0);
        });
    }

    if (pTutorialHome) {
        pTutorialHome->setParent(this);
        pTutorialHome->deleteLater();
    }
}

void MixxxMainWindow::loadTutorialDemoTracks(const QString& tutorialId) {
    if (tutorialId != QStringLiteral("crossfader") &&
            tutorialId != QStringLiteral("channel-faders") &&
            tutorialId != QStringLiteral("filter-sweep") &&
            tutorialId != QStringLiteral("cueing") &&
            tutorialId != QStringLiteral("beatmatching")) {
        return;
    }
    const auto pTrackCollectionManager =
            m_pCoreServices->getTrackCollectionManager();
    const auto pPlayerManager = m_pCoreServices->getPlayerManager();
    if (!pTrackCollectionManager || !pPlayerManager) {
        return;
    }

    QStringList locations = pTrackCollectionManager->internalCollection()
                                    ->getTrackDAO()
                                    .getAllExistingTrackLocations()
                                    .values();
    locations.removeIf([](const QString& location) {
        return !QFileInfo::exists(location);
    });
    const auto tutorialTrackScore = [](const QString& location) {
        const QString lower = location.toLower();
        int score = 0;
        if (lower.contains(QStringLiteral("serato demo tracks"))) {
            score += 100;
        }
        if (lower.contains(QStringLiteral("starter pack"))) {
            score += 50;
        }
        if (lower.contains(QStringLiteral("house track"))) {
            score += 25;
        }
        return score;
    };
    std::sort(locations.begin(),
            locations.end(),
            [&](const QString& left, const QString& right) {
                const int leftScore = tutorialTrackScore(left);
                const int rightScore = tutorialTrackScore(right);
                return leftScore == rightScore ? left < right : leftScore > rightScore;
            });
    if (locations.isEmpty()) {
        return;
    }

    pPlayerManager->slotLoadToDeck(locations.at(0), 1);
    if ((tutorialId == QStringLiteral("crossfader") ||
                tutorialId == QStringLiteral("channel-faders") ||
                tutorialId == QStringLiteral("filter-sweep") ||
                tutorialId == QStringLiteral("beatmatching")) &&
            locations.size() > 1) {
        pPlayerManager->slotLoadToDeck(locations.at(1), 2);
    }
}

QWidget* MixxxMainWindow::findTutorialGuideTarget(const QString& objectName,
        const QString& within,
        const QString& tooltipId,
        const QString& controlKey,
        const QString& widgetType) const {
    if (!m_pCentralWidget) {
        return nullptr;
    }

    QList<QWidget*> scopes;
    if (within.isEmpty()) {
        scopes.append(m_pCentralWidget);
    } else {
        if (m_pCentralWidget->objectName() == within) {
            scopes.append(m_pCentralWidget);
        }
        scopes.append(m_pCentralWidget->findChildren<QWidget*>(within));
    }

    const auto matches = [&](QWidget* pWidget) {
        return pWidget && pWidget->isVisibleTo(m_pCentralWidget) &&
                (objectName.isEmpty() || pWidget->objectName() == objectName) &&
                (tooltipId.isEmpty() ||
                        pWidget->property("mixxxTooltipId").toString() == tooltipId) &&
                (controlKey.isEmpty() ||
                        pWidget->property("mixxxControlKeys")
                                .toStringList()
                                .contains(controlKey)) &&
                (widgetType.isEmpty() ||
                        pWidget->property("mixxxSkinWidgetType").toString() == widgetType);
    };
    for (QWidget* pScope : std::as_const(scopes)) {
        if (matches(pScope)) {
            return pScope;
        }
        const QList<QWidget*> children = pScope->findChildren<QWidget*>();
        for (QWidget* pChild : children) {
            if (matches(pChild)) {
                return pChild;
            }
        }
    }
    return nullptr;
}

void MixxxMainWindow::showTutorialGuideStep(int step) {
    const QList<TutorialGuideStep> steps = tutorialGuideSteps(m_activeTutorialId);
    if (steps.isEmpty() || !m_pTutorialGuideLabel->isVisible()) {
        return;
    }
    m_tutorialGuideStep = qBound(0, step, steps.size() - 1);
    const TutorialGuideStep& guideStep = steps.at(m_tutorialGuideStep);
    m_pTutorialCheckNext->setEnabled(true);
    m_pTutorialCheckNext->setText(tr("Check step  →"));
    m_pTutorialCheckNext->setAccessibleName(tr("Check tutorial step"));
    m_pTutorialCheckNext->setProperty("stepReady", false);
    m_pTutorialCheckNext->style()->unpolish(m_pTutorialCheckNext.get());
    m_pTutorialCheckNext->style()->polish(m_pTutorialCheckNext.get());
    QWidget* pTarget = findTutorialGuideTarget(guideStep.objectName,
            guideStep.within,
            guideStep.tooltipId,
            guideStep.controlKey,
            guideStep.widgetType);

    m_pTutorialGuideLabel->setText(
            tr("LEARN  ·  %1 of %2  ·  %3")
                    .arg(m_tutorialGuideStep + 1)
                    .arg(steps.size())
                    .arg(guideStep.title));
    const bool usesNextButton = tutorialStepUsesNextButton(guideStep);
    m_pTutorialCheckNextAction->setVisible(usesNextButton);
    if (guideStep.action == TutorialAction::Acknowledge) {
        m_pTutorialCheckNext->setText(tr("Continue  →"));
        m_pTutorialCheckNext->setAccessibleName(tr("Continue tutorial"));
        m_pTutorialCheckNext->setProperty("stepReady", true);
    } else if (guideStep.manualAdvance) {
        m_pTutorialCheckNext->setText(tr("Next step  →"));
        m_pTutorialCheckNext->setAccessibleName(tr("Next tutorial step"));
    }
    m_pTutorialCheckNext->style()->unpolish(m_pTutorialCheckNext.get());
    m_pTutorialCheckNext->style()->polish(m_pTutorialCheckNext.get());
    if (m_pTutorialFocusOverlay) {
        m_pTutorialFocusOverlay->setTarget(pTarget,
                guideStep.title,
                guideStep.detail,
                m_tutorialGuideStep,
                steps.size());
    }
    armTutorialStep();
    updateTutorialStateGuards();
    m_pTutorialStateTimer->start();
}

void MixxxMainWindow::armTutorialStep() {
    m_pTutorialStepTimer->stop();
    m_pTutorialActionControl.reset();
    m_tutorialStepElapsedMs = 0;
    m_tutorialPlaybackWaitMs = 0;
    m_tutorialStepCompleted = false;

    const QList<TutorialGuideStep> steps = tutorialGuideSteps(m_activeTutorialId);
    if (m_tutorialGuideStep < 0 || m_tutorialGuideStep >= steps.size()) {
        return;
    }
    const TutorialGuideStep& guideStep = steps.at(m_tutorialGuideStep);
    if (guideStep.pauseOnEnter) {
        pauseTutorialDecks();
    }

    QString actionGroup = guideStep.actionGroup;
    QString actionItem = guideStep.actionItem;
    if (actionGroup.isEmpty() || actionItem.isEmpty()) {
        QWidget* pTarget = findTutorialGuideTarget(guideStep.objectName,
                guideStep.within,
                guideStep.tooltipId,
                guideStep.controlKey,
                guideStep.widgetType);
        const QStringList controlKeys =
                pTarget ? pTarget->property("mixxxControlKeys").toStringList()
                        : QStringList{};
        for (const QString& controlKey : controlKeys) {
            const int separator = controlKey.lastIndexOf(QStringLiteral("],"));
            if (separator > 0 && separator + 2 < controlKey.size()) {
                actionGroup = controlKey.left(separator + 1);
                actionItem = controlKey.mid(separator + 2);
                break;
            }
        }
    }

    if (!actionGroup.isEmpty() && !actionItem.isEmpty()) {
        m_pTutorialActionControl =
                std::make_unique<ControlProxy>(actionGroup, actionItem);
        if (m_pTutorialActionControl->valid()) {
            m_tutorialControlBaseline = m_pTutorialActionControl->get();
            m_pTutorialActionControl->connectValueChanged(
                    this, [this](double value) {
                        handleTutorialControlValue(value);
                    });
        } else {
            m_pTutorialActionControl.reset();
        }
    }
    m_pTutorialStepTimer->start();
}

void MixxxMainWindow::handleTutorialControlValue(double value) {
    if (m_tutorialStepCompleted) {
        return;
    }
    const QList<TutorialGuideStep> steps = tutorialGuideSteps(m_activeTutorialId);
    if (m_tutorialGuideStep < 0 || m_tutorialGuideStep >= steps.size()) {
        return;
    }
    const TutorialGuideStep& guideStep = steps.at(m_tutorialGuideStep);
    switch (guideStep.action) {
    case TutorialAction::ControlChanged:
        if (qAbs(value - m_tutorialControlBaseline) >=
                guideStep.changeThreshold) {
            completeTutorialStep();
        }
        break;
    case TutorialAction::ControlPositive:
        if (value > 0.0) {
            completeTutorialStep();
        }
        break;
    case TutorialAction::ControlBelow:
        if (value <= guideStep.expectedValue) {
            completeTutorialStep();
        }
        break;
    case TutorialAction::ControlAbove:
        if (value >= guideStep.expectedValue) {
            completeTutorialStep();
        }
        break;
    case TutorialAction::ControlNear:
        if (qAbs(value - guideStep.expectedValue) <=
                guideStep.changeThreshold) {
            completeTutorialStep();
        }
        break;
    case TutorialAction::TrackReload:
        if (qAbs(value - m_tutorialControlBaseline) >=
                guideStep.changeThreshold) {
            completeTutorialStep();
        }
        break;
    case TutorialAction::Acknowledge:
    case TutorialAction::Timed:
    case TutorialAction::PlaybackWait:
        break;
    }
}

void MixxxMainWindow::updateTutorialStepTimer() {
    if (m_tutorialStepCompleted) {
        return;
    }
    m_tutorialStepElapsedMs += m_pTutorialStepTimer->interval();
    const QList<TutorialGuideStep> steps = tutorialGuideSteps(m_activeTutorialId);
    if (m_tutorialGuideStep < 0 || m_tutorialGuideStep >= steps.size()) {
        return;
    }
    const TutorialGuideStep& guideStep = steps.at(m_tutorialGuideStep);
    if (guideStep.action == TutorialAction::Timed &&
            m_tutorialStepElapsedMs >= guideStep.waitMs) {
        completeTutorialStep();
        return;
    }
    if (guideStep.action != TutorialAction::PlaybackWait) {
        return;
    }

    if (m_pTutorialActionControl && m_pTutorialActionControl->toBool()) {
        m_tutorialPlaybackWaitMs += m_pTutorialStepTimer->interval();
    }
    const int secondsRemaining = qMax(
            0, (guideStep.waitMs - m_tutorialPlaybackWaitMs + 999) / 1000);
    m_pTutorialGuideLabel->setText(
            tr("LEARN  ·  Listening for the right point… %1s")
                    .arg(secondsRemaining));
    if (m_tutorialPlaybackWaitMs >= guideStep.waitMs) {
        completeTutorialStep();
    }
}

void MixxxMainWindow::completeTutorialStep() {
    if (m_tutorialStepCompleted) {
        return;
    }
    m_tutorialStepCompleted = true;
    m_pTutorialStepTimer->stop();
    updateTutorialStateGuards();

    const QList<TutorialGuideStep> steps = tutorialGuideSteps(m_activeTutorialId);
    if (m_tutorialGuideStep < 0 || m_tutorialGuideStep >= steps.size()) {
        return;
    }
    const TutorialGuideStep& guideStep = steps.at(m_tutorialGuideStep);
    const bool isLastStep = m_tutorialGuideStep + 1 >= steps.size();
    if (guideStep.manualAdvance) {
        m_pTutorialGuideLabel->setText(
                tr("LEARN  ·  ✓ Control detected — explore, then choose Next"));
        m_pTutorialCheckNext->setText(
                isLastStep ? tr("Finish lesson  →") : tr("Next step  →"));
        m_pTutorialCheckNext->setAccessibleName(
                isLastStep ? tr("Finish lesson") : tr("Next tutorial step"));
        m_pTutorialCheckNext->setProperty("stepReady", true);
        m_pTutorialCheckNext->style()->unpolish(m_pTutorialCheckNext.get());
        m_pTutorialCheckNext->style()->polish(m_pTutorialCheckNext.get());
        if (m_pTutorialFocusOverlay) {
            QWidget* pTarget = findTutorialGuideTarget(guideStep.objectName,
                    guideStep.within,
                    guideStep.tooltipId,
                    guideStep.controlKey,
                    guideStep.widgetType);
            m_pTutorialFocusOverlay->setTarget(pTarget,
                    tr("✓ %1").arg(guideStep.title),
                    tr("Nice. Keep trying the highlighted control and listen to what changes. Choose Next when you are ready."),
                    m_tutorialGuideStep,
                    steps.size());
        }
        return;
    }
    m_pTutorialGuideLabel->setText(
            tr("LEARN  ·  ✓ Done — %1")
                    .arg(isLastStep ? tr("lesson complete")
                                    : tr("action complete")));
    if (m_pTutorialFocusOverlay) {
        QWidget* pTarget = findTutorialGuideTarget(guideStep.objectName,
                guideStep.within,
                guideStep.tooltipId,
                guideStep.controlKey,
                guideStep.widgetType);
        m_pTutorialFocusOverlay->setTarget(pTarget,
                tr("✓ %1").arg(guideStep.title),
                guideStep.listenAfterMs >= 1000
                        ? tr("Correct. Listen to the result for a moment.")
                        : tr("Correct."),
                m_tutorialGuideStep,
                steps.size());
    }

    const QString tutorialId = m_activeTutorialId;
    const int completedStep = m_tutorialGuideStep;
    QTimer::singleShot(qMax(500, guideStep.listenAfterMs),
            this,
            [this, tutorialId, completedStep] {
                if (m_activeTutorialId != tutorialId ||
                        m_tutorialGuideStep != completedStep ||
                        !m_tutorialStepCompleted) {
                    return;
                }
                const int stepCount =
                        tutorialGuideSteps(m_activeTutorialId).size();
                if (completedStep + 1 >= stepCount) {
                    finishTutorialSession();
                } else {
                    showTutorialGuideStep(completedStep + 1);
                }
            });
}

void MixxxMainWindow::finishTutorialSession() {
    m_pTutorialStepTimer->stop();
    m_pTutorialStateTimer->stop();
    m_pTutorialActionControl.reset();
    clearTutorialStateGuards();

    const QString title = tr("All done!  ✓");
    const QString detail = tr("You finished the lesson. Keep practicing freely with this focused layout—your songs, controls, and settings will stay exactly as they are.");
    m_pTutorialGuideLabel->setText(title);
    m_pTutorialCheckNextAction->setVisible(false);
    if (m_pTutorialFocusOverlay) {
        m_pTutorialFocusOverlay->setResult(title, detail);
    }

    const QString tutorialId = m_activeTutorialId;
    QTimer::singleShot(2200, this, [this, tutorialId] {
        if (m_activeTutorialId != tutorialId || !m_tutorialStepCompleted) {
            return;
        }
        if (m_pTutorialFocusOverlay) {
            m_pTutorialFocusOverlay->hide();
        }
        m_pTutorialGuideLabel->setText(
                tr("FREE PLAY  ·  Lesson complete — practice with these controls"));
        m_pTutorialGuideLabel->show();
    });
}

void MixxxMainWindow::resetTutorialSession() {
    if (m_pTutorialStepTimer) {
        m_pTutorialStepTimer->stop();
    }
    if (m_pTutorialStateTimer) {
        m_pTutorialStateTimer->stop();
    }
    clearTutorialStateGuards();
    m_pTutorialActionControl.reset();
    m_tutorialGuideStep = 0;
    m_tutorialStepCompleted = false;

    const auto pPlayerManager = m_pCoreServices->getPlayerManager();
    if (!pPlayerManager) {
        return;
    }

    const auto triggerControl = [](const QString& group, const QString& item) {
        const ConfigKey key(group, item);
        ControlObject::set(key, 1.0);
        ControlObject::set(key, 0.0);
    };
    const auto ejectLoadedTrack = [&](const QString& group) {
        BaseTrackPlayer* pPlayer = pPlayerManager->getPlayer(group);
        if (pPlayer && pPlayer->getLoadedTrack()) {
            pPlayer->slotEjectTrack(1.0);
        }
    };
    const auto resetPlayer = [&](const QString& group) {
        ControlObject::set(ConfigKey(group, QStringLiteral("play")), 0.0);
        ControlObject::set(ConfigKey(group, QStringLiteral("pfl")), 0.0);
        ControlObject::set(
                ConfigKey(group, QStringLiteral("sync_enabled")), 0.0);
        triggerControl(group, QStringLiteral("rate_set_default"));
        triggerControl(group, QStringLiteral("volume_set_default"));
        triggerControl(group, QStringLiteral("pregain_set_default"));
        ejectLoadedTrack(group);
    };

    for (int deck = 0; deck < pPlayerManager->numberOfDecks(); ++deck) {
        const QString group = PlayerManager::groupForDeck(deck);
        resetPlayer(group);
        triggerControl(QStringLiteral("[EqualizerRack1_%1]").arg(group),
                QStringLiteral("super1_set_default"));
        triggerControl(QStringLiteral("[QuickEffectRack1_%1]").arg(group),
                QStringLiteral("super1_set_default"));
    }
    for (int sampler = 0;
            sampler < pPlayerManager->numberOfSamplers();
            ++sampler) {
        resetPlayer(PlayerManager::groupForSampler(sampler));
    }
    for (int preview = 0;
            preview < pPlayerManager->numberOfPreviewDecks();
            ++preview) {
        const QString group = PlayerManager::groupForPreviewDeck(preview);
        ControlObject::set(ConfigKey(group, QStringLiteral("play")), 0.0);
        ejectLoadedTrack(group);
    }
    triggerControl(QStringLiteral("[Master]"),
            QStringLiteral("crossfader_set_default"));
    for (int unit = 1; unit <= 4; ++unit) {
        ControlObject::set(
                ConfigKey(QStringLiteral("[EffectRack1_EffectUnit%1]").arg(unit),
                        QStringLiteral("enabled")),
                1.0);
    }
}

void MixxxMainWindow::updateTutorialStateGuards() {
    const QHash<QString, double> requirements = tutorialStateRequirements(
            m_activeTutorialId, m_tutorialGuideStep, m_tutorialStepCompleted);

    QList<QPointer<QWidget>> requiredLocks;
    for (auto it = requirements.constBegin(); it != requirements.constEnd(); ++it) {
        const ConfigKey playKey(it.key(), QStringLiteral("play"));
        if (qAbs(ControlObject::get(playKey) - it.value()) > 0.001) {
            ControlObject::set(playKey, it.value());
        }

        int deckNumber = 0;
        if (!PlayerManager::isDeckGroup(it.key(), &deckNumber)) {
            continue;
        }
        QWidget* pPlayButton = findTutorialGuideTarget(
                QStringLiteral("PlayDeck"),
                QStringLiteral("Deck%1_Src").arg(deckNumber));
        if (pPlayButton) {
            requiredLocks.append(pPlayButton);
        }
    }

    for (const QPointer<QWidget>& pWidget :
            std::as_const(m_tutorialStateLockedWidgets)) {
        if (pWidget && !requiredLocks.contains(pWidget)) {
            pWidget->setEnabled(true);
        }
    }
    for (const QPointer<QWidget>& pWidget : std::as_const(requiredLocks)) {
        if (pWidget) {
            pWidget->setEnabled(false);
        }
    }
    m_tutorialStateLockedWidgets = requiredLocks;
}

void MixxxMainWindow::clearTutorialStateGuards() {
    for (const QPointer<QWidget>& pWidget :
            std::as_const(m_tutorialStateLockedWidgets)) {
        if (pWidget) {
            pWidget->setEnabled(true);
        }
    }
    m_tutorialStateLockedWidgets.clear();
}

void MixxxMainWindow::pauseTutorialDecks() {
    const auto pPlayerManager = m_pCoreServices->getPlayerManager();
    if (!pPlayerManager) {
        return;
    }
    for (int deck = 0; deck < pPlayerManager->numberOfDecks(); ++deck) {
        ControlObject::set(ConfigKey(PlayerManager::groupForDeck(deck),
                                   QStringLiteral("play")),
                0.0);
    }
}

void MixxxMainWindow::initializeWindow() {
    // be sure createMenuBar() is called first
    DEBUG_ASSERT(m_pMenuBar);

    QPalette Pal(palette());
    // safe default QMenuBar background
    QColor MenuBarBackground(m_pMenuBar->palette().color(QPalette::Window));
    Pal.setColor(QPalette::Window, QColor(0x202020));
    setAutoFillBackground(true);
    setPalette(Pal);
    // restore default QMenuBar background
    Pal.setColor(QPalette::Window, MenuBarBackground);
    m_pMenuBar->setPalette(Pal);

    // Restore the current window state (position, maximized, etc).
    // This will also restore fullscreen and thereby create a seamless
    // start if we did shut down while in fullscreen mode and with
    // [Config],StartInFullscreen = 1
    // (slotViewFullScreen(true) in  initialize() is a no-op then)
    restoreGeometry(QByteArray::fromBase64(
            m_pCoreServices->getSettings()
                    ->getValueString(ConfigKey("[MainWindow]", "geometry"))
                    .toUtf8()));
    restoreState(QByteArray::fromBase64(
            m_pCoreServices->getSettings()
                    ->getValueString(ConfigKey("[MainWindow]", "state"))
                    .toUtf8()));

    setWindowIcon(QIcon(MIXXX_ICON_PATH));
    slotUpdateWindowTitle(TrackPointer());
}

#ifndef __APPLE__
void MixxxMainWindow::alwaysHideMenuBarDlg() {
    // Don't show the dialog if the user unchecked "Ask me again"
    if (!m_pCoreServices->getSettings()->getValue<bool>(
                kMenuBarHintConfigKey, true)) {
        return;
    }
    QString title = tr("Allow Mixxx to hide the menu bar?");
    //: Always show the menu bar?
    QString hideBtnLabel = tr("Hide");
    QString showBtnLabel = tr("Always show");
    //: Keep formatting tags <b> (bold text) and <br> (linebreak).
    //: %1 is the placeholder for the 'Always show' button label
    QString desc = tr(
            "The Mixxx menu bar is hidden and can be toggled with a single press "
            "of the <b>Alt</b> key.<br><br>"
            "Click <b>%1</b> to agree.<br><br>"
            "Click <b>%2</b> to disable that, for example if you don't use Mixxx "
            "with a keyboard.<br><br>"
            "You can change this setting any time in Preferences -> Interface."
            "<br>") // line break for some extra margin to the checkbox
                           .arg(hideBtnLabel, showBtnLabel);

    QMessageBox msg;
    msg.setIcon(QMessageBox::Question);
    msg.setWindowTitle(title);
    msg.setText(desc);
    QCheckBox askAgainCheckBox;
    askAgainCheckBox.setText(tr("Ask me again"));
    askAgainCheckBox.setCheckState(Qt::Checked);
    msg.setCheckBox(&askAgainCheckBox);
    QPushButton* pHideBtn = msg.addButton(hideBtnLabel, QMessageBox::AcceptRole);
    QPushButton* pShowBtn = msg.addButton(showBtnLabel, QMessageBox::RejectRole);
    msg.setDefaultButton(pShowBtn);
    msg.exec();

    m_pCoreServices->getSettings()->setValue(
            kMenuBarHintConfigKey,
            askAgainCheckBox.checkState() == Qt::Checked ? 1 : 0);

    m_pCoreServices->getSettings()->setValue(
            kHideMenuBarConfigKey,
            msg.clickedButton() == pHideBtn ? 1 : 0);
}
#endif

QDialog::DialogCode MixxxMainWindow::soundDeviceErrorDlg(
        const QString &title, const QString &text, bool* retryClicked) {
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setWindowTitle(title);
    msgBox.setText(text);

    QPushButton* retryButton =
            msgBox.addButton(tr("Retry"), QMessageBox::ActionRole);
    QPushButton* reconfigureButton =
            msgBox.addButton(tr("Reconfigure"), QMessageBox::ActionRole);
    QPushButton* wikiButton =
            msgBox.addButton(tr("Help"), QMessageBox::ActionRole);
    QPushButton* exitButton =
            msgBox.addButton(tr("Exit"), QMessageBox::ActionRole);

    while (true)
    {
        msgBox.exec();

        if (msgBox.clickedButton() == retryButton) {
            m_pCoreServices->getSoundManager()->clearAndQueryDevices();
            *retryClicked = true;
            return QDialog::Accepted;
        } else if (msgBox.clickedButton() == wikiButton) {
            mixxx::DesktopHelper::openUrl(QUrl(MIXXX_WIKI_TROUBLESHOOTING_SOUND_URL));
            wikiButton->setEnabled(false);
        } else if (msgBox.clickedButton() == reconfigureButton) {
            msgBox.hide();

            m_pCoreServices->getSoundManager()->clearAndQueryDevices();
            // This way of opening the dialog allows us to use it synchronously
            m_pPrefDlg->setWindowModality(Qt::ApplicationModal);
            // Open preferences, sound hardware page is selected (default on first call)
            m_pPrefDlg->exec();
            if (m_pPrefDlg->result() == QDialog::Accepted) {
                return QDialog::Accepted;
            }

            msgBox.show();
        } else if (msgBox.clickedButton() == exitButton) {
            // Will finally quit Mixxx
            return QDialog::Rejected;
        }
    }
}

QDialog::DialogCode MixxxMainWindow::soundDeviceBusyDlg(bool* retryClicked) {
    QString title(tr("Sound Device Busy"));
    QString text(
            "<html> <p>" %
                    tr("Mixxx was unable to open all the configured sound devices.") +
            "</p> <p>" %
                    m_pCoreServices->getSoundManager()->getErrorDeviceName() %
                    " is used by another application or not plugged in."
                    "</p><ul>"
                    "<li>" %
                    tr("<b>Retry</b> after closing the other application "
                       "or reconnecting a sound device") %
                    "</li>"
                    "<li>" %
                    tr("<b>Reconfigure</b> Mixxx's sound device settings.") %
                    "</li>"
                    "<li>" %
                    tr("Get <b>Help</b> from the Mixxx Wiki.") %
                    "</li>"
                    "<li>" %
                    tr("<b>Exit</b> Mixxx.") %
                    "</li>"
                    "</ul></html>");
    return soundDeviceErrorDlg(title, text, retryClicked);
}

QDialog::DialogCode MixxxMainWindow::soundDeviceErrorMsgDlg(
        SoundDeviceStatus status, bool* retryClicked) {
    QString title(tr("Sound Device Error"));
    QString text("<html> <p>" %
                    tr("Mixxx was unable to open all the configured sound "
                       "devices.") +
            "</p> <p>" %
                    m_pCoreServices->getSoundManager()
                            ->getLastErrorMessage(status)
                            .replace("\n", "<br/>") %
                    "</p><ul>"
                    "<li>" %
                    tr("<b>Retry</b> after fixing an issue") %
                    "</li>"
                    "<li>" %
                    tr("<b>Reconfigure</b> Mixxx's sound device settings.") %
                    "</li>"
                    "<li>" %
                    tr("Get <b>Help</b> from the Mixxx Wiki.") %
                    "</li>"
                    "<li>" %
                    tr("<b>Exit</b> Mixxx.") %
                    "</li>"
                    "</ul></html>");
    return soundDeviceErrorDlg(title, text, retryClicked);
}

QDialog::DialogCode MixxxMainWindow::noOutputDlg(bool* continueClicked) {
    QMessageBox msgBox;
    msgBox.setIcon(QMessageBox::Warning);
    msgBox.setWindowTitle(tr("No Output Devices"));
    msgBox.setText(
            "<html>" + tr("Mixxx was configured without any output sound devices. "
            "Audio processing will be disabled without a configured output device.") +
            "<ul>"
                "<li>" +
                    tr("<b>Continue</b> without any outputs.") +
                "</li>"
                "<li>" +
                    tr("<b>Reconfigure</b> Mixxx's sound device settings.") +
                "</li>"
                "<li>" +
                    tr("<b>Exit</b> Mixxx.") +
                "</li>"
            "</ul></html>"
    );

    QPushButton* continueButton =
            msgBox.addButton(tr("Continue"), QMessageBox::ActionRole);
    QPushButton* reconfigureButton =
            msgBox.addButton(tr("Reconfigure"), QMessageBox::ActionRole);
    QPushButton* exitButton =
            msgBox.addButton(tr("Exit"), QMessageBox::ActionRole);

    while (true)
    {
        msgBox.exec();

        if (msgBox.clickedButton() == continueButton) {
            *continueClicked = true;
            return QDialog::Accepted;
        } else if (msgBox.clickedButton() == reconfigureButton) {
            msgBox.hide();

            // This way of opening the dialog allows us to use it synchronously
            m_pPrefDlg->setWindowModality(Qt::ApplicationModal);
            m_pPrefDlg->showSoundHardwarePage(mixxx::preferences::SoundHardwareTab::Output);
            m_pPrefDlg->exec();
            if (m_pPrefDlg->result() == QDialog::Accepted) {
                return QDialog::Accepted;
            }

            msgBox.show();

        } else if (msgBox.clickedButton() == exitButton) {
            // Will finally quit Mixxx
            return QDialog::Rejected;
        }
    }
}

void MixxxMainWindow::slotUpdateWindowTitle(TrackPointer pTrack) {
    QString appTitle = VersionStore::applicationName();
    QString filePath;

    // If we have a track, use getInfo() to format a summary string and prepend
    // it to the title.
    // TODO(rryan): Does this violate Mac App Store policies?
    if (pTrack) {
        QString trackInfo = pTrack->getInfo();
        if (!trackInfo.isEmpty()) {
            appTitle = QString("%1 | %2").arg(trackInfo, appTitle);
        }
        filePath = pTrack->getLocation();
    }
    setWindowTitle(appTitle);

    // Display a draggable proxy icon for the track in the title bar on
    // platforms that support it, e.g. macOS
    setWindowFilePath(filePath);
}

void MixxxMainWindow::createMenuBar() {
    ScopedTimer t(QStringLiteral("MixxxMainWindow::createMenuBar"));
    DEBUG_ASSERT(m_pCoreServices->getKeyboardEventFilter());
    m_pMenuBar = make_parented<WMainMenuBar>(
            this, m_pCoreServices->getSettings(), m_pCoreServices->getKeyboardEventFilter());
    if (m_pCentralWidget) {
        m_pMenuBar->setStyleSheet(m_pCentralWidget->styleSheet());
    }
    setMenuBar(m_pMenuBar);
}

void MixxxMainWindow::connectMenuBar() {
    // This function might be invoked multiple times on startup
    // so all connections must be unique!

    ScopedTimer t(QStringLiteral("MixxxMainWindow::connectMenuBar"));
    connect(this,
            &MixxxMainWindow::skinLoaded,
            m_pMenuBar,
            &WMainMenuBar::onNewSkinLoaded,
            Qt::UniqueConnection);

    // Misc
    connect(m_pMenuBar,
            &WMainMenuBar::quit,
            this,
            &MixxxMainWindow::close,
            Qt::UniqueConnection);
    connect(m_pMenuBar,
            &WMainMenuBar::showPreferences,
            this,
            &MixxxMainWindow::slotOptionsPreferences,
            Qt::UniqueConnection);
    connect(m_pMenuBar,
            &WMainMenuBar::loadTrackToDeck,
            this,
            &MixxxMainWindow::slotFileLoadSongPlayer,
            Qt::UniqueConnection);

    connect(m_pMenuBar,
            &WMainMenuBar::showKeywheel,
            this,
            &MixxxMainWindow::slotShowKeywheel,
            Qt::UniqueConnection);
#ifndef __APPLE__
    // Menubar auto-hide
    connect(m_pMenuBar,
            &WMainMenuBar::menubarAutoHideChanged,
            this,
            &MixxxMainWindow::slotUpdateMenuBarAltKeyConnection,
            Qt::UniqueConnection);
#endif

    // Fullscreen
    connect(m_pMenuBar,
            &WMainMenuBar::toggleFullScreen,
            this,
            &MixxxMainWindow::slotViewFullScreen,
            Qt::UniqueConnection);
    connect(this,
            &MixxxMainWindow::fullScreenChanged,
            m_pMenuBar,
            &WMainMenuBar::onFullScreenStateChange,
            Qt::UniqueConnection);
    // Refresh the Fullscreen checkbox for the case we went fullscreen earlier
    m_pMenuBar->onFullScreenStateChange(isFullScreen());

    // Help
    connect(m_pMenuBar,
            &WMainMenuBar::showAbout,
            this,
            &MixxxMainWindow::slotHelpAbout,
            Qt::UniqueConnection);

    // Developer
    connect(m_pMenuBar,
            &WMainMenuBar::reloadSkin,
            this,
            &MixxxMainWindow::rebootMixxxView,
            Qt::UniqueConnection);
    connect(m_pMenuBar,
            &WMainMenuBar::toggleDeveloperTools,
            this,
            &MixxxMainWindow::slotDeveloperTools,
            Qt::UniqueConnection);

    if (m_pCoreServices->getRecordingManager()) {
        connect(m_pCoreServices->getRecordingManager().get(),
                &RecordingManager::isRecording,
                m_pMenuBar,
                &WMainMenuBar::onRecordingStateChange,
                Qt::UniqueConnection);
        connect(m_pMenuBar,
                &WMainMenuBar::toggleRecording,
                m_pCoreServices->getRecordingManager().get(),
                &RecordingManager::slotSetRecording,
                Qt::UniqueConnection);
        m_pMenuBar->onRecordingStateChange(
                m_pCoreServices->getRecordingManager()->isRecordingActive());
    }

#ifdef __BROADCAST__
    if (m_pCoreServices->getBroadcastManager()) {
        connect(m_pCoreServices->getBroadcastManager().get(),
                &BroadcastManager::broadcastEnabled,
                m_pMenuBar,
                &WMainMenuBar::onBroadcastingStateChange,
                Qt::UniqueConnection);
        connect(m_pMenuBar,
                &WMainMenuBar::toggleBroadcasting,
                m_pCoreServices->getBroadcastManager().get(),
                &BroadcastManager::setEnabled,
                Qt::UniqueConnection);
        m_pMenuBar->onBroadcastingStateChange(m_pCoreServices->getBroadcastManager()->isEnabled());
    }
#endif

#ifdef __VINYLCONTROL__
    if (m_pCoreServices->getVinylControlManager()) {
        connect(m_pMenuBar,
                &WMainMenuBar::toggleVinylControl,
                m_pCoreServices->getVinylControlManager().get(),
                &VinylControlManager::toggleVinylControl,
                Qt::UniqueConnection);
        connect(m_pCoreServices->getVinylControlManager().get(),
                &VinylControlManager::vinylControlDeckEnabled,
                m_pMenuBar,
                &WMainMenuBar::onVinylControlDeckEnabledStateChange,
                Qt::UniqueConnection);
    }
#endif

    auto pPlayerManager = m_pCoreServices->getPlayerManager();
    if (pPlayerManager) {
        connect(pPlayerManager.get(),
                &PlayerManager::numberOfDecksChanged,
                m_pMenuBar,
                &WMainMenuBar::onNumberOfDecksChanged,
                Qt::UniqueConnection);
        m_pMenuBar->onNumberOfDecksChanged(pPlayerManager->numberOfDecks());
    }

    if (m_pCoreServices->getTrackCollectionManager()) {
        connect(m_pMenuBar,
                &WMainMenuBar::rescanLibrary,
                m_pCoreServices->getTrackCollectionManager().get(),
                &TrackCollectionManager::startLibraryScan,
                Qt::UniqueConnection);
        connect(m_pCoreServices->getTrackCollectionManager().get(),
                &TrackCollectionManager::libraryScanStarted,
                m_pMenuBar,
                &WMainMenuBar::onLibraryScanStarted,
                Qt::UniqueConnection);
        connect(m_pCoreServices->getTrackCollectionManager().get(),
                &TrackCollectionManager::libraryScanFinished,
                m_pMenuBar,
                &WMainMenuBar::onLibraryScanFinished,
                Qt::UniqueConnection);
    }

    if (m_pCoreServices->getLibrary()) {
        connect(m_pMenuBar,
                &WMainMenuBar::searchInCurrentView,
                m_pCoreServices->getLibrary().get(),
                &Library::slotSearchInCurrentView,
                Qt::UniqueConnection);
        connect(m_pMenuBar,
                &WMainMenuBar::searchInAllTracks,
                m_pCoreServices->getLibrary().get(),
                &Library::slotSearchInAllTracks,
                Qt::UniqueConnection);
        connect(m_pMenuBar,
                &WMainMenuBar::createCrate,
                m_pCoreServices->getLibrary().get(),
                &Library::slotCreateCrate,
                Qt::UniqueConnection);
        connect(m_pMenuBar,
                &WMainMenuBar::createPlaylist,
                m_pCoreServices->getLibrary().get(),
                &Library::slotCreatePlaylist,
                Qt::UniqueConnection);
        connect(m_pMenuBar,
                &WMainMenuBar::showAutoDJ,
                m_pCoreServices->getLibrary().get(),
                &Library::showAutoDJ,
                Qt::UniqueConnection);
    }

#ifdef __ENGINEPRIME__
    DEBUG_ASSERT(m_pLibraryExporter);
    connect(m_pMenuBar,
            &WMainMenuBar::exportLibrary,
            m_pLibraryExporter.get(),
            &mixxx::LibraryExporter::slotRequestExport,
            Qt::UniqueConnection);
#endif
}

/// Enable/disable listening to Alt key press for toggling the menubar.
#ifndef __APPLE__
void MixxxMainWindow::slotUpdateMenuBarAltKeyConnection() {
    if (!m_pCoreServices->getKeyboardEventFilter() || !m_pMenuBar) {
        return;
    }

    if (m_pCoreServices->getSettings()->getValue<bool>(kHideMenuBarConfigKey, false)) {
        // with Qt::UniqueConnection we don't need to check whether we're already connected
        connect(m_pCoreServices->getKeyboardEventFilter().get(),
                &KeyboardEventFilter::altPressedWithoutKeys,
                m_pMenuBar,
                &WMainMenuBar::slotToggleMenuBar,
                Qt::UniqueConnection);
        m_pMenuBar->hideMenuBar();
    } else {
        disconnect(m_pCoreServices->getKeyboardEventFilter().get(),
                &KeyboardEventFilter::altPressedWithoutKeys,
                m_pMenuBar,
                &WMainMenuBar::slotToggleMenuBar);
        m_pMenuBar->showMenuBar();
    }
}
#endif

void MixxxMainWindow::slotFileLoadSongPlayer(int deck) {
    QString group = PlayerManager::groupForDeck(deck - 1);

    QString loadTrackText = tr("Load track to Deck %1").arg(QString::number(deck));
    QString deckWarningMessage = tr("Deck %1 is currently playing a track.")
            .arg(QString::number(deck));
    QString areYouSure = tr("Are you sure you want to load a new track?");

    if (ControlObject::get(ConfigKey(group, "play")) > 0.0) {
        int ret = QMessageBox::warning(this,
                VersionStore::applicationName(),
                deckWarningMessage + "\n" + areYouSure,
                QMessageBox::Yes | QMessageBox::No,
                QMessageBox::No);

        if (ret != QMessageBox::Yes) {
            return;
        }
    }

    UserSettingsPointer pConfig = m_pCoreServices->getSettings();
    QString trackPath =
            QFileDialog::getOpenFileName(
                    this,
                    loadTrackText,
                    pConfig->getValueString(mixxx::library::prefs::kLegacyDirectoryConfigKey),
                    QString("Audio (%1)")
                            .arg(SoundSourceProxy::getSupportedFileNamePatterns().join(" ")));

    if (!trackPath.isNull()) {
        // The user has picked a file via a file dialog. This means the system
        // sandboxer (if we are sandboxed) has granted us permission to this
        // folder. Create a security bookmark while we have permission so that
        // we can access the folder on future runs. We need to canonicalize the
        // path so we first wrap the directory string with a QDir.
        mixxx::FileInfo fileInfo(trackPath);
        Sandbox::createSecurityToken(&fileInfo);

        m_pCoreServices->getPlayerManager()->slotLoadToDeck(trackPath, deck);
    }
}

void MixxxMainWindow::slotDeveloperTools(bool visible) {
    if (visible) {
        if (m_pDeveloperToolsDlg == nullptr) {
            UserSettingsPointer pConfig = m_pCoreServices->getSettings();
            m_pDeveloperToolsDlg = new DlgDeveloperTools(this, pConfig);
            connect(m_pDeveloperToolsDlg,
                    &DlgDeveloperTools::destroyed,
                    this,
                    &MixxxMainWindow::slotDeveloperToolsClosed);
            connect(this,
                    &MixxxMainWindow::closeDeveloperToolsDlgChecked,
                    m_pDeveloperToolsDlg,
                    &DlgDeveloperTools::done);
            connect(m_pDeveloperToolsDlg,
                    &DlgDeveloperTools::destroyed,
                    m_pMenuBar,
                    &WMainMenuBar::onDeveloperToolsHidden);
        }
        m_pMenuBar->onDeveloperToolsShown();
        m_pDeveloperToolsDlg->show();
        m_pDeveloperToolsDlg->activateWindow();
    } else {
        emit closeDeveloperToolsDlgChecked(0);
    }
}

void MixxxMainWindow::slotDeveloperToolsClosed() {
    m_pDeveloperToolsDlg = nullptr;
}

void MixxxMainWindow::slotViewFullScreen(bool toggle) {
    if (isFullScreen() == toggle) {
        return;
    }

    // Just switch the window state here. eventFilter() will catch the
    // QWindowStateChangeEvent and inform the menu bar that fullscreen changed.
    if (toggle) {
        showFullScreen();
    } else {
        showNormal();
    }
}

void MixxxMainWindow::slotOptionsPreferences() {
    m_pPrefDlg->show();
    m_pPrefDlg->raise();
    m_pPrefDlg->activateWindow();
}

void MixxxMainWindow::slotNoVinylControlInputConfigured() {
    if (m_noVinylInputDialog && m_noVinylInputDialog->isVisible()) {
        // Don't show redundant dialogs.
        // They might be triggered be repeated controller or keyboard and
        // can lockup the GUI.
        return;
    }

    if (!m_noVinylInputDialog) {
        m_noVinylInputDialog = make_parented<QMessageBox>(
                QMessageBox::Warning,
                VersionStore::applicationName(),
                tr("There is no input device selected for this vinyl control.\n"
                   "Please select an input device in the sound hardware preferences first."),
                QMessageBox::Ok | QMessageBox::Cancel,
                this);
        m_noVinylInputDialog->setWindowModality(Qt::ApplicationModal);
        m_noVinylInputDialog->setDefaultButton(QMessageBox::Cancel);
    }
    m_noVinylInputDialog->exec();
    if (m_noVinylInputDialog->clickedButton() ==
            m_noVinylInputDialog->button(QMessageBox::Ok)) {
        m_pPrefDlg->show();
        m_pPrefDlg->showSoundHardwarePage(mixxx::preferences::SoundHardwareTab::Input);
    }
}

void MixxxMainWindow::slotNoDeckPassthroughInputConfigured() {
    if (m_noPassthroughInputDialog && m_noPassthroughInputDialog->isVisible()) {
        // Don't show redundant dialogs.
        // They might be triggered be repeated controller or keyboard and
        // can lockup the GUI.
        return;
    }

    if (!m_noPassthroughInputDialog) {
        m_noPassthroughInputDialog = make_parented<QMessageBox>(
                QMessageBox::Warning,
                VersionStore::applicationName(),
                tr("There is no input device selected for this passthrough control.\n"
                   "Please select an input device in the sound hardware preferences first."),
                QMessageBox::Ok | QMessageBox::Cancel,
                this);
        m_noPassthroughInputDialog->setWindowModality(Qt::ApplicationModal);
        m_noPassthroughInputDialog->setDefaultButton(QMessageBox::Cancel);
    }
    m_noPassthroughInputDialog->exec();
    if (m_noPassthroughInputDialog->clickedButton() ==
            m_noPassthroughInputDialog->button(QMessageBox::Ok)) {
        m_pPrefDlg->show();
        m_pPrefDlg->showSoundHardwarePage(mixxx::preferences::SoundHardwareTab::Input);
    }
}

void MixxxMainWindow::slotNoMicrophoneInputConfigured() {
    if (m_noMicInputDialog && m_noMicInputDialog->isVisible()) {
        // Don't show redundant dialogs.
        // They might be triggered be repeated controller or keyboard and
        // can lockup the GUI.
        return;
    }

    if (!m_noMicInputDialog) {
        m_noMicInputDialog = make_parented<QMessageBox>(
                QMessageBox::Warning,
                VersionStore::applicationName(),
                tr("There is no input device selected for this microphone.\n"
                   "Do you want to select an input device?"),
                QMessageBox::Ok | QMessageBox::Cancel,
                this);
        m_noMicInputDialog->setWindowModality(Qt::ApplicationModal);
        m_noMicInputDialog->setDefaultButton(QMessageBox::Cancel);
    }
    m_noMicInputDialog->exec();
    if (m_noMicInputDialog->clickedButton() ==
            m_noMicInputDialog->button(QMessageBox::Ok)) {
        m_pPrefDlg->show();
        m_pPrefDlg->showSoundHardwarePage(mixxx::preferences::SoundHardwareTab::Input);
    }
}

void MixxxMainWindow::slotNoAuxiliaryInputConfigured() {
    if (m_noAuxInputDialog && m_noAuxInputDialog->isVisible()) {
        // Don't show redundant dialogs.
        // They might be triggered be repeated controller or keyboard and
        // can lockup the GUI.
        return;
    }

    if (!m_noAuxInputDialog) {
        m_noAuxInputDialog = make_parented<QMessageBox>(
                QMessageBox::Warning,
                VersionStore::applicationName(),
                tr("There is no input device selected for this auxiliary.\n"
                   "Do you want to select an input device?"),
                QMessageBox::Ok | QMessageBox::Cancel,
                this);
        m_noAuxInputDialog->setWindowModality(Qt::ApplicationModal);
        m_noAuxInputDialog->setDefaultButton(QMessageBox::Cancel);
    }
    m_noAuxInputDialog->exec();
    if (m_noAuxInputDialog->clickedButton() ==
            m_noAuxInputDialog->button(QMessageBox::Ok)) {
        m_pPrefDlg->show();
        m_pPrefDlg->showSoundHardwarePage(mixxx::preferences::SoundHardwareTab::Input);
    }
}

void MixxxMainWindow::slotHelpAbout() {
    DlgAbout* about = new DlgAbout;
    about->show();
}

void MixxxMainWindow::slotLibraryScanSummaryDlg(const LibraryScanResultSummary& result) {
    if (!m_pCoreServices->getSettings()->getValue<bool>(
                mixxx::library::prefs::kShowScanSummaryConfigKey, true)) {
        return;
    }

    // Don't show the report dialog when the scan is run during startup and no
    // noteworthy changes have been detected.
    if (result.autoscan &&
            result.numNewTracks == 0 &&
            result.numNewMissingTracks == 0 &&
            result.numRediscoveredTracks == 0) {
        return;
    }

    QMessageBox* pMsg = new QMessageBox();
    pMsg->setAttribute(Qt::WA_DeleteOnClose);
    pMsg->setTextFormat(Qt::RichText); // required to get bold text with <b> tags
    pMsg->setWindowTitle(tr("Library scan finished"));

    if (result.noDirectoriesConfigured) {
        pMsg->setText(tr("No music directories configured for scanning.") +
                QStringLiteral("<br>") +
                tr("Add directories in the library preferences."));
        pMsg->show();
        return;
    }

    QString summary =
            tr("Scan took %1").arg(result.durationString) + QStringLiteral("<br><br>");
    if (result.numNewTracks == 0 &&
            result.numMovedTracks == 0 &&
            result.numNewMissingTracks == 0 &&
            result.numRediscoveredTracks == 0) {
        summary += tr("No changes detected.") +
                QStringLiteral("<br><b>") +
                tr("%n track(s) in total", nullptr, result.tracksTotal) +
                QStringLiteral("</b>");
    } else {
        if (result.numNewTracks != 0) {
            summary += tr("%n new track(s) found", nullptr, result.numNewTracks) +
                    QStringLiteral("<br>");
        }
        if (result.numMovedTracks != 0) {
            summary += tr("%n moved track(s) detected", nullptr, result.numMovedTracks) +
                    QStringLiteral("<br>");
        }
        if (result.numNewMissingTracks != 0) {
            summary += tr("%n track(s) missing (%1 total)",
                    nullptr,
                    result.numNewMissingTracks)
                               .arg(result.numMissingTracks);
        }
        if (result.numRediscoveredTracks != 0) {
            summary += QStringLiteral("<br>") +
                    tr("%n track(s) rediscovered",
                            nullptr,
                            result.numRediscoveredTracks);
        }
        summary += QStringLiteral("<br><br><b>") +
                tr("%n track(s) in total", nullptr, result.tracksTotal) +
                QStringLiteral("</b>");
    }

    pMsg->setText(summary);
    pMsg->show();
}

void MixxxMainWindow::slotShowKeywheel(bool toggle) {
    if (!m_pKeywheel) {
        m_pKeywheel = make_parented<DlgKeywheel>(this, m_pCoreServices->getSettings());
        // uncheck the menu item on window close
        connect(m_pKeywheel.get(),
                &DlgKeywheel::finished,
                m_pMenuBar,
                &WMainMenuBar::onKeywheelChange);
    }
    if (toggle) {
        m_pKeywheel->show();
        m_pKeywheel->raise();
    } else {
        m_pKeywheel->hide();
    }
}

void MixxxMainWindow::slotTooltipModeChanged(mixxx::preferences::Tooltips tt) {
    m_toolTipsCfg = tt;
    m_pCoreServices->getKeyboardEventFilter()->setShowOnlyKbdShortcuts(
            tt == mixxx::preferences::Tooltips::OnlyKbdShortcuts);
#ifdef MIXXX_USE_QOPENGL
    ToolTipQOpenGL::singleton().setActive(
            m_activeTutorialId.isEmpty() &&
            m_toolTipsCfg == mixxx::preferences::Tooltips::On);
#endif
}

void MixxxMainWindow::rebootMixxxView() {
    qDebug() << "Now in rebootMixxxView...";
    m_inRebootMixxxView = true;

    if (m_pTutorialVisibility) {
        m_pTutorialVisibilityPanel->setController(nullptr);
        m_pTutorialVisibility->restore();
        m_pTutorialVisibility.reset();
    }

    ScopedWaitCursor cursor;
    // safe geometry for later restoration
    const QRect initGeometry = geometry();

    // We need to tell the menu bar that we are about to delete the old skin and
    // create a new one. It holds "visibility" controls (e.g. "Show Samplers")
    // that need to be deleted -- otherwise we can't tell what features the skin
    // supports since the controls from the previous skin will be left over.
    m_pMenuBar->onNewSkinAboutToLoad();

    if (m_pCentralWidget) {
        // Clear widget pointer list and destroy all update connections before
        // we delete the main widget (ie. all WBaseWidgets) to prevent
        // KeyboardEventFilter accessing dangling pointers, just in case a
        // shortcuts/tooltip update is triggered while we re/load a skin.
        m_pCoreServices->getKeyboardEventFilter()->clearWidgets();
        m_pCentralWidget->hide();
        WaveformWidgetFactory::instance()->destroyWidgets();
        delete m_pCentralWidget;
        m_pCentralWidget = nullptr;
    }

    // Workaround for changing skins while fullscreen, just go out of fullscreen
    // mode. If you change skins while in fullscreen (on Linux, at least) the
    // window returns to 0,0 but and the backdrop disappears so it looks as if
    // it is not fullscreen, but acts as if it is.
    bool wasFullScreen = isFullScreen();
    if (wasFullScreen) {
        showMaximized();
    }

    tryParseAndSetDefaultStyleSheet();

    if (!loadConfiguredSkin()) {
        QMessageBox::critical(this,
                              tr("Error in skin file"),
                              tr("The selected skin cannot be loaded."));
        m_inRebootMixxxView = false;
        // m_pWidgetParent is NULL, we can't continue.
        return;
    }
    m_pMenuBar->setStyleSheet(m_pCentralWidget->styleSheet());

    setCentralWidget(m_pCentralWidget);
    if (m_pTutorialToolBar->isVisible()) {
        m_pTutorialVisibility =
                std::make_unique<mixxx::tutorial::VisibilityController>(m_pCentralWidget);
        const QString profilePath = QDir(m_pCoreServices->getSettings()->getResourcePath())
                                            .filePath(QStringLiteral(
                                                    "tutorials/visibility_profiles.json"));
        QString error;
        if (!m_pTutorialVisibility->applyProfile(
                    profilePath, m_activeTutorialId, &error)) {
            qWarning() << error;
        }
        m_pTutorialVisibilityPanel->setController(m_pTutorialVisibility.get());
    }
#ifdef __LINUX__
    // don't adjustSize() on Linux as this wouldn't use the entire available area
    // to paint the new skin with X11
    // https://github.com/mixxxdj/mixxx/issues/9309
#else
    adjustSize();
#endif

    if (wasFullScreen) {
        showFullScreen();
    } else {
        // Programmatic placement at this point is very problematic.
        // The screen() method returns stale data (primary screen)
        // until the user interacts with mixxx again. Keyboard shortcuts
        // do not count, moving window, opening menu etc does
        // Therefore the placement logic was removed by a simple geometry restore.
        // If the minimum size of the new skin is larger then the restored
        // geometry, the window will be enlarged right & bottom which is
        // safe as the menu is still reachable.
        setGeometry(initGeometry);
    }

    m_inRebootMixxxView = false;
    qDebug() << "rebootMixxxView DONE";
}

bool MixxxMainWindow::loadConfiguredSkin() {
    // TODO: use std::shared_ptr throughout skin widgets instead of these hacky get() calls
    m_pCentralWidget = m_pSkinLoader->loadConfiguredSkin(this,
            &m_skinCreatedControls,
            m_pCoreServices.get());
    if (centralWidget() == m_pLaunchImage) {
        initializationProgressUpdate(100, "");
    }
    emit skinLoaded();
    return m_pCentralWidget != nullptr;
}

/// Try to load default styles that can be overridden by skins
void MixxxMainWindow::tryParseAndSetDefaultStyleSheet() {
    const QString resPath = m_pCoreServices->getSettings()->getResourcePath();
    QFile file(resPath + "/skins/default.qss");
    if (file.open(QIODevice::ReadOnly)) {
        QByteArray fileBytes = file.readAll();
        QString style = QString::fromUtf8(fileBytes);
        setStyleSheet(style);
    } else {
        qWarning() << "Failed to load default skin styles /skins/default.qss!";
    }
}

/// Catch ToolTip and WindowStateChange events
bool MixxxMainWindow::eventFilter(QObject* obj, QEvent* event) {
    if (event->type() == QEvent::ToolTip) {
        // Guided lessons provide their own contextual coaching. Suppress the
        // skin's legacy hover text during the lesson and its focused free play.
        if (!m_activeTutorialId.isEmpty()) {
            return true;
        }
        // Always show tooltips if Ctrl is held down
        if (QApplication::keyboardModifiers().testFlag(Qt::ControlModifier)) {
            return QMainWindow::eventFilter(obj, event);
        }
        // Always show tooltips for cue type buttons in the Cue menu
        if (QLatin1String(obj->metaObject()->className()) == "CueMenuPushButton") {
            return QMainWindow::eventFilter(obj, event);
        }
        // Always show tooltips in Preferences
        QWidget* activeWindow = QApplication::activeWindow();
        if (activeWindow &&
                QLatin1String(activeWindow->metaObject()->className()) ==
                        "DlgPreferences") {
            return QMainWindow::eventFilter(obj, event);
        }

        // For all other we follow the tooltip sett8ing.
        // Return true for no tool tips
        switch (m_toolTipsCfg) {
        case mixxx::preferences::Tooltips::OnlyInLibrary:
            // WLibrary's stacked widgets are not derived from WBaseWidget
            if (dynamic_cast<WBaseWidget*>(obj) != nullptr) {
                return true;
            }
            break;
        case mixxx::preferences::Tooltips::OnlyKbdShortcuts:
            if (dynamic_cast<WBaseWidget*>(obj) == nullptr) {
                return true;
            }
            break;
        case mixxx::preferences::Tooltips::On:
            break;
        case mixxx::preferences::Tooltips::Off:
            return true;
        default:
            DEBUG_ASSERT(!"m_toolTipsCfg value unknown");
            return true;
        }
    } else if (event->type() == QEvent::WindowStateChange) {
#ifndef __APPLE__
        if (windowState() == m_prevState) {
            // Ignore no-op. This happens if another window is raised above
            // MixxxMianWindow,  e.g. DlgPeferences. In such a case event->oldState()
            // will be Qt::WindowNoState which is wrong anyway, so there is nothing
            // to do internally.
            return QMainWindow::eventFilter(obj, event);
        }
        m_prevState = windowState();
#endif
        // Detect if we entered or quit fullscreen mode.
        QWindowStateChangeEvent* changeEvent =
                static_cast<QWindowStateChangeEvent*>(event);
        const bool wasFullScreen = changeEvent->oldState() & Qt::WindowFullScreen;
        const bool isFullScreenNow = windowState() & Qt::WindowFullScreen;
        if ((isFullScreenNow && !wasFullScreen) ||
                (!isFullScreenNow && wasFullScreen)) {
#ifdef __LINUX__
            // Fix for "No menu bar with ubuntu unity in full screen mode"
            // (issues #6072 and #6689). Before touching anything here, please
            // read those bugs.
            // Set this attribute instead of calling setNativeMenuBar(false),
            // see https://github.com/mixxxdj/mixxx/issues/11320
            if (m_supportsGlobalMenuBar) {
                QApplication::setAttribute(Qt::AA_DontUseNativeMenuBar, isFullScreenNow);
                createMenuBar();
                connectMenuBar();
            }
#endif

#ifndef __APPLE__
#ifdef __LINUX__
            // Only show the dialog if we are able to have the menubar in the
            // main window, only then we're able to hide it.
            if (!m_supportsGlobalMenuBar || isFullScreenNow)
#endif
            {
                if (!m_inRebootMixxxView) {
                    alwaysHideMenuBarDlg();
                }
                slotUpdateMenuBarAltKeyConnection();
            }
#endif

            // This will toggle the Fullscreen checkbox and hide the menubar if
            // we go fullscreen.
            // Skip this during startup or the launchimage will be shifted
            // up & down when the menu is shown menu and 'hidden'. The menu
            // will be updated when the skin finished loading.
            if (centralWidget() != m_pLaunchImage) {
                emit fullScreenChanged(isFullScreen());
            }
        }
    }
    // standard event processing
    return QMainWindow::eventFilter(obj, event);
}

void MixxxMainWindow::closeEvent(QCloseEvent *event) {
    // WARNING: We can receive a CloseEvent while only partially
    // initialized. This is because we call QApplication::processEvents to
    // render LaunchImage progress in the constructor.
    if (!confirmExit()) {
        event->ignore();
        return;
    }
    QMainWindow::closeEvent(event);
}

void MixxxMainWindow::checkDirectRendering() {
    // IF
    //  * A waveform viewer exists
    // AND
    //  * The waveform viewer is an OpenGL waveform viewer
    // AND
    //  * The waveform viewer does not have direct rendering enabled.
    // THEN
    //  * Warn user

    WaveformWidgetFactory* factory = WaveformWidgetFactory::instance();
    if (!factory) {
        return;
    }

    UserSettingsPointer pConfig = m_pCoreServices->getSettings();

    if (!factory->isOpenGlAvailable() && !factory->isOpenGlesAvailable() &&
        pConfig->getValueString(ConfigKey("[Direct Rendering]", "Warned")) != QString("yes")) {
        QMessageBox::warning(nullptr,
                tr("OpenGL Direct Rendering"),
                tr("Direct rendering is not enabled on your machine.<br><br>"
                   "This means that the waveform displays will be very<br>"
                   "<b>slow and may tax your CPU heavily</b>. Either update "
                   "your<br>"
                   "configuration to enable direct rendering, or disable<br>"
                   "the waveform displays in the Mixxx preferences by "
                   "selecting<br>"
                   "\"Empty\" as the waveform display in the 'Interface' "
                   "section."));
        pConfig->set(ConfigKey("[Direct Rendering]", "Warned"), QString("yes"));
    }
}

bool MixxxMainWindow::confirmExit() {
    bool playing(false);
    bool playingSampler(false);
    auto pPlayerManager = m_pCoreServices->getPlayerManager();
    int deckCount = pPlayerManager->numberOfDecks();
    int samplerCount = pPlayerManager->numberOfSamplers();
    for (int i = 0; i < deckCount; ++i) {
        if (ControlObject::toBool(
                    ConfigKey(PlayerManager::groupForDeck(i), "play"))) {
            playing = true;
            break;
        }
    }
    for (int i = 0; i < samplerCount; ++i) {
        if (ControlObject::toBool(
                    ConfigKey(PlayerManager::groupForSampler(i), "play"))) {
            playingSampler = true;
            break;
        }
    }
    if (playing) {
        QMessageBox::StandardButton btn = QMessageBox::question(this,
            tr("Confirm Exit"),
            tr("A deck is currently playing. Exit Mixxx?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (btn == QMessageBox::No) {
            return false;
        }
    } else if (playingSampler) {
        QMessageBox::StandardButton btn = QMessageBox::question(this,
            tr("Confirm Exit"),
            tr("A sampler is currently playing. Exit Mixxx?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (btn == QMessageBox::No) {
            return false;
        }
    }
    if (m_pPrefDlg && m_pPrefDlg->isVisible()) {
        QMessageBox::StandardButton btn = QMessageBox::question(
            this, tr("Confirm Exit"),
            tr("The preferences window is still open.") + "<br>" +
            tr("Discard any changes and exit Mixxx?"),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (btn == QMessageBox::No) {
            return false;
        }
        else {
            m_pPrefDlg->close();
        }
    }

    return true;
}

void MixxxMainWindow::initializationProgressUpdate(int progress, const QString& serviceName) {
    if (m_pLaunchImage) {
        m_pLaunchImage->progress(progress, serviceName);
    }
    qApp->processEvents();
}
