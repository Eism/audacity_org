/*!********************************************************************

   Audacity: A Digital Audio Editor

   @file RealtimeEffectPanel.cpp

   @author Vitaly Sverchinsky
   
**********************************************************************/

#include "RealtimeEffectPanel.h"

#include <wx/sizer.h>
#include <wx/button.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/menu.h>
#include <wx/wupdlock.h>
#include <wx/bmpbuttn.h>
#include <wx/hyperlink.h>

#ifdef __WXMSW__
#include <wx/dcbuffer.h>
#else
#include <wx/dcclient.h>
#endif

#include "widgets/HelpSystem.h"
#include "Theme.h"
#include "AllThemeResources.h"
#include "AudioIO.h"
#include "PluginManager.h"
#include "Project.h"
#include "ProjectHistory.h"
#include "ProjectWindow.h"
#include "Track.h"
#include "WaveTrack.h"
#include "ThemedWrappers.h"
#include "effects/EffectUI.h"
#include "effects/EffectManager.h"
#include "effects/RealtimeEffectList.h"
#include "effects/RealtimeEffectState.h"

namespace
{
   auto DialogFactory(
      const std::shared_ptr<RealtimeEffectState> &pEffectState
   ){
      return [=](wxWindow &parent,
         EffectPlugin &host, EffectUIClientInterface &client,
         std::shared_ptr<EffectInstance> &pInstance,
         EffectSettingsAccess &access
      ) -> wxDialog * {
         // Make sure there is an associated project, whose lifetime will
         // govern the lifetime of the dialog
         if (auto project = FindProjectFromWindow(&parent)) {
            if (Destroy_ptr<EffectUIHost> dlg{ safenew EffectUIHost{
                  &parent, *project, host, client, pInstance, access,
                  pEffectState } }
               ; dlg->Initialize()
            )  // release() is safe because parent will own it
               return dlg.release();
         }
         return nullptr;
      };
   }

   //fwd
   class RealtimeEffectControl;
   PluginID ShowSelectEffectMenu(wxWindow* parent, RealtimeEffectControl* currentEffectControl = nullptr);

   class DropHintLine : public wxWindow
   {
   public:
      DropHintLine(wxWindow *parent,
                wxWindowID id,
                const wxPoint& pos = wxDefaultPosition,
                const wxSize& size = wxDefaultSize)
                   : wxWindow(parent, id, pos, size, wxNO_BORDER, wxEmptyString)
      {
         wxWindow::SetBackgroundStyle(wxBG_STYLE_PAINT);
         Bind(wxEVT_PAINT, &DropHintLine::OnPaint, this);
      }

   private:
      void OnPaint(wxPaintEvent&)
      {
#ifdef __WXMSW__
         wxBufferedPaintDC dc(this);
#else
         wxPaintDC dc(this);
#endif
         const auto rect = wxRect(GetSize());

         dc.SetPen(*wxTRANSPARENT_PEN);
         dc.SetBrush(GetBackgroundColour());
         dc.DrawRectangle(rect);
      }
   };

   //Event generated by MovableControl when item is picked,
   //dragged or dropped, extends wxCommandEvent interface
   //with "source" and "target" indices which denote the
   //initial and final element positions inside wxSizer (if present)
   class MovableControlEvent final : public wxCommandEvent
   {
      int mSourceIndex{-1};
      int mTargetIndex{-1};
   public:
      MovableControlEvent(wxEventType eventType, int winid = 0)
         : wxCommandEvent(eventType, winid) { }
      
      void SetSourceIndex(int index) noexcept { mSourceIndex = index; }
      int GetSourceIndex() const noexcept { return mSourceIndex; }

      void SetTargetIndex(int index) noexcept { mTargetIndex = index; }
      int GetTargetIndex() const noexcept { return mTargetIndex; }

      wxEvent* Clone() const override
      {
         return new MovableControlEvent(*this);
      }
   };

   wxDEFINE_EVENT(EVT_MOVABLE_CONTROL_DRAG_STARTED, MovableControlEvent);
   wxDEFINE_EVENT(EVT_MOVABLE_CONTROL_DRAG_POSITION, MovableControlEvent);
   wxDEFINE_EVENT(EVT_MOVABLE_CONTROL_DRAG_FINISHED, MovableControlEvent);

   //Base class for the controls that can be moved with drag-and-drop
   //action. Currently implementation is far from being generic and
   //can work only in pair with wxBoxSizer with wxVERTICAL layout.
   class MovableControl : public wxWindow
   {
      bool mDragging { false };
      wxPoint mInitialPosition;

      int mTargetIndex { -1 };
      int mSourceIndex { -1 };
   public:

      MovableControl(wxWindow* parent,
                   wxWindowID id,
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize,
                   long style = wxTAB_TRAVERSAL | wxNO_BORDER,
                   const wxString& name = wxPanelNameStr)
                      : wxWindow(parent, id, pos, size, style, name)
      {
         Bind(wxEVT_LEFT_DOWN, &MovableControl::OnMouseDown, this);
         Bind(wxEVT_LEFT_UP, &MovableControl::OnMouseUp, this);
         Bind(wxEVT_MOTION, &MovableControl::OnMove, this);
         Bind(wxEVT_MOUSE_CAPTURE_LOST, &MovableControl::OnMouseCaptureLost, this);
      }

      void PostDragEvent(wxWindow* target, wxEventType eventType)
      {
         MovableControlEvent event(eventType);
         event.SetSourceIndex(mSourceIndex);
         event.SetTargetIndex(mTargetIndex);
         event.SetEventObject(this);
         wxPostEvent(target, event);
      }
      
   private:
      
      void OnMouseCaptureLost(wxMouseCaptureLostEvent& event)
      {
         if(mDragging)
            DragFinished();
      }

      void DragFinished()
      {
         if(auto parent = GetParent())
         {
            wxWindowUpdateLocker freeze(this);
            PostDragEvent(parent, EVT_MOVABLE_CONTROL_DRAG_FINISHED);
         }
         mDragging = false;
      }

      void OnMouseDown(wxMouseEvent& evt)
      {
         if(mDragging)
         {
            DragFinished();
            return;
         }

         auto parent = GetParent();
         if(!parent)
            return;

         mSourceIndex = mTargetIndex = -1;
         
         CaptureMouse();

         mInitialPosition = evt.GetPosition();
         mDragging=true;
         
         if(auto sizer = parent->GetSizer())
         {
            for(size_t i = 0, count = sizer->GetItemCount(); i < count; ++i)
            {
               if(sizer->GetItem(i)->GetWindow() == this)
               {
                  mSourceIndex = mTargetIndex = static_cast<int>(i);
                  PostDragEvent(parent, EVT_MOVABLE_CONTROL_DRAG_STARTED);
                  break;
               }
            }
         }
      }

      void OnMouseUp(wxMouseEvent& evt)
      {
         if(!mDragging)
            return;
         
         ReleaseMouse();
         
         DragFinished();
      }
      
      void OnMove(wxMouseEvent& evt)
      {
         if(!mDragging)
            return;

         auto parent = GetParent();
         if(!parent)
            return;

         wxPoint newPosition = wxGetMousePosition() - mInitialPosition;
         Move(GetParent()->ScreenToClient(newPosition));

         if(auto boxSizer = dynamic_cast<wxBoxSizer*>(parent->GetSizer()))
         {
            if(boxSizer->GetOrientation() == wxVERTICAL)
            {
               auto targetIndex = mSourceIndex;
               
               //assuming that items are ordered from top to bottom (i > j <=> y(i) > y(j))
               //compare wxSizerItem position with the current MovableControl position!
               if(GetPosition().y < boxSizer->GetItem(mSourceIndex)->GetPosition().y)
               {
                  //moving up
                  for(int i = 0; i < mSourceIndex; ++i)
                  {
                     const auto item = boxSizer->GetItem(i);
                     
                     if(GetRect().GetTop() <= item->GetPosition().y + item->GetSize().y / 2)
                     {
                        targetIndex = i;
                        break;
                     }
                  }
               }
               else
               {
                  //moving down
                  for(int i = static_cast<int>(boxSizer->GetItemCount()) - 1; i > mSourceIndex; --i)
                  {
                     const auto item = boxSizer->GetItem(i);
                     if(GetRect().GetBottom() >= item->GetPosition().y + item->GetSize().y / 2)
                     {
                        targetIndex = i;
                        break;
                     }
                  }
               }

               if(targetIndex != mTargetIndex)
               {
                  mTargetIndex = targetIndex;
                  PostDragEvent(parent, EVT_MOVABLE_CONTROL_DRAG_POSITION);
               }
            }
         }
      }
   };

   //UI control that represents individual effect from the effect list
   class RealtimeEffectControl : public MovableControl
   {
      wxWeakRef<AudacityProject> mProject;
      std::shared_ptr<Track> mTrack;
      std::shared_ptr<RealtimeEffectState> mEffectState;
      std::shared_ptr<EffectSettingsAccess> mSettingsAccess;

      ThemedButtonWrapper<wxButton>* mChangeButton{nullptr};
      ThemedButtonWrapper<wxBitmapButton>* mEnableButton{nullptr};
      wxWindow *mOptionsButton{};

   public:
      RealtimeEffectControl(wxWindow* parent,
                   wxWindowID winid,
                   const wxPoint& pos = wxDefaultPosition,
                   const wxSize& size = wxDefaultSize,
                   long style = wxTAB_TRAVERSAL | wxNO_BORDER)
                      : MovableControl(parent, winid, pos, size, style)
      {
         //Prevents flickering and paint order issues
         MovableControl::SetBackgroundStyle(wxBG_STYLE_PAINT);

         Bind(wxEVT_PAINT, &RealtimeEffectControl::OnPaint, this);

         auto sizer = std::make_unique<wxBoxSizer>(wxHORIZONTAL);

         //On/off button
         auto enableButton = safenew ThemedButtonWrapper<wxBitmapButton>(this, wxID_ANY, wxBitmap{}, wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
         enableButton->SetBitmapIndex(bmpEffectOn);
         enableButton->SetBackgroundColorIndex(clrEffectListItemBackground);
         mEnableButton = enableButton;

         enableButton->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
            auto pButton =
               static_cast<ThemedButtonWrapper<wxBitmapButton>*>(mEnableButton);
            auto index = pButton->GetBitmapIndex();
            bool wasEnabled = (index == bmpEffectOn);
            if (mSettingsAccess) {
               mSettingsAccess->ModifySettings([&](EffectSettings &settings){
                  settings.extra.SetActive(!wasEnabled);
               });
            }
            pButton->SetBitmapIndex(wasEnabled ? bmpEffectOff : bmpEffectOn);
         });

         //Central button with effect name
         mChangeButton = safenew ThemedButtonWrapper<wxButton>(this, wxID_ANY);
         mChangeButton->Bind(wxEVT_BUTTON, &RealtimeEffectControl::OnChangeButtonClicked, this);

         //Show effect settings
         auto optionsButton = safenew ThemedButtonWrapper<wxBitmapButton>(this, wxID_ANY, wxBitmap{}, wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
         optionsButton->SetBitmapIndex(bmpEffectSettingsNormal);
         optionsButton->SetBitmapPressedIndex(bmpEffectSettingsDown);
         optionsButton->SetBitmapCurrentIndex(bmpEffectSettingsHover);
         optionsButton->SetBackgroundColorIndex(clrEffectListItemBackground);
         optionsButton->Bind(wxEVT_BUTTON, &RealtimeEffectControl::OnOptionsClicked, this);
         
         auto dragArea = safenew wxStaticBitmap(this, wxID_ANY, theTheme.Bitmap(bmpDragArea));
         dragArea->Disable();
         sizer->Add(dragArea, 0, wxLEFT | wxCENTER, 5);
         sizer->Add(mEnableButton, 0, wxLEFT | wxCENTER, 5);
         sizer->Add(mChangeButton, 1, wxLEFT | wxCENTER, 5);
         sizer->Add(optionsButton, 0, wxLEFT | wxRIGHT | wxCENTER, 5);
         mOptionsButton = optionsButton;

         auto vSizer = std::make_unique<wxBoxSizer>(wxVERTICAL);
         vSizer->Add(sizer.release(), 0, wxUP | wxDOWN | wxEXPAND, 10);

         SetSizer(vSizer.release());
      }

      static const PluginDescriptor *GetPlugin(const PluginID &ID) {
         auto desc = PluginManager::Get().GetPlugin(ID);
         return desc;
      }
      
      //! @pre `mEffectState != nullptr`
      TranslatableString GetEffectName() const
      {
         const auto &ID = mEffectState->GetID();
         const auto desc = GetPlugin(ID);
         return desc
            ? desc->GetSymbol().Msgid()
            : XO("%s (missing)")
               .Format(PluginManager::GetEffectNameFromID(ID).GET());
      }

      void SetEffect(AudacityProject& project,
         const std::shared_ptr<Track>& track,
         const std::shared_ptr<RealtimeEffectState> &pState)
      {
         mProject = &project;
         mTrack = track;
         mEffectState = pState;
         TranslatableString label;
         if (pState) {
            label = GetEffectName();
            mSettingsAccess = pState->GetAccess();
         }
         else
            mSettingsAccess.reset();
         if (mEnableButton)
            mEnableButton->SetBitmapIndex(
               (mSettingsAccess && mSettingsAccess->Get().extra.GetActive())
                  ? bmpEffectOn
                  : bmpEffectOff
            );
         mChangeButton->SetTranslatableLabel(label);
         if (mOptionsButton)
            mOptionsButton->Enable(pState && GetPlugin(pState->GetID()));
      }

      void RemoveFromList()
      {
         if(mProject == nullptr || mEffectState == nullptr)
            return;
         
         auto effectName = GetEffectName();
         //After AudioIO::RemoveState call this will be destroyed
         auto project = mProject.get();
         auto trackName = mTrack->GetName();

         AudioIO::Get()->RemoveState(*project, &*mTrack, mEffectState);
         ProjectHistory::Get(*project).PushState(
            //i18n-hint: undo history, first parameter - realtime effect name, second - track name
            XO("'%s' removed from '%s' effect stack").Format(effectName, trackName),
            //i18n-hint: undo history record
            XO("Remove realtime effect")
         );
      }
      
      void OnOptionsClicked(wxCommandEvent& event)
      {
         if(mProject == nullptr || mEffectState == nullptr)
            return;//not initialized
         
         const auto ID = mEffectState->GetID();
         const auto effectPlugin = EffectManager::Get().GetEffect(ID);
         if(effectPlugin == nullptr)
         {
            ///TODO: effect is not avaialble
            return;
         }
         auto access = mEffectState->GetAccess();
         // Copy settings
         auto initialSettings = access->Get();
         auto cleanup = EffectManager::Get().SetBatchProcessing(ID);

         std::shared_ptr<EffectInstance> pInstance;

         // Like the call in EffectManager::PromptUser, but access causes
         // the necessary inter-thread communication of settings changes
         // if play is in progress
         bool changed = effectPlugin->ShowHostInterface(
            ProjectWindow::Get( *mProject), DialogFactory(mEffectState),
            pInstance, *access, true );

         if (changed)
         {
            auto effectName = GetEffectName();
            ProjectHistory::Get(*mProject).PushState(
               //i18n-hint: undo history, first parameter - realtime effect name, second - track name
               XO("'%s' effect of '%s' modified").Format(effectName, mTrack->GetName()),
               //i18n-hint: undo history record
               XO("Modify realtime effect")
         );
         }
         else
            // Dialog was cancelled.
            // Reverse any temporary changes made to the state
            access->Set(std::move(initialSettings));
      }

      void OnChangeButtonClicked(wxCommandEvent& event)
      {
         if(mEffectState == nullptr)
            return;//not initialized

         ShowSelectEffectMenu(mChangeButton, this);
         //TODO: replace effect
      }

      void OnPaint(wxPaintEvent&)
      {
#ifdef __WXMSW__
         wxBufferedPaintDC dc(this);
#else
         wxPaintDC dc(this);
#endif
         const auto rect = wxRect(GetSize());

         dc.SetPen(*wxTRANSPARENT_PEN);
         dc.SetBrush(GetBackgroundColour());
         dc.DrawRectangle(rect);

         dc.SetPen(theTheme.Colour(clrEffectListItemBorder));
         dc.SetBrush(theTheme.Colour(clrEffectListItemBorder));
         dc.DrawLine(rect.GetBottomLeft(), rect.GetBottomRight());
      }
      
   };

   PluginID ShowSelectEffectMenu(wxWindow* parent, RealtimeEffectControl* currentEffectControl)
   {
      wxMenu menu;

      if(currentEffectControl != nullptr)
      {
         //no need to handle language change since menu creates it's own event loop
         menu.Append(wxID_REMOVE, _("No Effect"));
         menu.AppendSeparator();
      }

      auto& pluginManager = PluginManager::Get();

      std::vector<const PluginDescriptor*> effects;
      int selectedEffectIndex = -1;

      auto compareEffects = [](const PluginDescriptor* a, const PluginDescriptor* b)
      {
         return a->GetVendor() < b->GetVendor()
            || (a->GetVendor() == b->GetVendor() && a->GetSymbol().Translation() < b->GetSymbol().Translation());
      };

      for(auto& effect : pluginManager.EffectsOfType(EffectTypeProcess))
      {
         if(!effect.IsEffectRealtime() || !effect.IsEnabled())
            continue;
         effects.push_back(&effect);
      }

      std::sort(effects.begin(), effects.end(), compareEffects);
      
      wxString currentSubMenuName;
      std::unique_ptr<wxMenu> currentSubMenu;

      auto submenuEventHandler = [&](wxCommandEvent& event)
      {
         selectedEffectIndex = event.GetId() - wxID_HIGHEST; 
      };

      for(int i = 0, count = effects.size(); i < count; ++i)
      {
         auto& effect = *effects[i];

         if(currentSubMenuName != effect.GetVendor())
         {
            if(currentSubMenu)
            {
               currentSubMenu->Bind(wxEVT_MENU, submenuEventHandler);
               menu.AppendSubMenu(currentSubMenu.release(), currentSubMenuName);
            }
            currentSubMenuName = effect.GetVendor();
            currentSubMenu = std::make_unique<wxMenu>();
         }

         const auto ID = wxID_HIGHEST + i;
         currentSubMenu->Append(ID, effect.GetSymbol().Translation());
      }
      if(currentSubMenu)
      {
         currentSubMenu->Bind(wxEVT_MENU, submenuEventHandler);
         menu.AppendSubMenu(currentSubMenu.release(), currentSubMenuName);
         menu.AppendSeparator();
      }
      menu.Append(wxID_MORE, _("Get more effects..."));

      menu.Bind(wxEVT_MENU, [&](wxCommandEvent& event)
      {
         if(event.GetId() == wxID_REMOVE)
            currentEffectControl->RemoveFromList();
         else if(event.GetId() == wxID_MORE)
            OpenInDefaultBrowser("https://plugins.audacityteam.org/");
      });

      if(parent->PopupMenu(&menu) && selectedEffectIndex != -1)
         return effects[selectedEffectIndex]->GetID();
      
      return {};
   }
}

class RealtimeEffectListWindow : public wxScrolledWindow
{
   wxWeakRef<AudacityProject> mProject;
   std::shared_ptr<Track> mTrack;
   wxButton* mAddEffect{nullptr};
   wxStaticText* mAddEffectHint{nullptr};
   wxWindow* mAddEffectTutorialLink{nullptr};
   wxWindow* mEffectListContainer{nullptr};

   Observer::Subscription mEffectListItemMovedSubscription;

public:
   RealtimeEffectListWindow(wxWindow *parent,
                     wxWindowID winid = wxID_ANY,
                     const wxPoint& pos = wxDefaultPosition,
                     const wxSize& size = wxDefaultSize,
                     long style = wxScrolledWindowStyle,
                     const wxString& name = wxPanelNameStr)
      : wxScrolledWindow(parent, winid, pos, size, style, name)
   {
      Bind(wxEVT_SIZE, &RealtimeEffectListWindow::OnSizeChanged, this);
#ifdef __WXMSW__
      //Fixes flickering on redraw
      wxScrolledWindow::SetDoubleBuffered(true);
#endif
      auto rootSizer = std::make_unique<wxBoxSizer>(wxVERTICAL);

      auto addEffect = safenew ThemedButtonWrapper<wxButton>(this, wxID_ANY);
      addEffect->SetTranslatableLabel(XO("Add effect"));
      addEffect->Bind(wxEVT_BUTTON, &RealtimeEffectListWindow::OnAddEffectClicked, this);
      mAddEffect = addEffect;

      auto addEffectHint = safenew ThemedWindowWrapper<wxStaticText>(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxST_NO_AUTORESIZE);
      //Workaround: text is set in the OnSizeChange
      addEffectHint->SetForegroundColorIndex(clrTrackPanelText);
      mAddEffectHint = addEffectHint;

      //TODO: set link
      auto addEffectTutorialLink = safenew ThemedWindowWrapper<wxHyperlinkCtrl>(this, wxID_ANY, "x", "https://www.audacityteam.org");
      //i18n-hint: Hyperlink to the effects stack panel tutorial video
      addEffectTutorialLink->SetTranslatableLabel(XO("Watch video"));
      mAddEffectTutorialLink = addEffectTutorialLink;

      auto effectListContainer = safenew ThemedWindowWrapper<wxWindow>(this, wxID_ANY);
      effectListContainer->SetBackgroundColorIndex(clrMedium);
      effectListContainer->SetSizer(safenew wxBoxSizer(wxVERTICAL));
      effectListContainer->SetDoubleBuffered(true);
      mEffectListContainer = effectListContainer;

      //indicates the insertion position of the item
      auto dropHintLine = safenew ThemedWindowWrapper<DropHintLine>(effectListContainer, wxID_ANY);
      dropHintLine->SetBackgroundColorIndex(clrDropHintHighlight);
      dropHintLine->Hide();

      rootSizer->Add(mEffectListContainer, 0, wxEXPAND | wxBOTTOM, 10);
      rootSizer->Add(addEffect, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 20);
      rootSizer->Add(addEffectHint, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 20);
      rootSizer->Add(addEffectTutorialLink, 0, wxLEFT | wxRIGHT | wxEXPAND, 20);

      SetSizer(rootSizer.release());
      SetMinSize({});

      Bind(EVT_MOVABLE_CONTROL_DRAG_STARTED, [dropHintLine](const MovableControlEvent& event)
      {
         if(auto window = dynamic_cast<wxWindow*>(event.GetEventObject()))
            window->Raise();
      });
      Bind(EVT_MOVABLE_CONTROL_DRAG_POSITION, [this, dropHintLine](const MovableControlEvent& event)
      {
         constexpr auto DropHintLineHeight { 3 };//px

         auto sizer = mEffectListContainer->GetSizer();
         assert(sizer != nullptr);

         if(event.GetSourceIndex() == event.GetTargetIndex())
         {
            //do not display hint line if position didn't change
            dropHintLine->Hide();
            return;
         }

         if(!dropHintLine->IsShown())
         {
            dropHintLine->Show();
            dropHintLine->Raise();
            if(auto window = dynamic_cast<wxWindow*>(event.GetEventObject()))
               window->Raise();
         }

         auto item = sizer->GetItem(event.GetTargetIndex());
         dropHintLine->SetSize(item->GetSize().x, DropHintLineHeight);

         if(event.GetTargetIndex() > event.GetSourceIndex())
            dropHintLine->SetPosition(item->GetRect().GetBottomLeft() - wxPoint(0, DropHintLineHeight));
         else
            dropHintLine->SetPosition(item->GetRect().GetTopLeft());
      });
      Bind(EVT_MOVABLE_CONTROL_DRAG_FINISHED, [this, dropHintLine](const MovableControlEvent& event)
      {
         dropHintLine->Hide();
         
         if(mProject == nullptr)
            return;

         auto& effectList = RealtimeEffectList::Get(*mTrack);
         const auto from = event.GetSourceIndex();
         const auto to = event.GetTargetIndex();
         if(from != to)
         {
            effectList.MoveEffect(from, to);
            ProjectHistory::Get(*mProject).PushState(
               //i18n-hint: undo history, first parameter - track name, second - old position, third - new position
               XO("'%s' effects reorder %d->%d").Format(mTrack->GetName(), from + 1, to + 1),
               //i18n-hint: undo history record
               XO("Realtime effect reorder"), UndoPush::CONSOLIDATE);
         }
         else
         {
            wxWindowUpdateLocker freeze(this);
            Layout();
         }
      });
      SetScrollRate(0, 20);
   }

   void OnSizeChanged(wxSizeEvent& event)
   {
      if(auto sizerItem = GetSizer()->GetItem(mAddEffectHint))
      {
         //We need to wrap the text whenever panel width changes and adjust widget height
         //so that text is fully visible, but there is no height-for-width layout algorithm
         //in wxWidgets yet, so for now we just do it manually

         //Restore original text, because 'Wrap' will replace it with wrapped one
         mAddEffectHint->SetLabel(_("Realtime effects are non-destructive and can be changed at any time."));
         mAddEffectHint->Wrap(GetClientSize().x - sizerItem->GetBorder() * 2);
         mAddEffectHint->InvalidateBestSize();
      }
      event.Skip();
   }

   void OnEffectListItemChange(const RealtimeEffectListMessage& msg)
   {
      wxWindowUpdateLocker freeze(this);
      if(msg.type == RealtimeEffectListMessage::Type::Move)
      {
         const auto sizer = mEffectListContainer->GetSizer();

         const auto movedItem = sizer->GetItem(msg.srcIndex);

         const auto proportion = movedItem->GetProportion();
         const auto flag = movedItem->GetFlag();
         const auto border = movedItem->GetBorder();
         const auto window = movedItem->GetWindow();
         
         sizer->Remove(msg.srcIndex);
         sizer->Insert(msg.dstIndex, window, proportion, flag, border);
      }
      else if(msg.type == RealtimeEffectListMessage::Type::Insert)
      {
         auto& effects = RealtimeEffectList::Get(*mTrack);
         InsertEffectRow(msg.srcIndex, effects.GetStateAt(msg.srcIndex));
      }
      else if(msg.type == RealtimeEffectListMessage::Type::Remove)
      {
         auto sizer = mEffectListContainer->GetSizer();
         auto item = sizer->GetItem(msg.srcIndex)->GetWindow();
         item->Destroy();
#ifdef __WXGTK__
         // See comment in ReloadEffectsList
         if(sizer->IsEmpty())
            mEffectListContainer->Hide();
#endif
      }
      SendSizeEventToParent();
   }

   void ResetTrack()
   {
      mEffectListItemMovedSubscription.Reset();
      
      mTrack.reset();
      mProject = nullptr;
      ReloadEffectsList();
   }

   void SetTrack(AudacityProject& project, const std::shared_ptr<Track>& track)
   {
      if(mTrack == track)
         return;

      mEffectListItemMovedSubscription.Reset();
      
      mTrack = track;
      mProject = &project;
      ReloadEffectsList();

      if(track)
      {
         auto& effects = RealtimeEffectList::Get(*mTrack);
         mEffectListItemMovedSubscription = effects.Subscribe(*this, &RealtimeEffectListWindow::OnEffectListItemChange);
      }
   }

   void EnableEffects(bool enable)
   {
      if (mTrack)
         RealtimeEffectList::Get(*mTrack).SetActive(enable);
   }

   void ReloadEffectsList()
   {
      wxWindowUpdateLocker freeze(this);
      //delete items that were added to the sizer
      mEffectListContainer->GetSizer()->Clear(true);

#ifdef __WXGTK__
      //Workaround for GTK: Underlying GTK widget does not update
      //its size when wxWindow size is set to zero
      if(!mTrack || RealtimeEffectList::Get(*mTrack).GetStatesCount() == 0)
         mEffectListContainer->Hide();
#endif

      if(mTrack)
      {
         auto& effects = RealtimeEffectList::Get(*mTrack);
         for(size_t i = 0, count = effects.GetStatesCount(); i < count; ++i)
            InsertEffectRow(i, effects.GetStateAt(i));
      }
      mAddEffect->Enable(!!mTrack);
      SendSizeEventToParent();
   }

   void OnAddEffectClicked(const wxCommandEvent& event)
   {
      if(!mTrack || mProject == nullptr)
         return;
      
      const auto effectID = ShowSelectEffectMenu(dynamic_cast<wxWindow*>(event.GetEventObject()));
      if(effectID.empty())
         return;
      
      if(auto state = AudioIO::Get()->AddState(*mProject, &*mTrack, effectID))
      {
         auto effect = state->GetEffect();
         assert(effect); // postcondition of AddState
         ProjectHistory::Get(*mProject).PushState(
            //i18n-hint: undo history, first parameter - realtime effect name, second - track name
            XO("'%s' added to the '%s' effect stack").Format(effect->GetName(), mTrack->GetName()),
            //i18n-hint: undo history record
            XO("Add realtime effect"));
      }
   }

   void InsertEffectRow(size_t index,
      const std::shared_ptr<RealtimeEffectState> &pState)
   {
      if(mProject == nullptr)
         return;

#ifdef __WXGTK__
      // See comment in ReloadEffectsList
      if(!mEffectListContainer->IsShown())
         mEffectListContainer->Show();
#endif

      auto row = safenew ThemedWindowWrapper<RealtimeEffectControl>(mEffectListContainer, wxID_ANY);
      row->SetBackgroundColorIndex(clrEffectListItemBackground);
      row->SetEffect(*mProject, mTrack, pState);
      mEffectListContainer->GetSizer()->Insert(index, row, 0, wxEXPAND);
   }
};


RealtimeEffectPanel::RealtimeEffectPanel(wxWindow* parent, wxWindowID id, const wxPoint& pos, const wxSize& size,
   long style, const wxString& name)
      : wxWindow(parent, id, pos, size, style, name)
{
   auto vSizer = std::make_unique<wxBoxSizer>(wxVERTICAL);
   
   auto header = safenew ThemedWindowWrapper<wxWindow>(this, wxID_ANY);
   header->SetBackgroundColorIndex(clrMedium);
   {
      auto hSizer = std::make_unique<wxBoxSizer>(wxHORIZONTAL);
      auto toggleEffects = safenew ThemedButtonWrapper<wxBitmapButton>(header, wxID_ANY, wxBitmap{}, wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
      toggleEffects->SetBitmapIndex(bmpEffectOn);
      toggleEffects->SetBackgroundColorIndex(clrMedium);
      mToggleEffects = toggleEffects;

      toggleEffects->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
         auto pButton =
            static_cast<ThemedButtonWrapper<wxBitmapButton>*>(mToggleEffects);
         auto index = pButton->GetBitmapIndex();
         bool wasEnabled = (index == bmpEffectOn);
         if (mEffectList) {
            mEffectList->EnableEffects(!wasEnabled);
         }
         pButton->SetBitmapIndex(wasEnabled ? bmpEffectOff : bmpEffectOn);
      });
   
      hSizer->Add(toggleEffects, 0, wxSTRETCH_NOT | wxALIGN_CENTER | wxLEFT, 5);
      {
         auto vSizer = std::make_unique<wxBoxSizer>(wxVERTICAL);

         auto headerText = safenew ThemedWindowWrapper<wxStaticText>(header, wxID_ANY, wxEmptyString);
         headerText->SetFont(wxFont(wxFontInfo().Bold()));
         headerText->SetTranslatableLabel(XO("Realtime Effects"));
         headerText->SetForegroundColorIndex(clrTrackPanelText);

         auto trackTitle = safenew ThemedWindowWrapper<wxStaticText>(header, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxST_ELLIPSIZE_END);
         trackTitle->SetForegroundColorIndex(clrTrackPanelText);
         mTrackTitle = trackTitle;

         vSizer->Add(headerText);
         vSizer->Add(trackTitle);

         hSizer->Add(vSizer.release(), 1, wxEXPAND | wxALL, 10);
      }
      auto close = safenew ThemedButtonWrapper<wxBitmapButton>(header, wxID_ANY, wxBitmap{}, wxDefaultPosition, wxDefaultSize, wxNO_BORDER);
      close->SetBitmapLabelIndex(bmpCloseNormal);
      close->SetBitmapPressedIndex(bmpCloseDown);
      close->SetBitmapCurrentIndex(bmpCloseHover);
      close->SetBackgroundColorIndex(clrMedium);

      close->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Close(); });
      
      hSizer->Add(close, 0, wxSTRETCH_NOT | wxALIGN_CENTER | wxRIGHT, 5);

      header->SetSizer(hSizer.release());
   }
   vSizer->Add(header, 0, wxEXPAND);

   auto effectList = safenew ThemedWindowWrapper<RealtimeEffectListWindow>(this, wxID_ANY);
   effectList->SetBackgroundColorIndex(clrMedium);
   vSizer->Add(effectList, 1, wxEXPAND);
   mEffectList = effectList;

   SetSizerAndFit(vSizer.release());
}

void RealtimeEffectPanel::SetTrack(AudacityProject& project, const std::shared_ptr<Track>& track)
{
   //Avoid creation-on-demand of a useless, empty list in case the track is of non-wave type.
   if(track && dynamic_cast<WaveTrack*>(&*track) != nullptr)
   {
      mTrackTitle->SetLabel(track->GetName());
      mToggleEffects->Enable(true);
      mEffectList->SetTrack(project, track);
   }
   else
      ResetTrack();
}

void RealtimeEffectPanel::ResetTrack()
{
   mTrackTitle->SetLabel(wxEmptyString);
   mToggleEffects->Enable(false);
   mEffectList->ResetTrack();
}
