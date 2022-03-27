/**********************************************************************

  Audacity: A Digital Audio Editor

  Effect.h

  Dominic Mazzoni
  Vaughan Johnson

**********************************************************************/

#ifndef __AUDACITY_EFFECT__
#define __AUDACITY_EFFECT__


#include <wx/event.h>
// Wanted to include just wx/weakref.h, but it fails to compile on Windows
// when you have wxWeakRef to an incomplete type
#include <wx/dialog.h>

#include "EffectPlugin.h" // to inherit
#include "EffectInterface.h" // to inherit
#include "SampleCount.h"

#define BUILTIN_EFFECT_PREFIX wxT("Built-in Effect: ")

namespace BasicUI { class ProgressDialog; }

class AudacityProject;
class EffectParameterMethods;
class LabelTrack;
class Track;
class WaveTrack;

/* i18n-hint: "Nyquist" is an embedded interpreted programming language in
 Audacity, named in honor of the Swedish-American Harry Nyquist (or Nyqvist).
 In the translations of this and other strings, you may transliterate the
 name into another alphabet.  */
#define NYQUISTEFFECTS_FAMILY ( EffectFamilySymbol{ XO("Nyquist") } )

// TODO:  Apr-06-2015
// TODO:  Much more cleanup of old methods and variables is needed, but
// TODO:  can't be done until after all effects are using the NEW API.

class AUDACITY_DLL_API Effect /* not final */ : public wxEvtHandler,
   public EffectUIClientInterface,
   public EffectPlugin
{
 //
 // public methods
 //
 // Used by the outside program to determine properties of an effect and
 // apply the effect to one or more tracks.
 //
 public:
   static inline Effect *FetchParameters(Effect &e, EffectSettings &)
   { return &e; }

   // The constructor is called once by each subclass at the beginning of the program.
   // Avoid allocating memory or doing time-consuming processing here.
   Effect();
   virtual ~Effect();

   // ComponentInterface implementation

   PluginPath GetPath() const override;
   bool VisitSettings(
      SettingsVisitor &visitor, EffectSettings &settings) override;
   bool VisitSettings(
      ConstSettingsVisitor &visitor, const EffectSettings &settings)
      const override;

   ComponentInterfaceSymbol GetSymbol() const override;

   VendorSymbol GetVendor() const override;
   wxString GetVersion() const override;
   TranslatableString GetDescription() const override;

   // EffectDefinitionInterface implementation

   EffectType GetType() const override;
   EffectFamilySymbol GetFamily() const override;
   bool IsInteractive() const override;
   bool IsDefault() const override;
   bool SupportsRealtime() const override;
   bool SupportsAutomation() const override;

   bool SaveSettings(
      const EffectSettings &settings, CommandParameters & parms) const override;
   bool LoadSettings(
      const CommandParameters & parms, Settings &settings) const override;

   bool LoadUserPreset(
      const RegistryPath & name, Settings &settings) const override;
   bool SaveUserPreset(
      const RegistryPath & name, const Settings &settings) const override;

   RegistryPaths GetFactoryPresets() const override;
   bool LoadFactoryPreset(int id, EffectSettings &settings) const override;
   bool LoadFactoryDefaults(Settings &settings) const override;

   // EffectProcessor implementation

   bool InitializeInstance(EffectSettings &settings) override;
   
   unsigned GetAudioInCount() const override;
   unsigned GetAudioOutCount() const override;

   int GetMidiInCount() override;
   int GetMidiOutCount() override;

   sampleCount GetLatency() override;
   size_t GetTailSize() override;

   void SetSampleRate(double rate) override;
   size_t SetBlockSize(size_t maxBlockSize) override;
   size_t GetBlockSize() const override;  

   // VisitSettings(), SaveSettings(), and LoadSettings()
   // use the functions of EffectParameterMethods.  By default, this function
   // defines an empty list of parameters.
   virtual const EffectParameterMethods &Parameters() const;

   bool ProcessInitialize(EffectSettings &settings,
      sampleCount totalLen, ChannelNames chanMap) override;
   bool ProcessFinalize() override;
   size_t ProcessBlock(EffectSettings &settings,
      const float *const *inBlock, float *const *outBlock, size_t blockLen)
      override;

   bool RealtimeInitialize(EffectSettings &settings) override;
   bool RealtimeAddProcessor(EffectSettings &settings,
         unsigned numChannels, float sampleRate) override;
   bool RealtimeFinalize(EffectSettings &settings) noexcept override;
   bool RealtimeSuspend() override;
   bool RealtimeResume() noexcept override;
   bool RealtimeProcessStart(EffectSettings &settings) override;
   size_t RealtimeProcess(int group,  EffectSettings &settings,
      const float *const *inbuf, float *const *outbuf, size_t numSamples)
      override;
   bool RealtimeProcessEnd(EffectSettings &settings) noexcept override;

   int ShowClientInterface(
      wxWindow &parent, wxDialog &dialog, bool forceModal = false) override;

   // EffectUIClientInterface implementation

   std::unique_ptr<EffectUIValidator> PopulateUI(
      ShuttleGui &S, EffectSettingsAccess &access) override;
   bool IsGraphicalUI() override;
   bool ValidateUI(EffectSettings &) override;
   bool CloseUI() override;

   bool CanExportPresets() override;
   void ExportPresets(const EffectSettings &settings) const override;
   void ImportPresets(EffectSettings &settings) override;

   bool HasOptions() override;
   void ShowOptions() override;

   // EffectPlugin implementation

   const EffectDefinitionInterface& GetDefinition() const override;
   virtual NumericFormatSymbol GetSelectionFormat() /* not override? */; // time format in Selection toolbar

   // EffectPlugin implementation

   int ShowHostInterface( wxWindow &parent,
      const EffectDialogFactory &factory, EffectSettingsAccess &access,
      bool forceModal = false) override;
   // The Effect class fully implements the Preview method for you.
   // Only override it if you need to do preprocessing or cleanup.
   void Preview(EffectSettingsAccess &access, bool dryOnly) override;
   bool SaveSettingsAsString(
      const EffectSettings &settings, wxString & parms) const override;
   bool LoadSettingsFromString(
      const wxString & parms, EffectSettings &settings) const override;
   bool IsBatchProcessing() const override;
   void SetBatchProcessing() override;
   void UnsetBatchProcessing() override;
   bool DoEffect(EffectSettings &settings, //!< Always given; only for processing
      double projectRate, TrackList *list,
      WaveTrackFactory *factory, NotifyingSelectedRegion &selectedRegion,
      unsigned flags,
      // Prompt the user for input only if the next arguments are not all null.
      wxWindow *pParent,
      const EffectDialogFactory &dialogFactory,
      const EffectSettingsAccessPtr &pAccess //!< Sometimes given; only for UI
   ) override;
   bool TransferDataToWindow(const EffectSettings &settings) override;
   bool TransferDataFromWindow(EffectSettings &settings) override;

   // Effect implementation

   unsigned TestUIFlags(unsigned mask);

   //! Re-invoke DoEffect on another Effect object that implements the work
   bool Delegate(Effect &delegate, EffectSettings &settings);

   // Display a message box, using effect's (translated) name as the prefix
   // for the title.
   enum : long { DefaultMessageBoxStyle = wxOK | wxCENTRE };
   int MessageBox(const TranslatableString& message,
                  long style = DefaultMessageBoxStyle,
                  const TranslatableString& titleStr = {}) const;

   static void IncEffectCounter(){ nEffectsDone++;};

   virtual bool Process(EffectSettings &settings) = 0;

 protected:
   bool EnableApply(bool enable = true);
   bool EnablePreview(bool enable = true);

//
// protected virtual methods
//
// Each subclass of Effect overrides one or more of these methods to
// do its processing.
//
protected:

   // Called once each time an effect is called.  Perform any initialization;
   // make sure that the effect can be performed on the selected tracks and
   // return false otherwise
   virtual bool Init();

   // Check whether effect should be skipped
   // Typically this is only useful in automation, for example
   // detecting that zero noise reduction is to be done,
   // or that normalisation is being done without Dc bias shift
   // or amplitude modification
   virtual bool CheckWhetherSkipEffect() { return false; }

   // Most effects just use the previewLength, but time-stretching/compressing
   // effects need to use a different input length, so override this method.
   virtual double CalcPreviewInputLength(
      const EffectSettings &settings, double previewLength);

   //! Add controls to effect panel; always succeeds
   /*!
    @return if not null, then return it from Effect::PopulateUI instead of a
    DefaultEffectUIValidator; default implementation returns null
    */
   virtual std::unique_ptr<EffectUIValidator> PopulateOrExchange(
      ShuttleGui & S, EffectSettingsAccess &access);

   // No more virtuals!

   // The Progress methods all return true if the user has cancelled;
   // you should exit immediately if this happens (cleaning up memory
   // is okay, but don't try to undo).

   // Pass a fraction between 0.0 and 1.0
   bool TotalProgress(double frac, const TranslatableString & = {});

   // Pass a fraction between 0.0 and 1.0, for the current track
   // (when doing one track at a time)
   bool TrackProgress(int whichTrack, double frac, const TranslatableString & = {});

   // Pass a fraction between 0.0 and 1.0, for the current track group
   // (when doing stereo groups at a time)
   bool TrackGroupProgress(int whichGroup, double frac, const TranslatableString & = {});

   int GetNumWaveTracks() { return mNumTracks; }
   int GetNumWaveGroups() { return mNumGroups; }

   // Calculates the start time and length in samples for one or two channels
   void GetBounds(
      const WaveTrack &track, const WaveTrack *pRight,
      sampleCount *start, sampleCount *len);

   // Previewing linear effect can be optimised by pre-mixing. However this
   // should not be used for non-linear effects such as dynamic processors
   // To allow pre-mixing before Preview, set linearEffectFlag to true.
   void SetLinearEffectFlag(bool linearEffectFlag);

   // Most effects only need to preview a short selection. However some
   // (such as fade effects) need to know the full selection length.
   void SetPreviewFullSelectionFlag(bool previewDurationFlag);

   // Use this if the effect needs to know if it is previewing
   bool IsPreviewing() { return mIsPreview; }

   // Most effects only require selected tracks to be copied for Preview.
   // If IncludeNotSelectedPreviewTracks(true), then non-linear effects have
   // preview copies of all wave tracks.
   void IncludeNotSelectedPreviewTracks(bool includeNotSelected);

   // Use this method to copy the input tracks to mOutputTracks, if
   // doing the processing on them, and replacing the originals only on success (and not cancel).
   // If not all sync-locked selected, then only selected wave tracks.
   void CopyInputTracks(bool allSyncLockSelected = false);

   // A global counter of all the successful Effect invocations.
   static int nEffectsDone;

   // For the use of analyzers, which don't need to make output wave tracks,
   // but may need to add label tracks.
   class AUDACITY_DLL_API AddedAnalysisTrack {
      friend Effect;
      AddedAnalysisTrack(Effect *pEffect, const wxString &name);
      AddedAnalysisTrack(const AddedAnalysisTrack&) PROHIBITED;

   public:

      AddedAnalysisTrack() {}

      // So you can have a vector of them
      AddedAnalysisTrack(AddedAnalysisTrack &&that);

      LabelTrack *get() const { return mpTrack; }

      // Call this to indicate successful completion of the analyzer.
      void Commit();

      // Destructor undoes the addition of the analysis track if not committed.
      ~AddedAnalysisTrack();

   private:
      Effect *mpEffect{};
      LabelTrack *mpTrack{};
   };

   // Set name to given value if that is not empty, else use default name
   std::shared_ptr<AddedAnalysisTrack> AddAnalysisTrack(const wxString &name = wxString());

   // For the use of analyzers, which don't need to make output wave tracks,
   // but may need to modify label tracks.
   class AUDACITY_DLL_API ModifiedAnalysisTrack {
      friend Effect;
      ModifiedAnalysisTrack
         (Effect *pEffect, const LabelTrack *pOrigTrack, const wxString &name);
      ModifiedAnalysisTrack(const ModifiedAnalysisTrack&) PROHIBITED;

   public:

      ModifiedAnalysisTrack();

      // So you can have a vector of them
      ModifiedAnalysisTrack(ModifiedAnalysisTrack &&that);

      LabelTrack *get() const { return mpTrack; }

      // Call this to indicate successful completion of the analyzer.
      void Commit();

      // Destructor undoes the modification of the analysis track if not committed.
      ~ModifiedAnalysisTrack();

   private:
      Effect *mpEffect{};
      LabelTrack *mpTrack{};
      std::shared_ptr<Track> mpOrigTrack{};
   };

   // Set name to given value if that is not empty, else use default name
   ModifiedAnalysisTrack ModifyAnalysisTrack
      (const LabelTrack *pOrigTrack, const wxString &name = wxString());

   // If bGoodResult, replace mWaveTracks tracks in mTracks with successfully processed
   // mOutputTracks copies, get rid of old mWaveTracks, and set mWaveTracks to mOutputTracks.
   // Else clear and DELETE mOutputTracks copies.
   void ReplaceProcessedTracks(const bool bGoodResult);

   // Use this to append a NEW output track.
   Track *AddToOutputTracks(const std::shared_ptr<Track> &t);

//
// protected data
//
// The Effect base class will set these variables, some or all of which
// may be needed by any particular subclass of Effect.
//
protected:
   BasicUI::ProgressDialog *mProgress{}; // Temporary pointer, NOT deleted in destructor.
   double         mProjectRate{}; // Sample rate of the project - NEW tracks should
                               // be created with this rate...
   double         mSampleRate{};
   WaveTrackFactory   *mFactory{};
   const TrackList *inputTracks() const { return mTracks; }
   const AudacityProject *FindProject() const;
   std::shared_ptr<TrackList> mOutputTracks; // used only if CopyInputTracks() is called.
   double         mT0{};
   double         mT1{};
#ifdef EXPERIMENTAL_SPECTRAL_EDITING
   double         mF0{};
   double         mF1{};
#endif
   wxArrayString  mPresetNames;

   // UI
   //! This smart pointer tracks the lifetime of the dialog
   wxWeakRef<wxDialog> mHostUIDialog;
   wxWindow       *mUIParent{};
   unsigned       mUIFlags{ 0 };

private:
   //! This weak pointer may be the same as the above, or null
   wxWeakRef<wxDialog> mUIDialog;

   wxString GetSavedStateGroup();
   double GetDefaultDuration();

   void CountWaveTracks();

   TrackList *mTracks{}; // the complete list of all tracks

   bool mIsBatch{ false };
   bool mIsLinearEffect{ false };
   bool mPreviewWithNotSelected{ false };
   bool mPreviewFullSelection{ false };

   bool mIsPreview{ false };

   std::vector<Track*> mIMap;
   std::vector<Track*> mOMap;

   int mNumTracks{}; //v This is really mNumWaveTracks, per CountWaveTracks() and GetNumWaveTracks().
   int mNumGroups{};

   size_t mEffectBlockSize{ 0 };
};

//! Convenience for generating EffectDefinitionInterface overrides
//! and static down-casting functions
template<typename Settings, typename Base = Effect>
class EffectWithSettings : public Base {
public:
   EffectSettings MakeSettings() const override
   {
      return EffectSettings::Make<Settings>();
   }
   bool CopySettingsContents(
      const EffectSettings &src, EffectSettings &dst) const override
   {
      return EffectSettings::Copy<Settings>(src, dst);
   }
   //! Assume settings originated from MakeSettings() and copies thereof
   static inline Settings &GetSettings(EffectSettings &settings)
   {
      auto pSettings = settings.cast<Settings>();
      assert(pSettings);
      return *pSettings;
   }
   //! Assume settings originated from MakeSettings() and copies thereof
   static inline const Settings &GetSettings(const EffectSettings &settings)
   {
      return GetSettings(const_cast<EffectSettings &>(settings));
   }
};

// FIXME:
// FIXME:  Remove this once all effects are using the NEW dialog
// FIXME:

#define ID_EFFECT_PREVIEW ePreviewID

#endif
