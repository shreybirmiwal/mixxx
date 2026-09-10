#include "dialog/dlgtutorialcreator.h"

#include <limits>

#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSlider>
#include <QStandardPaths>
#include <QStyle>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

#include "control/control.h"
#include "control/controlproxy.h"
#include "moc_dlgtutorialcreator.cpp"
#include "util/parented_ptr.h"

namespace {

QString formatTimestamp(qint64 timestampMs) {
    timestampMs = qMax<qint64>(0, timestampMs);
    const qint64 hours = timestampMs / 3600000;
    const qint64 minutes = (timestampMs / 60000) % 60;
    const qint64 seconds = (timestampMs / 1000) % 60;
    const qint64 milliseconds = timestampMs % 1000;
    return QStringLiteral("%1:%2:%3.%4")
            .arg(hours, 2, 10, QLatin1Char('0'))
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'))
            .arg(milliseconds, 3, 10, QLatin1Char('0'));
}

} // namespace

DlgTutorialCreator::DlgTutorialCreator(QWidget* parent)
        : QDialog(parent),
          m_recording(mixxx::tutorial::Recording::create(tr("Untitled tutorial"))) {
    setObjectName(QStringLiteral("tutorialCreator"));
    setWindowTitle(tr("Create a DJ tutorial"));
    setWindowFlag(Qt::Tool, true);
    setAttribute(Qt::WA_DeleteOnClose);
    setMinimumSize(760, 600);
    resize(900, 700);
    setStyleSheet(QStringLiteral(R"(
        QDialog#tutorialCreator { background: #090d12; color: #e6edf3; }
        QLabel#creatorEyebrow { color: #38d996; font-size: 10px; font-weight: 800; }
        QLabel#creatorTitle { color: #f0f6fc; font-size: 22px; font-weight: 850; }
        QLabel#creatorCopy { color: #8492a1; font-size: 11px; }
        QLabel#creatorHint { background: #101b18; border: 1px solid #214b39;
            border-radius: 6px; color: #8eddb9; font-size: 10px; padding: 7px 9px; }
        QLineEdit, QTextEdit, QDoubleSpinBox, QComboBox, QListWidget {
            background: #0f151d; border: 1px solid #293440; border-radius: 6px;
            color: #e6edf3; padding: 7px;
        }
        QListWidget { padding: 4px; }
        QListWidget::item { border-bottom: 1px solid #202a35; padding: 9px; }
        QListWidget::item:selected { background: #173428; color: #80ebbc; }
        QPushButton { background: #18212c; border: 1px solid #344151;
            border-radius: 6px; color: #dce5ee; font-weight: 700; padding: 8px 12px; }
        QPushButton:hover { background: #202c38; border-color: #526276; }
        QPushButton#recordButton { background: #442026; border-color: #8e3947; color: #ff9ba8; }
        QPushButton#recordButton[recording="true"] { background: #a62d40; color: white; }
        QPushButton#saveButton { background: #153d2e; border-color: #2c7a58; color: #80ebbc; }
        QLabel#recordState[recording="true"] { color: #ff7889; font-weight: 800; }
        QLabel[fieldLabel="true"] { color: #718090; font-size: 9px; font-weight: 800; }
        QLabel#timestampClock { color: #f0f6fc; font-family: monospace; font-size: 18px; font-weight: 800; }
        QSlider::groove:horizontal { height: 6px; background: #26313d; border-radius: 3px; }
        QSlider::sub-page:horizontal { background: #38d996; border-radius: 3px; }
        QSlider::handle:horizontal { width: 16px; margin: -6px 0; border-radius: 8px; background: #e6edf3; }
    )"));

    auto pRoot = make_parented<QVBoxLayout>(this);
    pRoot->setContentsMargins(22, 20, 22, 20);
    pRoot->setSpacing(13);

    auto pHeading = make_parented<QLabel>(tr("TUTORIAL RECORDER"), this);
    pHeading->setObjectName(QStringLiteral("creatorEyebrow"));
    auto pTitle = make_parented<QLabel>(tr("Build a tutorial from the UI, a controller, or both."), this);
    pTitle->setObjectName(QStringLiteral("creatorTitle"));
    auto pCopy = make_parented<QLabel>(
            tr("Capture moves from the on-screen Mixxx controls or your DJ controller, then edit every timestamp and instruction."), this);
    pCopy->setObjectName(QStringLiteral("creatorCopy"));
    pCopy->setWordWrap(true);
    pRoot->addWidget(pHeading);
    pRoot->addWidget(pTitle);
    pRoot->addWidget(pCopy);

    auto pLibraryRowWidget = make_parented<QWidget>(this);
    auto pLibraryRow = make_parented<QHBoxLayout>(pLibraryRowWidget);
    pLibraryRow->setContentsMargins(0, 0, 0, 0);
    m_pTutorialChoices = make_parented<QComboBox>(pLibraryRowWidget).get();
    auto pNew = make_parented<QPushButton>(tr("New tutorial"), pLibraryRowWidget);
    auto pDelete = make_parented<QPushButton>(tr("Delete"), pLibraryRowWidget);
    pLibraryRow->addWidget(m_pTutorialChoices, 1);
    pLibraryRow->addWidget(pNew);
    pLibraryRow->addWidget(pDelete);
    pRoot->addWidget(pLibraryRowWidget);

    auto pTitleRowWidget = make_parented<QWidget>(this);
    auto pTitleRow = make_parented<QHBoxLayout>(pTitleRowWidget);
    pTitleRow->setContentsMargins(0, 0, 0, 0);
    m_pTitle = make_parented<QLineEdit>(m_recording.title(), pTitleRowWidget).get();
    m_pTitle->setPlaceholderText(tr("Tutorial name"));
    m_pRecord = make_parented<QPushButton>(tr("●  Start capturing"), pTitleRowWidget).get();
    m_pRecord->setObjectName(QStringLiteral("recordButton"));
    auto pSave = make_parented<QPushButton>(tr("Save tutorial"), pTitleRowWidget);
    pSave->setObjectName(QStringLiteral("saveButton"));
    pTitleRow->addWidget(m_pTitle, 1);
    pTitleRow->addWidget(m_pRecord);
    pTitleRow->addWidget(pSave);
    pRoot->addWidget(pTitleRowWidget);

    auto pTransportWidget = make_parented<QWidget>(this);
    auto pTransport = make_parented<QHBoxLayout>(pTransportWidget);
    pTransport->setContentsMargins(0, 0, 0, 0);
    m_pClock = make_parented<QLabel>(formatTimestamp(0), pTransportWidget).get();
    m_pClock->setObjectName(QStringLiteral("timestampClock"));
    m_pRecordState = make_parented<QLabel>(tr("READY · UI and controller moves become steps"), pTransportWidget).get();
    m_pRecordState->setObjectName(QStringLiteral("recordState"));
    pTransport->addWidget(m_pClock);
    pTransport->addSpacing(10);
    pTransport->addWidget(m_pRecordState);
    pTransport->addStretch();
    pRoot->addWidget(pTransportWidget);

    m_pTimeline = make_parented<QSlider>(Qt::Horizontal, this).get();
    m_pTimeline->setRange(0, 1);
    m_pTimeline->setSingleStep(100);
    m_pTimeline->setPageStep(1000);
    pRoot->addWidget(m_pTimeline);
    auto pCaptureHint = make_parented<QLabel>(
            tr("While capture is running, click or drag any control in the main Mixxx window. To author without performing, scrub here and choose Add manual step at timestamp."),
            this);
    pCaptureHint->setObjectName(QStringLiteral("creatorHint"));
    pCaptureHint->setWordWrap(true);
    pRoot->addWidget(pCaptureHint);

    auto pBodyWidget = make_parented<QWidget>(this);
    auto pBody = make_parented<QHBoxLayout>(pBodyWidget);
    pBody->setContentsMargins(0, 0, 0, 0);
    m_pSteps = make_parented<QListWidget>(pBodyWidget).get();
    m_pSteps->setMinimumWidth(330);
    pBody->addWidget(m_pSteps, 5);

    auto pEditorWidget = make_parented<QWidget>(pBodyWidget);
    auto pEditor = make_parented<QVBoxLayout>(pEditorWidget);
    pEditor->setContentsMargins(8, 0, 0, 0);
    pEditor->setSpacing(7);
    auto pNavigationWidget = make_parented<QWidget>(pEditorWidget);
    auto pNavigation = make_parented<QHBoxLayout>(pNavigationWidget);
    pNavigation->setContentsMargins(0, 0, 0, 0);
    auto pPrevious = make_parented<QPushButton>(tr("← Previous"), pNavigationWidget);
    m_pStepPosition = make_parented<QLabel>(tr("No step selected"), pNavigationWidget).get();
    m_pStepPosition->setAlignment(Qt::AlignCenter);
    auto pNext = make_parented<QPushButton>(tr("Next →"), pNavigationWidget);
    pNavigation->addWidget(pPrevious);
    pNavigation->addWidget(m_pStepPosition, 1);
    pNavigation->addWidget(pNext);
    pEditor->addWidget(pNavigationWidget);

    const auto addFieldLabel = [&pEditor, &pEditorWidget](const QString& text) {
        auto pLabel = make_parented<QLabel>(text, pEditorWidget);
        pLabel->setProperty("fieldLabel", true);
        pEditor->addWidget(pLabel);
    };
    addFieldLabel(tr("TIMESTAMP (SECONDS)"));
    m_pTimestamp = make_parented<QDoubleSpinBox>(pEditorWidget).get();
    m_pTimestamp->setRange(0.0, 86400.0);
    m_pTimestamp->setDecimals(3);
    m_pTimestamp->setSingleStep(0.1);
    pEditor->addWidget(m_pTimestamp);
    addFieldLabel(tr("CAPTURED MIXXX ACTION"));
    m_pAction = make_parented<QLineEdit>(pEditorWidget).get();
    pEditor->addWidget(m_pAction);
    addFieldLabel(tr("WHAT SHOULD THE LEARNER DO?"));
    m_pInstruction = make_parented<QTextEdit>(pEditorWidget).get();
    m_pInstruction->setPlaceholderText(
            tr("Explain the move and what the learner should listen for…"));
    pEditor->addWidget(m_pInstruction, 1);
    auto pStepActionsWidget = make_parented<QWidget>(pEditorWidget);
    auto pStepActions = make_parented<QHBoxLayout>(pStepActionsWidget);
    pStepActions->setContentsMargins(0, 0, 0, 0);
    auto pAddStep = make_parented<QPushButton>(
            tr("+ Add manual step at timestamp"), pStepActionsWidget);
    m_pDeleteStep = make_parented<QPushButton>(tr("Delete step"), pStepActionsWidget).get();
    pStepActions->addWidget(pAddStep);
    pStepActions->addWidget(m_pDeleteStep);
    pEditor->addWidget(pStepActionsWidget);
    pBody->addWidget(pEditorWidget, 6);
    pRoot->addWidget(pBodyWidget, 1);

    m_pRefreshTimer = make_parented<QTimer>(this).get();
    m_pRefreshTimer->setInterval(50);
    connect(m_pRefreshTimer, &QTimer::timeout, this, &DlgTutorialCreator::updateRecordingClock);
    connect(pNew, &QPushButton::clicked, this, &DlgTutorialCreator::newTutorial);
    connect(pDelete, &QPushButton::clicked, this, &DlgTutorialCreator::deleteTutorial);
    connect(m_pTutorialChoices,
            qOverload<int>(&QComboBox::currentIndexChanged),
            this,
            &DlgTutorialCreator::loadSelectedTutorial);
    connect(m_pRecord, &QPushButton::clicked, this, &DlgTutorialCreator::toggleRecording);
    connect(pSave, &QPushButton::clicked, this, &DlgTutorialCreator::saveTutorial);
    connect(m_pTimeline, &QSlider::valueChanged, this, &DlgTutorialCreator::scrubTo);
    connect(m_pSteps, &QListWidget::currentRowChanged, this, &DlgTutorialCreator::selectStep);
    connect(pPrevious, &QPushButton::clicked, this, &DlgTutorialCreator::selectPreviousStep);
    connect(pNext, &QPushButton::clicked, this, &DlgTutorialCreator::selectNextStep);
    connect(pAddStep, &QPushButton::clicked, this, &DlgTutorialCreator::addStep);
    connect(m_pDeleteStep, &QPushButton::clicked, this, &DlgTutorialCreator::deleteStep);
    connect(m_pTimestamp,
            qOverload<double>(&QDoubleSpinBox::valueChanged),
            this,
            &DlgTutorialCreator::updateSelectedStep);
    connect(m_pAction, &QLineEdit::textChanged, this, &DlgTutorialCreator::updateSelectedStep);
    connect(m_pInstruction, &QTextEdit::textChanged, this, &DlgTutorialCreator::updateSelectedStep);

    connectRecordableControls();
    reloadTutorialChoices();
    refreshTimeline();
}

QString DlgTutorialCreator::storageDirectory() const {
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
            .filePath(QStringLiteral("tutorials/created"));
}

QString DlgTutorialCreator::tutorialPath() const {
    return QDir(storageDirectory()).filePath(m_recording.id() + QStringLiteral(".json"));
}

void DlgTutorialCreator::connectRecordableControls() {
    for (const auto& pControl : ControlDoublePrivate::getAllInstances()) {
        const ConfigKey key = pControl->getKey();
        if (!mixxx::tutorial::Recording::shouldCapture(key, 1.0)) {
            continue;
        }
        auto* pProxy = new ControlProxy(key, this);
        pProxy->connectValueChanged(this, [this, key](double value) {
            captureControl(key, value);
        });
    }
}

void DlgTutorialCreator::captureControl(const ConfigKey& key, double value) {
    if (!m_isRecording) {
        return;
    }
    const qint64 timestampMs = m_recordingOffsetMs + m_recordingClock.elapsed();
    const int row = m_recording.capture(timestampMs, key, value);
    if (row >= 0) {
        m_currentTimestampMs = timestampMs;
        m_timelineDirty = true;
    }
}

void DlgTutorialCreator::newTutorial() {
    stopRecording();
    m_recording = mixxx::tutorial::Recording::create(tr("Untitled tutorial"));
    m_currentTimestampMs = 0;
    m_pTitle->setText(m_recording.title());
    m_pTutorialChoices->setCurrentIndex(0);
    refreshTimeline();
    m_pTitle->selectAll();
    m_pTitle->setFocus();
}

void DlgTutorialCreator::toggleRecording() {
    if (m_isRecording) {
        stopRecording();
        saveTutorial();
        return;
    }
    m_recording.setTitle(m_pTitle->text().trimmed().isEmpty() ?
                    tr("Untitled tutorial") : m_pTitle->text().trimmed());
    m_recordingOffsetMs = m_recording.durationMs();
    m_currentTimestampMs = m_recordingOffsetMs;
    m_recordingClock.start();
    m_isRecording = true;
    m_pRecord->setText(tr("■  Stop recording"));
    m_pRecord->setProperty("recording", true);
    m_pRecord->style()->unpolish(m_pRecord);
    m_pRecord->style()->polish(m_pRecord);
    m_pRecordState->setText(tr("CAPTURING · use the Mixxx UI or your controller"));
    m_pRecordState->setProperty("recording", true);
    m_pRecordState->style()->unpolish(m_pRecordState);
    m_pRecordState->style()->polish(m_pRecordState);
    m_pRefreshTimer->start();
}

void DlgTutorialCreator::stopRecording() {
    if (!m_isRecording) {
        return;
    }
    m_currentTimestampMs = m_recordingOffsetMs + m_recordingClock.elapsed();
    m_recording.setDurationMs(qMax(m_recording.durationMs(), m_currentTimestampMs));
    m_isRecording = false;
    m_pRefreshTimer->stop();
    m_pRecord->setText(tr("●  Start capturing"));
    m_pRecord->setProperty("recording", false);
    m_pRecord->style()->unpolish(m_pRecord);
    m_pRecord->style()->polish(m_pRecord);
    m_pRecordState->setText(tr("CAPTURE STOPPED · edit each timestamp below"));
    m_pRecordState->setProperty("recording", false);
    m_pRecordState->style()->unpolish(m_pRecordState);
    m_pRecordState->style()->polish(m_pRecordState);
    refreshTimeline(m_pSteps->currentRow());
}

void DlgTutorialCreator::updateRecordingClock() {
    if (!m_isRecording) {
        return;
    }
    m_currentTimestampMs = m_recordingOffsetMs + m_recordingClock.elapsed();
    m_recording.setDurationMs(qMax(m_recording.durationMs(), m_currentTimestampMs));
    m_pClock->setText(formatTimestamp(m_currentTimestampMs));
    m_loadingUi = true;
    m_pTimeline->setMaximum(qMax(1, static_cast<int>(qMin<qint64>(
                                             m_recording.durationMs(),
                                             std::numeric_limits<int>::max()))));
    m_pTimeline->setValue(static_cast<int>(m_currentTimestampMs));
    m_loadingUi = false;
    if (m_timelineDirty) {
        m_timelineDirty = false;
        refreshTimeline(m_recording.steps().size() - 1);
    }
}

void DlgTutorialCreator::saveTutorial() {
    m_recording.setTitle(m_pTitle->text().trimmed().isEmpty() ?
                    tr("Untitled tutorial") : m_pTitle->text().trimmed());
    QString error;
    if (!m_recording.save(tutorialPath(), &error)) {
        QMessageBox::warning(this, tr("Tutorial was not saved"), error);
        return;
    }
    m_pRecordState->setText(tr("SAVED · %1 steps").arg(m_recording.steps().size()));
    reloadTutorialChoices();
}

void DlgTutorialCreator::reloadTutorialChoices() {
    const QString currentId = m_recording.id();
    m_loadingUi = true;
    m_pTutorialChoices->clear();
    m_pTutorialChoices->addItem(tr("Current · %1").arg(m_recording.title()), QString());
    QDir directory(storageDirectory());
    const QFileInfoList files = directory.entryInfoList(
            {QStringLiteral("*.json")}, QDir::Files, QDir::Time);
    for (const QFileInfo& file : files) {
        QString error;
        const auto saved = mixxx::tutorial::Recording::load(file.absoluteFilePath(), &error);
        if (saved.isValid()) {
            m_pTutorialChoices->addItem(
                    QStringLiteral("%1 · %2 steps")
                            .arg(saved.title())
                            .arg(saved.steps().size()),
                    file.absoluteFilePath());
            if (saved.id() == currentId) {
                m_pTutorialChoices->setCurrentIndex(m_pTutorialChoices->count() - 1);
            }
        }
    }
    m_loadingUi = false;
}

void DlgTutorialCreator::loadSelectedTutorial(int index) {
    if (m_loadingUi || index <= 0) {
        return;
    }
    stopRecording();
    QString error;
    const auto loaded = mixxx::tutorial::Recording::load(
            m_pTutorialChoices->itemData(index).toString(), &error);
    if (!loaded.isValid()) {
        QMessageBox::warning(this, tr("Tutorial could not be opened"), error);
        return;
    }
    m_recording = loaded;
    m_currentTimestampMs = 0;
    m_pTitle->setText(m_recording.title());
    refreshTimeline(m_recording.steps().isEmpty() ? -1 : 0);
}

void DlgTutorialCreator::deleteTutorial() {
    const QString path = m_pTutorialChoices->currentData().toString();
    if (path.isEmpty()) {
        return;
    }
    if (QMessageBox::question(this,
                tr("Delete tutorial?"),
                tr("Delete %1? This cannot be undone.").arg(m_pTutorialChoices->currentText())) !=
            QMessageBox::Yes) {
        return;
    }
    QFile::remove(path);
    newTutorial();
    reloadTutorialChoices();
}

void DlgTutorialCreator::refreshTimeline(int selectRow) {
    m_loadingUi = true;
    m_pSteps->clear();
    for (const auto& step : m_recording.steps()) {
        m_pSteps->addItem(QStringLiteral("%1   %2\n        %3")
                                  .arg(formatTimestamp(step.timestampMs),
                                          step.action,
                                          step.instruction.isEmpty() ?
                                                  tr("Add teaching instructions") :
                                                  step.instruction));
    }
    const int maximum = qMax(1, static_cast<int>(qMin<qint64>(
                                      m_recording.durationMs(),
                                      std::numeric_limits<int>::max())));
    m_pTimeline->setMaximum(maximum);
    m_pTimeline->setValue(static_cast<int>(qMin<qint64>(m_currentTimestampMs, maximum)));
    m_pClock->setText(formatTimestamp(m_currentTimestampMs));
    if (selectRow >= 0 && selectRow < m_pSteps->count()) {
        m_pSteps->setCurrentRow(selectRow);
    }
    m_loadingUi = false;
    refreshStepEditor();
}

void DlgTutorialCreator::refreshStepEditor() {
    const int row = m_pSteps->currentRow();
    const bool valid = row >= 0 && row < m_recording.steps().size();
    m_loadingUi = true;
    m_pStepPosition->setText(valid ?
                    tr("Step %1 of %2").arg(row + 1).arg(m_recording.steps().size()) :
                    tr("No step selected"));
    m_pDeleteStep->setEnabled(valid);
    if (valid) {
        const auto& step = m_recording.steps().at(row);
        m_pTimestamp->setValue(step.timestampMs / 1000.0);
        m_pAction->setText(step.action);
        m_pInstruction->setPlainText(step.instruction);
    } else {
        m_pTimestamp->setValue(m_currentTimestampMs / 1000.0);
        m_pAction->clear();
        m_pInstruction->clear();
    }
    m_pTimestamp->setEnabled(valid);
    m_pAction->setEnabled(valid);
    m_pInstruction->setEnabled(valid);
    m_loadingUi = false;
}

void DlgTutorialCreator::selectStep(int row) {
    if (m_loadingUi || row < 0 || row >= m_recording.steps().size()) {
        refreshStepEditor();
        return;
    }
    setCurrentTimestamp(m_recording.steps().at(row).timestampMs, false);
    refreshStepEditor();
}

void DlgTutorialCreator::selectPreviousStep() {
    m_pSteps->setCurrentRow(qMax(0, m_pSteps->currentRow() - 1));
}

void DlgTutorialCreator::selectNextStep() {
    m_pSteps->setCurrentRow(qMin(m_pSteps->count() - 1, m_pSteps->currentRow() + 1));
}

void DlgTutorialCreator::setCurrentTimestamp(qint64 timestampMs, bool selectNearest) {
    m_currentTimestampMs = qBound<qint64>(0, timestampMs, m_recording.durationMs());
    m_loadingUi = true;
    m_pTimeline->setValue(static_cast<int>(m_currentTimestampMs));
    m_pClock->setText(formatTimestamp(m_currentTimestampMs));
    m_loadingUi = false;
    if (!selectNearest || m_recording.steps().isEmpty()) {
        return;
    }
    int nearest = 0;
    qint64 distance = qAbs(m_recording.steps().first().timestampMs - m_currentTimestampMs);
    for (int index = 1; index < m_recording.steps().size(); ++index) {
        const qint64 candidate = qAbs(m_recording.steps().at(index).timestampMs - m_currentTimestampMs);
        if (candidate < distance) {
            distance = candidate;
            nearest = index;
        }
    }
    m_pSteps->setCurrentRow(nearest);
}

void DlgTutorialCreator::scrubTo(int timestampMs) {
    if (!m_loadingUi && !m_isRecording) {
        setCurrentTimestamp(timestampMs, true);
    }
}

void DlgTutorialCreator::addStep() {
    stopRecording();
    const int row = m_recording.addNote(m_currentTimestampMs);
    refreshTimeline(row);
    m_pInstruction->setFocus();
}

void DlgTutorialCreator::deleteStep() {
    const int row = m_pSteps->currentRow();
    if (row < 0 || row >= m_recording.steps().size()) {
        return;
    }
    m_recording.steps().removeAt(row);
    refreshTimeline(qMin(row, m_recording.steps().size() - 1));
}

void DlgTutorialCreator::updateSelectedStep() {
    if (m_loadingUi) {
        return;
    }
    const int row = m_pSteps->currentRow();
    if (row < 0 || row >= m_recording.steps().size()) {
        return;
    }
    auto& step = m_recording.steps()[row];
    const qint64 previousTimestampMs = step.timestampMs;
    step.timestampMs = qRound64(m_pTimestamp->value() * 1000.0);
    step.action = m_pAction->text();
    step.instruction = m_pInstruction->toPlainText();
    m_currentTimestampMs = step.timestampMs;
    m_recording.setDurationMs(qMax(m_recording.durationMs(), step.timestampMs));
    if (step.timestampMs == previousTimestampMs) {
        if (QListWidgetItem* pItem = m_pSteps->item(row)) {
            pItem->setText(QStringLiteral("%1   %2\n        %3")
                                   .arg(formatTimestamp(step.timestampMs),
                                           step.action,
                                           step.instruction.isEmpty() ?
                                                   tr("Add teaching instructions") :
                                                   step.instruction));
        }
        return;
    }
    m_recording.sortSteps();
    int selected = 0;
    qint64 distance = std::numeric_limits<qint64>::max();
    for (int index = 0; index < m_recording.steps().size(); ++index) {
        const qint64 candidate = qAbs(m_recording.steps().at(index).timestampMs - m_currentTimestampMs);
        if (candidate < distance) {
            selected = index;
            distance = candidate;
        }
    }
    refreshTimeline(selected);
}

void DlgTutorialCreator::closeEvent(QCloseEvent* pEvent) {
    stopRecording();
    if (!m_recording.steps().isEmpty()) {
        saveTutorial();
    }
    QDialog::closeEvent(pEvent);
}
