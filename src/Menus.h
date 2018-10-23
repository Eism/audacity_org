/**********************************************************************

  Audacity: A Digital Audio Editor

  Menus.h

  Dominic Mazzoni

**********************************************************************/
#ifndef __AUDACITY_MENUS__
#define __AUDACITY_MENUS__

#include "Experimental.h"

#include <memory>
#include <vector>
#include <wx/event.h>
#include "SelectedRegion.h"
#include "commands/CommandFunctors.h"

class AudacityProject;
class CommandContext;
class CommandManager;
class LabelTrack;
class PluginDescriptor;
class Track;
class TrackList;
class ViewInfo;
class WaveClip;
class WaveTrack;

enum CommandFlag : unsigned long long;
enum EffectType : int;

typedef wxString PluginID;
typedef wxArrayString PluginIDList;

class PrefsListener
{
public:
   virtual ~PrefsListener();
   virtual void UpdatePrefs(); // default is no-op
};

struct MenuCommandHandler final
   : public CommandHandlerObject // MUST be the first base class!
   , public PrefsListener
{
   MenuCommandHandler();
   ~MenuCommandHandler();

        // Audio I/O Commands

void OnStop(const CommandContext &context );
void OnPause(const CommandContext &context );
void OnRecord(const CommandContext &context );
void OnRecord2ndChoice(const CommandContext &context );
void OnStopSelect(const CommandContext &context );

        // Different posibilities for playing sound

bool MakeReadyToPlay(AudacityProject &project,
                     bool loop = false, bool cutpreview = false); // Helper function that sets button states etc.
void OnPlayStop(const CommandContext &context );
bool DoPlayStopSelect(AudacityProject &project, bool click, bool shift);
void OnPlayStopSelect(const CommandContext &context );
void OnPlayOneSecond(const CommandContext &context );
void OnPlayToSelection(const CommandContext &context );
void OnPlayBeforeSelectionStart(const CommandContext &context );
void OnPlayAfterSelectionStart(const CommandContext &context );
void OnPlayBeforeSelectionEnd(const CommandContext &context );
void OnPlayAfterSelectionEnd(const CommandContext &context );
void OnPlayBeforeAndAfterSelectionStart(const CommandContext &context );
void OnPlayBeforeAndAfterSelectionEnd(const CommandContext &context );
void OnPlayLooped(const CommandContext &context );
void OnPlayCutPreview(const CommandContext &context );

        // Wave track control

void OnTrackPan(const CommandContext &context );
void OnTrackPanLeft(const CommandContext &context );
void OnTrackPanRight(const CommandContext &context );
void OnTrackGain(const CommandContext &context );
void OnTrackGainInc(const CommandContext &context );
void OnTrackGainDec(const CommandContext &context );
void OnTrackMenu(const CommandContext &context );
void OnTrackMute(const CommandContext &context );
void OnTrackSolo(const CommandContext &context );
void OnTrackClose(const CommandContext &context );
void OnTrackMoveUp(const CommandContext &context );
void OnTrackMoveDown(const CommandContext &context );
void OnTrackMoveTop(const CommandContext &context );
void OnTrackMoveBottom(const CommandContext &context );

enum MoveChoice { OnMoveUpID, OnMoveDownID, OnMoveTopID, OnMoveBottomID };
void MoveTrack(AudacityProject &project, Track* target, MoveChoice choice);

        // Device control
void OnInputDevice(const CommandContext &context );
void OnOutputDevice(const CommandContext &context );
void OnAudioHost(const CommandContext &context );
void OnInputChannels(const CommandContext &context );

        // Mixer control

void OnOutputGain(const CommandContext &context );
void OnInputGain(const CommandContext &context );
void OnOutputGainInc(const CommandContext &context );
void OnOutputGainDec(const CommandContext &context );
void OnInputGainInc(const CommandContext &context );
void OnInputGainDec(const CommandContext &context );

        // Transcription control

void OnPlayAtSpeed(const CommandContext &context );
void OnPlayAtSpeedLooped(const CommandContext &context );
void OnPlayAtSpeedCutPreview(const CommandContext &context );
void OnSetPlaySpeed(const CommandContext &context );
void OnPlaySpeedInc(const CommandContext &context );
void OnPlaySpeedDec(const CommandContext &context );

        // Moving track focus commands

void DoPrevTrack( AudacityProject &project, bool shift );
void DoNextTrack( AudacityProject &project, bool shift );
void OnCursorUp(const CommandContext &context );
void OnCursorDown(const CommandContext &context );
void OnFirstTrack(const CommandContext &context );
void OnLastTrack(const CommandContext &context );

        // Selection-Editing Commands

void OnShiftUp(const CommandContext &context );
void OnShiftDown(const CommandContext &context );
void OnToggle(const CommandContext &context );


void OnMoveToNextLabel(const CommandContext &context );
void OnMoveToPrevLabel(const CommandContext &context );
void DoMoveToLabel(AudacityProject &project, bool next);

void OnLockPlayRegion(const CommandContext &context );
void OnUnlockPlayRegion(const CommandContext &context );

void OnSortTime(const CommandContext &context );
void OnSortName(const CommandContext &context );

void OnFullScreen(const CommandContext &context );

static void DoMacMinimize(AudacityProject *project);
void OnMacMinimize(const CommandContext &context );
void OnMacMinimizeAll(const CommandContext &context );
void OnMacZoom(const CommandContext &context );
void OnMacBringAllToFront(const CommandContext &context );

// File Menu

void OnCheckDependencies(const CommandContext &context );

// Edit Menu


public:

void DoPanTracks(AudacityProject &project, float PanValue);
void OnPanLeft(const CommandContext &context );
void OnPanRight(const CommandContext &context );
void OnPanCenter(const CommandContext &context );

void OnMuteAllTracks(const CommandContext &context );
void OnUnmuteAllTracks(const CommandContext &context );

void OnPlotSpectrum(const CommandContext &context );
void OnContrast(const CommandContext &context );

// Transport Menu

void OnSoundActivated(const CommandContext &context );
void OnToggleSoundActivated(const CommandContext &context );
void OnTogglePinnedHead(const CommandContext &context );
void OnTogglePlayRecording(const CommandContext &context );
void OnToggleSWPlaythrough(const CommandContext &context );
#ifdef EXPERIMENTAL_AUTOMATED_INPUT_LEVEL_ADJUSTMENT
   void OnToggleAutomatedInputLevelAdjustment(const CommandContext &context );
#endif
void OnRescanDevices(const CommandContext &context );

#ifdef EXPERIMENTAL_PUNCH_AND_ROLL
void OnPunchAndRoll(const CommandContext &context);
#endif

// Import Submenu

void OnMixAndRender(const CommandContext &context );
void OnMixAndRenderToNewTrack(const CommandContext &context );
void HandleMixAndRender(AudacityProject &project, bool toNewTrack);

void OnAlignNoSync(const CommandContext &context );
void OnAlign(const CommandContext &context );
//void OnAlignMoveSel(int index);
void HandleAlign(AudacityProject &project, int index, bool moveSel);

#ifdef EXPERIMENTAL_SCOREALIGN
void OnScoreAlign(const CommandContext &context );
#endif // EXPERIMENTAL_SCOREALIGN

// Tracks menu
void OnNewWaveTrack(const CommandContext &context );
void OnNewStereoTrack(const CommandContext &context );
void OnNewLabelTrack(const CommandContext &context );
void OnNewTimeTrack(const CommandContext &context );
void OnTimerRecord(const CommandContext &context );
void OnRemoveTracks(const CommandContext &context );
void OnMoveSelectionWithTracks(const CommandContext &context );
void OnSyncLock(const CommandContext &context );

// Effect Menu

struct OnEffectFlags
{

   // No flags specified
   static const int kNone = 0x00;
   // Flag used to disable prompting for configuration parameteres.
   static const int kConfigured = 0x01;
   // Flag used to disable saving the state after processing.
   static const int kSkipState  = 0x02;
   // Flag used to disable "Repeat Last Effect"
   static const int kDontRepeatLast = 0x04;
};

bool DoEffect(const PluginID & ID, const CommandContext & context, int flags);
void OnEffect(const CommandContext &context );
void OnRepeatLastEffect(const CommandContext &context );
bool DoAudacityCommand(const PluginID & ID, const CommandContext &, int flags);
void OnApplyMacroDirectly(const CommandContext &context );
void OnApplyMacrosPalette(const CommandContext &context );
void OnManageMacros(const CommandContext &context );
void OnStereoToMono(const CommandContext &context );
void OnAudacityCommand(const CommandContext &context );
void DoManagePluginsMenu(AudacityProject &project, EffectType Type);
static void RebuildAllMenuBars();
void OnManageGenerators(const CommandContext &context );
void OnManageEffects(const CommandContext &context );
void OnManageAnalyzers(const CommandContext &context );
void OnManageTools(const CommandContext &context );

// Help Menu

void OnAbout(const CommandContext &context );
void OnQuickHelp(const CommandContext &context );
void OnQuickFix(const CommandContext &context );
void OnManual(const CommandContext &context );
void OnCheckForUpdates(const CommandContext &context );
void MayCheckForUpdates(AudacityProject &project);
void OnShowLog(const CommandContext &context );
void OnHelpWelcome(const CommandContext &context );
void OnBenchmark(const CommandContext &context );
#if defined(EXPERIMENTAL_CRASH_REPORT)
void OnCrashReport(const CommandContext &context );
#endif
void OnSimulateRecordingErrors(const CommandContext &context );
void OnDetectUpstreamDropouts(const CommandContext &context );
void OnScreenshot(const CommandContext &context );
void OnAudioDeviceInfo(const CommandContext &context );
#ifdef EXPERIMENTAL_MIDI_OUT
void OnMidiDeviceInfo(const CommandContext &context );
#endif

// Keyboard navigation

void NextOrPrevFrame(AudacityProject &project, bool next);
void OnPrevFrame(const CommandContext &context );
void OnNextFrame(const CommandContext &context );

void OnPrevWindow(const CommandContext &context );
void OnNextWindow(const CommandContext &context );

void OnResample(const CommandContext &context );

public:
   bool mCircularTrackNavigation{};

   void UpdatePrefs() override;
};

class MenuCreator
{
public:
   MenuCreator();
   ~MenuCreator();
   void CreateMenusAndCommands(AudacityProject &project);
   void RebuildMenuBar(AudacityProject &project);

public:
   CommandFlag mLastFlags;
   
   // Last effect applied to this project
   PluginID mLastEffect{};
};

class MenuManager : public MenuCreator
{
public:
   static void ModifyUndoMenuItems(AudacityProject &project);
   static void ModifyToolbarMenus(AudacityProject &project);
   // Calls ModifyToolbarMenus() on all projects
   static void ModifyAllProjectToolbarMenus();

   // checkActive is a temporary hack that should be removed as soon as we
   // get multiple effect preview working
   void UpdateMenus(AudacityProject &project, bool checkActive = true);

   // If checkActive, do not do complete flags testing on an
   // inactive project as it is needlessly expensive.
   CommandFlag GetUpdateFlags(AudacityProject &project, bool checkActive = false);
   void UpdatePrefs();

   // Command Handling
   bool ReportIfActionNotAllowed
      ( AudacityProject &project,
        const wxString & Name, CommandFlag & flags, CommandFlag flagsRqd, CommandFlag mask );
   bool TryToMakeActionAllowed
      ( AudacityProject &project,
        CommandFlag & flags, CommandFlag flagsRqd, CommandFlag mask );


private:
   CommandFlag GetFocusedFrame(AudacityProject &project);

   // 0 is grey out, 1 is Autoselect, 2 is Give warnings.
   int  mWhatIfNoSelection;
   bool mStopIfWasPaused;
};


MenuCommandHandler &GetMenuCommandHandler(AudacityProject &project);
MenuManager &GetMenuManager(AudacityProject &project);

// Exported helper functions from various menu handling source files
namespace FileActions {
AudacityProject *DoImportMIDI(
   AudacityProject *pProject, const wxString &fileName );
}

namespace EditActions {
bool DoEditMetadata(
   AudacityProject &project,
   const wxString &title, const wxString &shortUndoDescription, bool force );
void DoReloadPreferences( AudacityProject & );
void DoUndo( AudacityProject &project );
}

namespace SelectActions {
void DoListSelection(
   AudacityProject &project, Track *t,
   bool shift, bool ctrl, bool modifyState );
void DoSelectAll( AudacityProject &project );
void DoSelectSomething( AudacityProject &project );
}

namespace ViewActions {
void DoZoomFit( AudacityProject &project );
void DoZoomFitV( AudacityProject &project );
}

#endif



