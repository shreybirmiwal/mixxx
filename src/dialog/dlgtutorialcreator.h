#pragma once

#include <QDialog>
#include <QElapsedTimer>
#include <QString>

#include "tutorial/tutorialrecording.h"

class QCloseEvent;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSlider;
class QTextEdit;
class QTimer;

class DlgTutorialCreator final : public QDialog {
    Q_OBJECT

  public:
    explicit DlgTutorialCreator(QWidget* parent = nullptr);

  protected:
    void closeEvent(QCloseEvent* pEvent) override;

  private slots:
    void newTutorial();
    void toggleRecording();
    void saveTutorial();
    void loadSelectedTutorial(int index);
    void deleteTutorial();
    void addStep();
    void deleteStep();
    void selectPreviousStep();
    void selectNextStep();
    void selectStep(int row);
    void scrubTo(int timestampMs);
    void updateSelectedStep();
    void updateRecordingClock();

  private:
    void connectRecordableControls();
    void captureControl(const ConfigKey& key, double value);
    void stopRecording();
    void reloadTutorialChoices();
    void refreshTimeline(int selectRow = -1);
    void refreshStepEditor();
    void setCurrentTimestamp(qint64 timestampMs, bool selectNearest);
    QString tutorialPath() const;
    QString storageDirectory() const;

    mixxx::tutorial::Recording m_recording;
    QElapsedTimer m_recordingClock;
    qint64 m_recordingOffsetMs{0};
    qint64 m_currentTimestampMs{0};
    bool m_isRecording{false};
    bool m_loadingUi{false};
    bool m_timelineDirty{false};

    QComboBox* m_pTutorialChoices{nullptr};
    QLineEdit* m_pTitle{nullptr};
    QPushButton* m_pRecord{nullptr};
    QLabel* m_pRecordState{nullptr};
    QLabel* m_pClock{nullptr};
    QSlider* m_pTimeline{nullptr};
    QListWidget* m_pSteps{nullptr};
    QLabel* m_pStepPosition{nullptr};
    QDoubleSpinBox* m_pTimestamp{nullptr};
    QLineEdit* m_pAction{nullptr};
    QTextEdit* m_pInstruction{nullptr};
    QPushButton* m_pDeleteStep{nullptr};
    QTimer* m_pRefreshTimer{nullptr};
};
