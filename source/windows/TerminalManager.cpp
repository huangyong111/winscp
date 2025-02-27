//---------------------------------------------------------------------------
#define NO_WIN32_LEAN_AND_MEAN
#include <vcl.h>
#pragma hdrstop

#include "TerminalManager.h"
#include <Authenticate.h>
#include "CustomScpExplorer.h"
#include "NonVisual.h"
#include "WinConfiguration.h"
#include "Tools.h"
#include <Common.h>
#include <CoreMain.h>
#include <GUITools.h>
#include <TextsWin.h>
#include <TextsCore.h>
#include <Progress.h>
#include <Exceptions.h>
#include <VCLCommon.h>
#include <WinApi.h>
#include <PuttyTools.h>
#include <HelpWin.h>
#include <System.IOUtils.hpp>
#include <StrUtils.hpp>
//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------
TTerminalManager * TTerminalManager::FInstance = NULL;
//---------------------------------------------------------------------------
__fastcall TManagedTerminal::TManagedTerminal(TSessionData * SessionData,
  TConfiguration * Configuration) :
  TTerminal(SessionData, Configuration),
  LocalExplorerState(NULL), RemoteExplorerState(NULL),
  ReopenStart(0), DirectoryLoaded(Now()), TerminalThread(NULL), Disconnected(false), DisconnectedTemporarily(false)
{
  StateData = new TSessionData(L"");
  StateData->Assign(SessionData);
  StateData->LocalDirectory = StateData->LocalDirectoryExpanded;
}
//---------------------------------------------------------------------------
__fastcall TManagedTerminal::~TManagedTerminal()
{
  delete StateData;
  delete LocalExplorerState;
  delete RemoteExplorerState;
}
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
TTerminalManager * __fastcall TTerminalManager::Instance(bool ForceCreation)
{
  if (!FInstance && ForceCreation)
  {
    FInstance = new TTerminalManager();
  }
  return FInstance;
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::DestroyInstance()
{
  DebugAssert(FInstance);
  SAFE_DESTROY(FInstance);
}
//---------------------------------------------------------------------------
__fastcall TTerminalManager::TTerminalManager() :
  TTerminalList(Configuration)
{
  FQueueSection = new TCriticalSection();
  FActiveTerminal = NULL;
  FScpExplorer = NULL;
  FDestroying = false;
  FTerminalPendingAction = tpNull;
  FDirectoryReadingStart = 0;
  FAuthenticateForm = NULL;
  FTaskbarList = NULL;
  FAuthenticating = 0;
  FMainThread = GetCurrentThreadId();
  FChangeSection.reset(new TCriticalSection());
  FPendingConfigurationChange = 0;
  FKeepAuthenticateForm = false;

  FApplicationsEvents.reset(new TApplicationEvents(Application));
  FApplicationsEvents->OnException = ApplicationException;
  FApplicationsEvents->OnShowHint = ApplicationShowHint;
  FApplicationsEvents->OnMessage = ApplicationMessage;
  FApplicationsEvents->OnModalBegin = ApplicationModalBegin;
  FApplicationsEvents->OnModalEnd = ApplicationModalEnd;

  DebugAssert(WinConfiguration->OnMasterPasswordPrompt == NULL);
  WinConfiguration->OnMasterPasswordPrompt = MasterPasswordPrompt;

  InitTaskbarButtonCreatedMessage();

  DebugAssert(Configuration && !Configuration->OnChange);
  Configuration->OnChange = ConfigurationChange;
  FOnLastTerminalClosed = NULL;
  FOnTerminalListChanged = NULL;

  FTerminalList = new TStringList();
  FMaxSessions = WinConfiguration->MaxSessions;

  FQueues = new TList();
  FTerminationMessages = new TStringList();
  std::unique_ptr<TSessionData> DummyData(new TSessionData(L""));
  FLocalTerminal = CreateTerminal(DummyData.get());
  SetupTerminal(FLocalTerminal);
}
//---------------------------------------------------------------------------
__fastcall TTerminalManager::~TTerminalManager()
{
  FreeAll();

  DebugAssert(!ScpExplorer);

  DebugAssert(Configuration->OnChange == ConfigurationChange);
  Configuration->OnChange = NULL;

  FApplicationsEvents.reset(NULL);

  DebugAssert(WinConfiguration->OnMasterPasswordPrompt == MasterPasswordPrompt);
  WinConfiguration->OnMasterPasswordPrompt = NULL;

  delete FLocalTerminal;
  delete FQueues;
  delete FTerminationMessages;
  delete FTerminalList;
  CloseAutheticateForm();
  delete FQueueSection;
  ReleaseTaskbarList();
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::SetQueueConfiguration(TTerminalQueue * Queue)
{
  Queue->TransfersLimit = GUIConfiguration->QueueTransfersLimit;
  Queue->KeepDoneItemsFor =
    (GUIConfiguration->QueueKeepDoneItems ? GUIConfiguration->QueueKeepDoneItemsFor : 0);
}
//---------------------------------------------------------------------------
TTerminalQueue * __fastcall TTerminalManager::NewQueue(TTerminal * Terminal)
{
  TTerminalQueue * Queue = new TTerminalQueue(Terminal, Configuration);
  SetQueueConfiguration(Queue);
  Queue->Enabled = WinConfiguration->EnableQueueByDefault;
  Queue->OnQueryUser = TerminalQueryUser;
  Queue->OnPromptUser = TerminalPromptUser;
  Queue->OnShowExtendedException = TerminalShowExtendedException;
  Queue->OnEvent = QueueEvent;
  return Queue;
}
//---------------------------------------------------------------------------
TManagedTerminal * __fastcall TTerminalManager::CreateManagedTerminal(TSessionData * Data)
{
  return new TManagedTerminal(Data, Configuration);
}
//---------------------------------------------------------------------------
TTerminal * __fastcall TTerminalManager::CreateTerminal(TSessionData * Data)
{
  return CreateManagedTerminal(Data);
}
//---------------------------------------------------------------------------
TManagedTerminal * __fastcall TTerminalManager::GetTerminal(int Index)
{
  return DebugNotNull(dynamic_cast<TManagedTerminal *>(TTerminalList::Terminals[Index]));
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::SetupTerminal(TTerminal * Terminal)
{
  Terminal->OnQueryUser = TerminalQueryUser;
  Terminal->OnPromptUser = TerminalPromptUser;
  Terminal->OnDisplayBanner = TerminalDisplayBanner;
  Terminal->OnShowExtendedException = TerminalShowExtendedException;
  Terminal->OnProgress = OperationProgress;
  Terminal->OnFinished = OperationFinished;
  Terminal->OnDeleteLocalFile = DeleteLocalFile;
  Terminal->OnReadDirectoryProgress = TerminalReadDirectoryProgress;
  Terminal->OnInformation = TerminalInformation;
  Terminal->OnCustomCommand = TerminalCustomCommand;
}
//---------------------------------------------------------------------------
TManagedTerminal * __fastcall TTerminalManager::DoNewTerminal(TSessionData * Data)
{
  if (Count >= FMaxSessions)
  {
    UnicodeString Msg = FMTLOAD(TOO_MANY_SESSIONS, (Count));
    if (MessageDialog(Msg, qtConfirmation, qaOK | qaCancel, HELP_TOO_MANY_SESSIONS) == qaCancel)
    {
      Abort();
    }
    FMaxSessions = FMaxSessions * 3 / 2; // increase limit before the next warning by 50%
  }
  TManagedTerminal * Terminal = DebugNotNull(dynamic_cast<TManagedTerminal *>(TTerminalList::NewTerminal(Data)));
  try
  {
    FQueues->Add(NewQueue(Terminal));
    FTerminationMessages->Add(L"");

    SetupTerminal(Terminal);
  }
  catch(...)
  {
    if (Terminal != NULL)
    {
      FreeTerminal(Terminal);
    }
    throw;
  }

  return Terminal;
}
//---------------------------------------------------------------------------
TTerminal * __fastcall TTerminalManager::NewTerminal(TSessionData * Data)
{
  TTerminal * Terminal = DoNewTerminal(Data);
  DoTerminalListChanged();
  return Terminal;
}
//---------------------------------------------------------------------------
TManagedTerminal * __fastcall TTerminalManager::NewManagedTerminal(TSessionData * Data)
{
  return DebugNotNull(dynamic_cast<TManagedTerminal *>(NewTerminal(Data)));
}
//---------------------------------------------------------------------------
TManagedTerminal * __fastcall TTerminalManager::NewTerminals(TList * DataList)
{
  TManagedTerminal * Result = NULL;
  for (int Index = 0; Index < DataList->Count; Index++)
  {
    TSessionData * Data = reinterpret_cast<TSessionData *>(DataList->Items[Index]);
    TManagedTerminal * Terminal = DoNewTerminal(Data);
    // When opening workspace/folder, keep the sessions open, even if they fail to connect.
    // We cannot detect a folder here, so we "guess" it by a session set size.
    // After all, no one will have a folder with a one session only (while a workspace with one session is likely).
    // And when when opening a folder with a one session only, it's not that big problem, if we treat it the same way
    // as when opening the session only.
    // Also closing a workspace session will remove the session from the workspace.
    // While closing a folder session won't remove the session from the folder.
    Terminal->Permanent = Data->IsWorkspace || (DataList->Count > 1);
    if (Index == 0)
    {
      Result = Terminal;
    }
  }
  DoTerminalListChanged();
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::FreeActiveTerminal()
{
  if (FTerminalPendingAction == tpNull)
  {
    DebugAssert(ActiveTerminal);
    FreeTerminal(ActiveTerminal);
  }
  else
  {
    DebugAssert(FTerminalPendingAction == ::tpNone);
    FTerminalPendingAction = tpFree;
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::DoConnectTerminal(TTerminal * Terminal, bool Reopen, bool AdHoc)
{
  TManagedTerminal * ManagedTerminal = dynamic_cast<TManagedTerminal *>(Terminal);
  // it must be managed terminal, unless it is secondary terminal (of managed terminal)
  DebugAssert((ManagedTerminal != NULL) || (dynamic_cast<TSecondaryTerminal *>(Terminal) != NULL));

  // particularly when we are reconnecting RemoteDirectory of managed terminal
  // hold the last used remote directory as opposite to session data, which holds
  // the default remote directory.
  // make sure the last used directory is used, but the default is preserved too
  UnicodeString OrigRemoteDirectory = Terminal->SessionData->RemoteDirectory;
  try
  {
    TTerminalThread * TerminalThread = new TTerminalThread(Terminal);
    TerminalThread->AllowAbandon = (Terminal == FActiveTerminal);
    try
    {
      if (ManagedTerminal != NULL)
      {
        Terminal->SessionData->RemoteDirectory = ManagedTerminal->StateData->RemoteDirectory;

        if ((double)ManagedTerminal->ReopenStart == 0)
        {
          ManagedTerminal->ReopenStart = Now();
        }

        ManagedTerminal->Disconnected = false;
        ManagedTerminal->DisconnectedTemporarily = false;
        DebugAssert(ManagedTerminal->TerminalThread == NULL);
        ManagedTerminal->TerminalThread = TerminalThread;
      }

      TNotifyEvent OnIdle;
      ((TMethod*)&OnIdle)->Code = TerminalThreadIdle;
      TerminalThread->OnIdle = OnIdle;
      if (Reopen)
      {
        TerminalThread->TerminalReopen();
      }
      else
      {
        TerminalThread->TerminalOpen();
      }
    }
    __finally
    {
      TerminalThread->OnIdle = NULL;
      if (!TerminalThread->Release())
      {
        if (!AdHoc && (DebugAlwaysTrue(Terminal == FActiveTerminal)))
        {
          // terminal was abandoned, must create a new one to replace it
          Terminal = ManagedTerminal = CreateManagedTerminal(new TSessionData(L""));
          SetupTerminal(Terminal);
          OwnsObjects = false;
          Items[ActiveTerminalIndex] = Terminal;
          OwnsObjects = true;
          FActiveTerminal = ManagedTerminal;
          // Can be NULL, when opening the first session from command-line
          if (FScpExplorer != NULL)
          {
            FScpExplorer->ReplaceTerminal(ManagedTerminal);
          }
        }
        // Now we do not have any reference to an abandoned terminal, so we can safely allow the thread
        // to complete its task and destroy the terminal afterwards.
        TerminalThread->Terminate();

        // When abandoning cancelled terminal, DoInformation(Phase = 0) does not make it to TerminalInformation handler.
        if (DebugAlwaysTrue(FAuthenticating > 0))
        {
          FKeepAuthenticateForm = false;
          AuthenticatingDone();
        }
      }
      else
      {
        if (ManagedTerminal != NULL)
        {
          ManagedTerminal->TerminalThread = NULL;
        }
      }
    }
  }
  __finally
  {
    Terminal->SessionData->RemoteDirectory = OrigRemoteDirectory;
    if (Terminal->Active && (ManagedTerminal != NULL))
    {
      ManagedTerminal->ReopenStart = 0;
      ManagedTerminal->Permanent = true;
    }
  }

  if (DebugAlwaysTrue(Terminal->Active) && !Reopen && GUIConfiguration->QueueBootstrap)
  {
    FindQueueForTerminal(Terminal)->AddItem(new TBootstrapQueueItem());
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::CloseAutheticateForm()
{
  SAFE_DESTROY(FAuthenticateForm);
}
//---------------------------------------------------------------------------
bool __fastcall TTerminalManager::ConnectTerminal(TTerminal * Terminal)
{
  bool Result = true;
  // were it an active terminal, it would allow abandoning, what this API cannot deal with
  DebugAssert(Terminal != FActiveTerminal);
  try
  {
    DoConnectTerminal(Terminal, false, false);
  }
  catch (Exception & E)
  {
    ShowExtendedExceptionEx(Terminal, &E);
    Result = false;
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::TerminalThreadIdle(void * /*Data*/, TObject * /*Sender*/)
{
  Application->ProcessMessages();
}
//---------------------------------------------------------------------------
bool __fastcall TTerminalManager::ConnectActiveTerminalImpl(bool Reopen)
{
  TTerminalPendingAction Action;
  bool Result;
  do
  {
    Action = tpNull;
    Result = false;
    try
    {
      DebugAssert(ActiveTerminal);

      DoConnectTerminal(ActiveTerminal, Reopen, false);

      if (ScpExplorer)
      {
        DebugAssert(ActiveTerminal->Status == ssOpened);
        TerminalReady();
      }

      WinConfiguration->ClearTemporaryLoginData();

      Result = true;
    }
    catch (Exception & E)
    {
      DebugAssert(FTerminalPendingAction == tpNull);
      FTerminalPendingAction = ::tpNone;
      try
      {
        DebugAssert(ActiveTerminal != NULL);
        ActiveTerminal->ShowExtendedException(&E);
        Action = FTerminalPendingAction;
      }
      __finally
      {
        FTerminalPendingAction = tpNull;
      }
    }
  }
  while (Action == tpReconnect);

  if (Action == tpFree)
  {
    DisconnectActiveTerminalIfPermanentFreeOtherwise();
  }

  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::DisconnectActiveTerminalIfPermanentFreeOtherwise()
{
  if (ActiveTerminal->Permanent)
  {
    DisconnectActiveTerminal();
  }
  else
  {
    FreeActiveTerminal();
  }
}
//---------------------------------------------------------------------------
bool __fastcall TTerminalManager::ConnectActiveTerminal()
{
  ActiveTerminal->CollectUsage();

  // add only stored sessions to the jump list,
  // ad-hoc session cannot be reproduced from just a session name
  if (StoredSessions->FindSame(ActiveTerminal->SessionData) != NULL)
  {
    try
    {
      WinConfiguration->AddSessionToJumpList(ActiveTerminal->SessionData->SessionName);
    }
    catch (Exception & E)
    {
      ShowExtendedException(&E);
    }
  }

  FAuthenticationCancelled = false;
  bool Result = ConnectActiveTerminalImpl(false);

  UnicodeString DateStamp = StandardDatestamp();
  if (Result)
  {
    if (Configuration->Usage->Get(L"OpenedSessionsFailedLastDate") == DateStamp)
    {
      Configuration->Usage->Inc(L"OpenedSessionsFailedRecovered");
    }
  }
  else
  {
    Configuration->Usage->Inc(L"OpenedSessionsFailed");
    Configuration->Usage->Set(L"OpenedSessionsFailedLastDate", DateStamp);
    if (FAuthenticationCancelled)
    {
      Configuration->Usage->Inc(L"OpenedSessionsFailedAfterCancel");
    }
  }

  if (Result && WinConfiguration->AutoOpenInPutty && CanOpenInPutty())
  {
    try
    {
      OpenInPutty();
    }
    catch(Exception & E)
    {
      ShowExtendedExceptionEx(NULL, &E);
    }
  }

  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::DisconnectActiveTerminal()
{
  DebugAssert(ActiveTerminal);
  if (ActiveTerminal->Active)
  {
    ActiveTerminal->Close();
  }

  int Index = IndexOf(ActiveTerminal);

  TTerminalQueue * OldQueue;
  TTerminalQueue * NewQueue;
  OldQueue = reinterpret_cast<TTerminalQueue *>(FQueues->Items[Index]);
  NewQueue = this->NewQueue(ActiveTerminal);
  FQueues->Items[Index] = NewQueue;
  if (ScpExplorer != NULL)
  {
    ScpExplorer->Queue = NewQueue;
  }
  delete OldQueue;

  ActiveTerminal->Disconnected = true;
  if (ScpExplorer != NULL)
  {
    TerminalReady(); // in case it was never connected
    ScpExplorer->TerminalDisconnected();
  }
  // disconnecting duplidate session removes need to distinguish the only connected session with short path
  DoTerminalListChanged();
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::ReconnectActiveTerminal()
{
  DebugAssert(ActiveTerminal);

  if (ScpExplorer)
  {
    if (ScpExplorer->Terminal == ActiveTerminal)
    {
      ScpExplorer->UpdateTerminal(ActiveTerminal);
    }
  }

  try
  {
    if (FTerminalPendingAction == tpNull)
    {
      ConnectActiveTerminalImpl(true);
    }
    else
    {
      FTerminalPendingAction = tpReconnect;
    }
  }
  catch(...)
  {
    DisconnectActiveTerminal();
    throw;
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::FreeAll()
{
  FDestroying = true;
  try
  {
    while (Count)
    {
      FreeTerminal(Terminals[0]);
    }
  }
  __finally
  {
    FDestroying = false;
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::FreeTerminal(TTerminal * Terminal)
{
  try
  {
    // we want the Login dialog to open on auto-workspace name,
    // as set in TCustomScpExplorerForm::FormClose
    if (!FDestroying || !WinConfiguration->AutoSaveWorkspace)
    {
      if (StoredSessions->FindSame(Terminal->SessionData) != NULL)
      {
        WinConfiguration->LastStoredSession = Terminal->SessionData->Name;
      }
    }

    if (ScpExplorer != NULL)
    {
      ScpExplorer->TerminalRemoved(Terminal);
    }

    if (Terminal->Active)
    {
      Terminal->Close();
    }
  }
  __finally
  {
    int Index = IndexOf(Terminal);
    Extract(Terminal);

    TTerminalQueue * Queue;
    Queue = reinterpret_cast<TTerminalQueue *>(FQueues->Items[Index]);
    FQueues->Delete(Index);
    FTerminationMessages->Delete(Index);

    if (ActiveTerminal && (Terminal == ActiveTerminal))
    {
      if ((Count > 0) && !FDestroying)
      {
        TManagedTerminal * NewActiveTerminal = Terminals[Index < Count ? Index : Index - 1];
        if (!NewActiveTerminal->Active && !NewActiveTerminal->Disconnected)
        {
          NewActiveTerminal->Disconnected = true;
          NewActiveTerminal->DisconnectedTemporarily = true;
        }
        ActiveTerminal = NewActiveTerminal;
      }
      else
      {
        ActiveTerminal = NULL;
      }
    }
    else
    {
      SaveTerminal(Terminal);
    }

    // only now all references to/from queue (particularly events to explorer)
    // are cleared
    delete Queue;
    delete Terminal;

    DoTerminalListChanged();
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::SetScpExplorer(TCustomScpExplorerForm * value)
{
  if (ScpExplorer != value)
  {
    // changing explorer is not supported yet
    DebugAssert(!ScpExplorer || !value);
    FScpExplorer = value;
    if (FScpExplorer)
    {
      FScpExplorer->Terminal = ActiveTerminal;
      FScpExplorer->Queue = ActiveQueue;
      FOnLastTerminalClosed = FScpExplorer->LastTerminalClosed;
      FOnTerminalListChanged = FScpExplorer->TerminalListChanged;
      UpdateTaskbarList();
    }
    else
    {
      FOnLastTerminalClosed = NULL;
      FOnTerminalListChanged = NULL;
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::SetActiveTerminal(TManagedTerminal * value)
{
  DoSetActiveTerminal(value, false);
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::SetActiveTerminalWithAutoReconnect(TManagedTerminal * value)
{
  DoSetActiveTerminal(value, true);
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::DoSetActiveTerminal(TManagedTerminal * value, bool AutoReconnect)
{
  if (ActiveTerminal != value)
  {
    if (NonVisualDataModule != NULL)
    {
      NonVisualDataModule->StartBusy();
    }
    try
    {
      // here used to be call to TCustomScpExporer::UpdateSessionData (now UpdateTerminal)
      // but it seems to be duplicate to call from TCustomScpExporer::TerminalChanging

      TManagedTerminal * PActiveTerminal = ActiveTerminal;
      FActiveTerminal = value;
      // moved from else block of next if (ActiveTerminal) statement
      // so ScpExplorer can update its caption
      UpdateAppTitle();
      if (ScpExplorer)
      {
        if (ActiveTerminal && ((ActiveTerminal->Status == ssOpened) || ActiveTerminal->Disconnected))
        {
          TerminalReady();
        }
        else
        {
          ScpExplorer->Terminal = NULL;
          ScpExplorer->Queue = NULL;
        }
      }

      if (PActiveTerminal != NULL)
      {
        if (PActiveTerminal->DisconnectedTemporarily && DebugAlwaysTrue(PActiveTerminal->Disconnected))
        {
          PActiveTerminal->Disconnected = false;
          PActiveTerminal->DisconnectedTemporarily = false;
        }

        if (!PActiveTerminal->Active)
        {
          SaveTerminal(PActiveTerminal);
        }
      }

      if (ActiveTerminal)
      {
        int Index = ActiveTerminalIndex;
        if (!ActiveTerminal->Active && !FTerminationMessages->Strings[Index].IsEmpty())
        {
          UnicodeString Message = FTerminationMessages->Strings[Index];
          FTerminationMessages->Strings[Index] = L"";
          if (AutoReconnect)
          {
            ReconnectActiveTerminal();
          }
          else
          {
            Exception * E = new ESshFatal(NULL, Message);
            try
            {
              // finally show pending terminal message,
              // this gives user also possibility to reconnect
              ActiveTerminal->ShowExtendedException(E);
            }
            __finally
            {
              delete E;
            }
          }
        }
      }
      else
      {
        if (OnLastTerminalClosed)
        {
          OnLastTerminalClosed(this);
        }
      }


      if ((ActiveTerminal != NULL) && !ActiveTerminal->Active &&
          !ActiveTerminal->Disconnected)
      {
        ConnectActiveTerminal();
      }
    }
    __finally
    {
      if (NonVisualDataModule != NULL)
      {
        NonVisualDataModule->EndBusy();
      }
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::QueueStatusUpdated()
{
  UpdateAppTitle();
}
//---------------------------------------------------------------------------
bool __fastcall TTerminalManager::ShouldDisplayQueueStatusOnAppTitle()
{
  bool Result = IsApplicationMinimized();
  if (!Result && (ScpExplorer != NULL))
  {
    HWND Window = GetActiveWindow();
    Window = GetAncestor(Window, GA_ROOTOWNER);
    Result = (ScpExplorer->Handle != Window);
  }
  return Result;
}
//---------------------------------------------------------------------------
UnicodeString __fastcall TTerminalManager::FormatFormCaptionWithSession(TCustomForm * Form, const UnicodeString & Caption)
{
  return FormatFormCaption(Form, Caption, GetActiveTerminalTitle(false));
}
//---------------------------------------------------------------------------
UnicodeString __fastcall TTerminalManager::GetAppProgressTitle()
{
  UnicodeString Result;
  UnicodeString QueueProgressTitle;
  UnicodeString ProgressTitle = !FProgressTitle.IsEmpty() ? FProgressTitle : ScpExplorer->GetProgressTitle();
  if (!FForegroundProgressTitle.IsEmpty())
  {
    Result = FForegroundProgressTitle;
  }
  else if (!ProgressTitle.IsEmpty() && !ForegroundTask())
  {
    Result = ProgressTitle;
  }
  else if (ShouldDisplayQueueStatusOnAppTitle() &&
           !(QueueProgressTitle = ScpExplorer->GetQueueProgressTitle()).IsEmpty())
  {
    Result = QueueProgressTitle;
  }

  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::UpdateAppTitle()
{
  if (ScpExplorer)
  {
    TForm * MainForm = GetMainForm();
    if (MainForm != ScpExplorer)
    {
      // triggers caption update for some forms
      MainForm->Perform(WM_MANAGES_CAPTION, 0, 0);
    }

    UnicodeString NewTitle = FormatMainFormCaption(GetActiveTerminalTitle(false));

    UnicodeString ProgressTitle = GetAppProgressTitle();
    if (!ProgressTitle.IsEmpty())
    {
      NewTitle = ProgressTitle + L" - " + NewTitle;
    }
    else if (ActiveTerminal && (ScpExplorer != NULL))
    {
      UnicodeString Path = ScpExplorer->PathForCaption();
      if (!Path.IsEmpty())
      {
        NewTitle = Path + L" - " + NewTitle;
      }
    }

    // Not updating MainForm here, as for all other possible main forms, this code is actually not what we want.
    // And they all update their title on their own (some using GetAppProgressTitle()).
    ScpExplorer->Caption = NewTitle;
    ScpExplorer->ApplicationTitleChanged();
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::SaveTerminal(TTerminal * Terminal)
{
  TSessionData * Data = StoredSessions->FindSame(Terminal->SessionData);
  if (Data != NULL)
  {
    TManagedTerminal * ManagedTerminal = dynamic_cast<TManagedTerminal *>(Terminal);
    DebugAssert(ManagedTerminal != NULL);

    bool Changed = false;
    if (Terminal->SessionData->UpdateDirectories)
    {
      Data->CopyDirectoriesStateData(ManagedTerminal->StateData);
      Changed = true;
    }

    if (Changed)
    {
      // modified only, implicit
      StoredSessions->Save(false, false);
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::HandleException(Exception * E)
{
  // can be null for example when exception is thrown on login dialog
  if (ActiveTerminal != NULL)
  {
    ActiveTerminal->ShowExtendedException(E);
  }
  else
  {
    ShowExtendedException(E);
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::ApplicationException(TObject * /*Sender*/,
  Exception * E)
{
  HandleException(E);
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::ApplicationShowHint(UnicodeString & HintStr,
  bool & /*CanShow*/, THintInfo & HintInfo)
{
  HintInfo.HintData = HintInfo.HintControl;
  if (HasLabelHintPopup(HintInfo.HintControl, HintStr))
  {
    // Hack for transfer setting labels.
    // Should be converted to something like HintLabel()
    HintInfo.HideTimeout = 100000; // "almost" never
  }
  else if (dynamic_cast<TProgressBar *>(HintInfo.HintControl) != NULL)
  {
    // Hint is forcibly hidden in TProgressForm::FormHide
    HintInfo.HideTimeout = 100000; // "almost" never
    HintInfo.ReshowTimeout = 500; // updated each 0.5s
  }
  else
  {
    int HintMaxWidth = 300;

    TControl * ScaleControl = HintInfo.HintControl;
    if (DebugAlwaysFalse(HintInfo.HintControl == NULL) ||
        (GetParentForm(HintInfo.HintControl) == NULL))
    {
      ScaleControl = ScpExplorer;
    }
    HintMaxWidth = ScaleByTextHeight(ScaleControl, HintMaxWidth);

    HintInfo.HintMaxWidth = HintMaxWidth;
  }
}
//---------------------------------------------------------------------------
bool __fastcall TTerminalManager::HandleMouseWheel(WPARAM WParam, LPARAM LParam)
{
  // WORKAROUND This is no longer necessary on Windows 10 (except for WM_WANTS_MOUSEWHEEL_INACTIVE part)
  bool Result = false;
  if (Application->Active)
  {
    TPoint Point(LOWORD(LParam), HIWORD(LParam));
    TWinControl * Control = FindVCLWindow(Point);
    if (Control != NULL)
    {
      TCustomForm * Form = GetParentForm(Control);
      // Only case we expect the parent form to be NULL is on the Find/Replace dialog,
      // which is owned by VCL's internal TRedirectorWindow.
      DebugAssert((Form != NULL) || (Control->ClassName() == L"TRedirectorWindow"));
      if (Form != NULL)
      {
        // Send it only to windows we tested it with.
        // Though we should sooner or later remove this test and pass it to all our windows.
        if (Form->Active && (Form->Perform(WM_WANTS_MOUSEWHEEL, 0, 0) == 1))
        {
          SendMessage(Control->Handle, WM_MOUSEWHEEL, WParam, LParam);
          Result = true;
        }
        else if (!Form->Active && (Form->Perform(WM_WANTS_MOUSEWHEEL_INACTIVE, 0, 0) == 1))
        {
          TWinControl * Control2;
          // FindVCLWindow stops on window level, when the window is not active? or when there's a modal window over it?
          // (but in any case, when we have operation running on top of Synchronization checklist).
          // WORKAROUND: The while loop does what AllLevels parameter of ControlAtPos should do, but it's broken.
          // Based on (now removed) Embarcadero QC 82143.
          while ((Control2 = dynamic_cast<TWinControl *>(Control->ControlAtPos(Control->ScreenToClient(Point), false, true))) != NULL)
          {
            Control = Control2;
          }
          SendMessage(Control->Handle, WM_MOUSEWHEEL, WParam, LParam);
          Result = true;
        }
      }
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::ApplicationMessage(TMsg & Msg, bool & Handled)
{
  if (Msg.message == FTaskbarButtonCreatedMessage)
  {
    CreateTaskbarList();
  }
  if (Msg.message == WM_MOUSEWHEEL)
  {
    Handled = HandleMouseWheel(Msg.wParam, Msg.lParam);
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::ApplicationModalBegin(TObject * /*Sender*/)
{
  InterfaceStartDontMeasure();
  NonVisualDataModule->StartBusy();
  if (ScpExplorer != NULL)
  {
    ScpExplorer->SuspendWindowLock();
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::ApplicationModalEnd(TObject * /*Sender*/)
{
  NonVisualDataModule->EndBusy();
  if (ScpExplorer != NULL)
  {
    ScpExplorer->ResumeWindowLock();
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::InitTaskbarButtonCreatedMessage()
{
  // XE6 VCL already handles TaskbarButtonCreated, but does not call ChangeWindowMessageFilterEx.
  // So we keep our implementation.
  // See also https://stackoverflow.com/q/14614823/850848#14618587
  FTaskbarButtonCreatedMessage = RegisterWindowMessage(L"TaskbarButtonCreated");

  HINSTANCE User32Library = LoadLibrary(L"user32.dll");
  ChangeWindowMessageFilterExProc ChangeWindowMessageFilterEx =
    (ChangeWindowMessageFilterExProc)GetProcAddress(User32Library, "ChangeWindowMessageFilterEx");

  if (ChangeWindowMessageFilterEx != NULL)
  {
    // without this we won't get TaskbarButtonCreated, when app is running elevated
    ChangeWindowMessageFilterEx(
      Application->Handle, FTaskbarButtonCreatedMessage, MSGFLT_ALLOW, NULL);
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::CreateTaskbarList()
{

  ReleaseTaskbarList();

  if(SUCCEEDED(CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_ALL,
        IID_ITaskbarList3, (void **) &FTaskbarList)))
  {
    if (ScpExplorer != NULL)
    {
      UpdateTaskbarList();
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::ReleaseTaskbarList()
{
  if (FTaskbarList != NULL)
  {
    FTaskbarList->Release();
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::UpdateTaskbarList()
{
  ScpExplorer->UpdateTaskbarList(FTaskbarList);
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::DeleteLocalFile(const UnicodeString FileName, bool Alternative, int & Deleted)
{
  Deleted = RecursiveDeleteFileChecked(FileName, (WinConfiguration->DeleteToRecycleBin != Alternative));
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::TerminalQueryUser(TObject * Sender,
  const UnicodeString Query, TStrings * MoreMessages, unsigned int Answers,
  const TQueryParams * Params, unsigned int & Answer, TQueryType Type, void * /*Arg*/)
{
  UnicodeString HelpKeyword;
  TMessageParams MessageParams(Params);
  UnicodeString AQuery = Query;

  if (Params != NULL)
  {
    HelpKeyword = Params->HelpKeyword;

    if (FLAGSET(Params->Params, qpFatalAbort))
    {
      AQuery = FMTLOAD(WARN_FATAL_ERROR, (AQuery));

      if (!MessageParams.TimerMessage.IsEmpty())
      {
        MessageParams.TimerMessage = FMTLOAD(WARN_FATAL_ERROR, (MessageParams.TimerMessage));
      }
    }
  }

  if (ScpExplorer)
  {
    Answer = ScpExplorer->MoreMessageDialog(AQuery, MoreMessages, Type, Answers,
      HelpKeyword, &MessageParams, dynamic_cast<TTerminal *>(Sender));
  }
  else
  {
    Answer = MoreMessageDialog(AQuery, MoreMessages, Type, Answers, HelpKeyword,
      &MessageParams);
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::AuthenticateFormCancel(TObject * Sender)
{
  TAuthenticateForm * Form = dynamic_cast<TAuthenticateForm *>(Sender);
  DebugAssert(Form != NULL);
  TManagedTerminal * ManagedTerminal = dynamic_cast<TManagedTerminal *>(Form->Terminal);
  // will be null e.g. for background transfers
  if (ManagedTerminal != NULL)
  {
    TTerminalThread * TerminalThread = ManagedTerminal->TerminalThread;
    // can be NULL for reconnects from transfers
    if ((TerminalThread != NULL) && !TerminalThread->Cancelling)
    {
      Form->Log(LoadStr(AUTH_CANCELLING));
      TerminalThread->Cancel();
    }
  }
  FAuthenticationCancelled = true;
}
//---------------------------------------------------------------------------
TAuthenticateForm * __fastcall TTerminalManager::MakeAuthenticateForm(
  TTerminal * Terminal)
{
  TAuthenticateForm * Dialog = SafeFormCreate<TAuthenticateForm>();
  Dialog->Init(Terminal);
  DebugAssert(Dialog->OnCancel == NULL);
  Dialog->OnCancel = AuthenticateFormCancel;
  return Dialog;
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::FileNameInputDialogInitializeRenameBaseName(
  TObject * /*Sender*/, TInputDialogData * Data)
{
  EditSelectBaseName(Data->Edit->Handle);
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::TerminalPromptUser(
  TTerminal * Terminal, TPromptKind Kind, UnicodeString Name, UnicodeString Instructions,
  TStrings * Prompts, TStrings * Results, bool & Result, void * /*Arg*/)
{
  if (((Kind == pkPrompt) || (Kind == pkFileName)) && (FAuthenticateForm == NULL) &&
      (Terminal->Status != ssOpening))
  {
    DebugAssert(Instructions.IsEmpty());
    DebugAssert(Prompts->Count == 1);
    DebugAssert(FLAGSET(int(Prompts->Objects[0]), pupEcho));
    UnicodeString AResult = Results->Strings[0];

    TInputDialogInitialize InputDialogInitialize = NULL;
    if ((Kind == pkFileName) && !WinConfiguration->RenameWholeName)
    {
      InputDialogInitialize = FileNameInputDialogInitializeRenameBaseName;
    }

    Result = InputDialog(Name, Prompts->Strings[0], AResult, L"", NULL, false, InputDialogInitialize);
    if (Result)
    {
      Results->Strings[0] = AResult;
    }
  }
  else
  {
    TAuthenticateForm * AuthenticateForm = FAuthenticateForm;
    if (AuthenticateForm == NULL)
    {
      AuthenticateForm = MakeAuthenticateForm(Terminal);
    }

    try
    {
      Result = AuthenticateForm->PromptUser(Kind, Name, Instructions, Prompts, Results,
        (FAuthenticateForm != NULL), Terminal->StoredCredentialsTried);
    }
    __finally
    {
      if (FAuthenticateForm == NULL)
      {
        delete AuthenticateForm;
      }
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::TerminalDisplayBanner(
  TTerminal * Terminal, UnicodeString SessionName,
  const UnicodeString & Banner, bool & NeverShowAgain, int Options, unsigned int & Params)
{
  DebugAssert(FAuthenticateForm != NULL);
  TAuthenticateForm * AuthenticateForm = FAuthenticateForm;
  if (AuthenticateForm == NULL)
  {
    AuthenticateForm = MakeAuthenticateForm(Terminal);
  }

  try
  {
    AuthenticateForm->Banner(Banner, NeverShowAgain, Options, Params);
  }
  __finally
  {
    if (FAuthenticateForm == NULL)
    {
      delete AuthenticateForm;
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::TerminalShowExtendedException(
  TTerminal * Terminal, Exception * E, void * /*Arg*/)
{
  if (ScpExplorer)
  {
    ScpExplorer->ShowExtendedException(Terminal, E);
  }
  else
  {
    ShowExtendedExceptionEx(Terminal, E);
  }
}
//---------------------------------------------------------------------------
static TDateTime DirectoryReadingProgressDelay(0, 0, 1, 500);
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::TerminalReadDirectoryProgress(
  TObject * /*Sender*/, int Progress, int ResolvedLinks, bool & Cancel)
{
  if (Progress == 0)
  {
    if (ScpExplorer != NULL)
    {
      // See also TCustomScpExplorerForm::RemoteDirViewBusy
      ScpExplorer->LockWindow();
    }
    FDirectoryReadingStart = Now();
    if (!FProgressTitle.IsEmpty() || !FForegroundProgressTitle.IsEmpty())
    {
      FProgressTitle = L"";
      FForegroundProgressTitle = L"";
      UpdateAppTitle();
    }

    // Reset "was ESC ever pressed?" state
    GetAsyncKeyState(VK_ESCAPE);
  }
  else if (Progress < 0)
  {
    if (Progress == -2)
    {
      // cancelled
      if (ScpExplorer != NULL)
      {
        ScpExplorer->ReadDirectoryCancelled();
      }
    }
    else
    {
      if (ScpExplorer != NULL)
      {
        ScpExplorer->UnlockWindow();
      }

      FProgressTitle = L"";
      FForegroundProgressTitle = L"";
      UpdateAppTitle();
    }
  }
  else
  {
    // If the least significant bit is set,
    // the key was pressed after the previous call to GetAsyncKeyState.
    int KeyState = GetAsyncKeyState(VK_ESCAPE);
    if (FLAGSET(KeyState, 0x01))
    {
      Cancel = true;
    }

    if ((Now() - FDirectoryReadingStart) >= DirectoryReadingProgressDelay)
    {
      // 4 is arbitrary number
      FForegroundProgressTitle =
        FMTLOAD(ResolvedLinks >= 4 ? DIRECTORY_READING_AND_RESOLVING_PROGRESS : DIRECTORY_READING_PROGRESS,
          (Progress));
      UpdateAppTitle();
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::TerminalCustomCommand(
  TTerminal * /*Terminal*/, const UnicodeString & Command, bool & Handled)
{
  // Implementation has to be thread-safe
  Handled = CopyCommandToClipboard(Command);
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::AuthenticatingDone()
{
  FAuthenticating--;
  if (FAuthenticating == 0)
  {
    BusyEnd(FBusyToken);
    FBusyToken = NULL;
  }
  if (!FKeepAuthenticateForm)
  {
    CloseAutheticateForm();
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::TerminalInformation(
  TTerminal * Terminal, const UnicodeString & Str, bool DebugUsedArg(Status), int Phase, const UnicodeString & Additional)
{

  if (Phase == 1)
  {
    if (FAuthenticating == 0)
    {
      FBusyToken = BusyStart();
    }
    FAuthenticating++;
  }
  else if (Phase == 0)
  {
    DebugAssert(FAuthenticating > 0);
    AuthenticatingDone();
  }
  else
  {
    if (FAuthenticating > 0)
    {
      bool ShowPending = false;
      if (FAuthenticateForm == NULL)
      {
        FAuthenticateForm = MakeAuthenticateForm(Terminal);
        ShowPending = true;
      }
      FAuthenticateForm->Log(Str, Additional);
      if (ShowPending)
      {
        FAuthenticateForm->ShowAsModal();
      }
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::OperationFinished(::TFileOperation Operation,
  TOperationSide Side, bool Temp, const UnicodeString & FileName, bool Success,
  TOnceDoneOperation & OnceDoneOperation)
{
  DebugAssert(ScpExplorer);
  ScpExplorer->OperationFinished(Operation, Side, Temp, FileName, Success,
    OnceDoneOperation);
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::OperationProgress(
  TFileOperationProgressType & ProgressData)
{
  UpdateAppTitle();
  DebugAssert(ScpExplorer);
  ScpExplorer->OperationProgress(ProgressData);
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::QueueEvent(TTerminalQueue * Queue, TQueueEvent Event)
{
  TGuard Guard(FQueueSection);
  FQueueEvents.push_back(std::make_pair(Queue, Event));
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::DoConfigurationChange()
{
  DebugAssert(Configuration);
  DebugAssert(Configuration == WinConfiguration);

  TTerminalQueue * Queue;
  for (int Index = 0; Index < Count; Index++)
  {
    DebugAssert(Terminals[Index]->Log);
    Terminals[Index]->Log->ReflectSettings();
    Terminals[Index]->ActionLog->ReflectSettings();
    Queue = reinterpret_cast<TTerminalQueue *>(FQueues->Items[Index]);
    SetQueueConfiguration(Queue);
  }

  if (ScpExplorer)
  {
    ScpExplorer->ConfigurationChanged();
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::ConfigurationChange(TObject * /*Sender*/)
{
  if (FMainThread == GetCurrentThreadId())
  {
    DoConfigurationChange();
  }
  else
  {
    TGuard Guard(FChangeSection.get());
    FPendingConfigurationChange++;
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::TerminalReady()
{
  ScpExplorer->Terminal = ActiveTerminal;
  ScpExplorer->Queue = ActiveQueue;
  ScpExplorer->TerminalReady();
}
//---------------------------------------------------------------------------
TStrings * __fastcall TTerminalManager::GetTerminalList()
{
  FTerminalList->Clear();
  for (int i = 0; i < Count; i++)
  {
    TManagedTerminal * Terminal = Terminals[i];
    UnicodeString Name = GetTerminalTitle(Terminal, true);
    FTerminalList->AddObject(Name, Terminal);
  }
  return FTerminalList;
}
//---------------------------------------------------------------------------
int __fastcall TTerminalManager::GetActiveTerminalIndex()
{
  return ActiveTerminal ? IndexOf(ActiveTerminal) : -1;
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::SetActiveTerminalIndex(int value)
{
  ActiveTerminal = Terminals[value];
}
//---------------------------------------------------------------------------
UnicodeString __fastcall TTerminalManager::GetTerminalShortPath(TTerminal * Terminal)
{
  UnicodeString Result = UnixExtractFileName(Terminal->CurrentDirectory);
  if (Result.IsEmpty())
  {
    Result = Terminal->CurrentDirectory;
  }
  return Result;
}
//---------------------------------------------------------------------------
UnicodeString __fastcall TTerminalManager::GetTerminalTitle(TTerminal * Terminal, bool Unique)
{
  UnicodeString Result = Terminal->SessionData->SessionName;
  if (Unique &&
      (WinConfiguration->SessionTabNameFormat != stnfNone))
  {
    int Index = IndexOf(Terminal);
    // not for background transfer sessions and disconnected sessions
    if ((Index >= 0) && Terminal->Active)
    {
      for (int Index2 = 0; Index2 < Count; Index2++)
      {
        UnicodeString Name = Terminals[Index2]->SessionData->SessionName;
        if ((Terminals[Index2] != Terminal) &&
            Terminals[Index2]->Active &&
            SameText(Name, Result))
        {
          UnicodeString Path = GetTerminalShortPath(Terminal);
          if (!Path.IsEmpty())
          {
            const int MaxPathLen = 16;
            if ((WinConfiguration->SessionTabNameFormat == stnfShortPathTrunc) &&
                (Path.Length() > MaxPathLen))
            {
              Path = Path.SubString(1, MaxPathLen - 2) + Ellipsis;
            }
            Result = FORMAT(L"%s (%s)", (Result, Path));
          }
          break;
        }
      }
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
UnicodeString __fastcall TTerminalManager::GetActiveTerminalTitle(bool Unique)
{
  UnicodeString Result;
  if (ActiveTerminal != NULL)
  {
    Result = GetTerminalTitle(ActiveTerminal, Unique);
  }
  return Result;
}
//---------------------------------------------------------------------------
TTerminalQueue * __fastcall TTerminalManager::GetActiveQueue()
{
  TTerminalQueue * Result = NULL;
  if (ActiveTerminal != NULL)
  {
    Result = reinterpret_cast<TTerminalQueue *>(FQueues->Items[ActiveTerminalIndex]);
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::CycleTerminals(bool Forward)
{
  if (Count > 0)
  {
    int Index = ActiveTerminalIndex;
    Index += Forward ? 1 : -1;
    if (Index < 0)
    {
      Index = Count-1;
    }
    else if (Index >= Count)
    {
      Index = 0;
    }
    ActiveTerminalIndex = Index;
  }
}
//---------------------------------------------------------------------------
bool __fastcall TTerminalManager::CanOpenInPutty()
{
  return (ActiveTerminal != NULL) && !GUIConfiguration->PuttyPath.Trim().IsEmpty();
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::OpenInPutty()
{
  Configuration->Usage->Inc(L"OpenInPutty");

  TSessionData * Data = NULL;
  try
  {
    // Is NULL on the first session when called from ConnectActiveTerminal()
    // due to WinConfiguration->AutoOpenInPutty
    if (ScpExplorer != NULL)
    {
      Data = ScpExplorer->CloneCurrentSessionData();
    }
    else
    {
      Data = new TSessionData(L"");
      DebugAssert(ActiveTerminal != NULL);
      Data->Assign(ActiveTerminal->SessionData);
      ActiveTerminal->UpdateSessionCredentials(Data);
    }

    // putty does not support resolving environment variables in session settings
    Data->ExpandEnvironmentVariables();

    if (ActiveTerminal->TunnelLocalPortNumber != 0)
    {
      Data->ConfigureTunnel(ActiveTerminal->TunnelLocalPortNumber);
    }

    OpenSessionInPutty(GUIConfiguration->PuttyPath, Data);
  }
  __finally
  {
    delete Data;
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::NewSession(const UnicodeString & SessionUrl, bool ReloadSessions, TForm * LinkedForm)
{
  if (ReloadSessions)
  {
    StoredSessions->Reload();
  }

  std::unique_ptr<TObjectList> DataList;

  bool Retry;
  do
  {
    Retry = false;
    if (!DataList) // first round
    {
      DataList.reset(new TObjectList());
      UnicodeString DownloadFile; // unused
      GetLoginData(SessionUrl, NULL, DataList.get(), DownloadFile, true, LinkedForm);
    }
    else
    {
      if (!DoLoginDialog(DataList.get(), LinkedForm))
      {
        Abort(); // As GetLoginData would do
      }
    }

    if (DataList->Count > 0)
    {
      TManagedTerminal * ANewTerminal = NewTerminals(DataList.get());
      bool AdHoc = (DataList->Count == 1) && (StoredSessions->FindSame(reinterpret_cast<TSessionData *>(DataList->Items[0])) == NULL);
      bool CanRetry = SessionUrl.IsEmpty() && AdHoc;
      bool ShowLoginWhenNoSession = WinConfiguration->ShowLoginWhenNoSession;
      if (CanRetry)
      {
        // we will show our own login dialog, so prevent opening an empty one
        WinConfiguration->ShowLoginWhenNoSession = false;
      }
      try
      {
        ActiveTerminal = ANewTerminal;
      }
      __finally
      {
        if (CanRetry) // do not reset the value, unless really needed, as it can theoretically be changed meanwhile by the user
        {
          WinConfiguration->ShowLoginWhenNoSession = ShowLoginWhenNoSession;
        }
      }
      Retry = CanRetry && (ActiveTerminal != ANewTerminal);
    }
  }
  while (Retry);
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::Idle(bool SkipCurrentTerminal)
{

  if (FPendingConfigurationChange > 0) // optimization
  {
    bool Changed = false;

    {
      TGuard Guard(FChangeSection.get());
      if (DebugAlwaysTrue(FPendingConfigurationChange > 0))
      {
        FPendingConfigurationChange--;
        Changed = true;
      }
    }

    if (Changed)
    {
      DoConfigurationChange();
    }
  }

  for (int Index = 0; Index < Count; Index++)
  {
    TManagedTerminal * Terminal = Terminals[Index];
    try
    {
      if (!SkipCurrentTerminal || (Terminal != ActiveTerminal))
      {
        // make sure Idle is called on the thread that runs the terminal
        if (Terminal->TerminalThread != NULL)
        {
          Terminal->TerminalThread->Idle();
        }
        else
        {
          if (Terminal->Active)
          {
            Terminal->Idle();
          }
        }

        if (Terminal->Active)
        {
          DebugAssert(Index < FQueues->Count);
          if (Index < FQueues->Count)
          {
            reinterpret_cast<TTerminalQueue *>(FQueues->Items[Index])->Idle();
          }
        }
      }
    }
    catch(Exception & E)
    {
      if (Terminal == ActiveTerminal)
      {
        // throw further, so that the exception is handled in proper place
        // (particularly in broken-transfer reconnect handler, bug 72)
        throw;
      }
      else
      {
        // we may not have inactive terminal, unless there is a explorer,
        // also Idle is called from explorer anyway
        DebugAssert(ScpExplorer != NULL);
        if (ScpExplorer != NULL)
        {
          ScpExplorer->InactiveTerminalException(Terminal, &E);
        }

        if (!Terminal->Active)
        {
          // if session is lost, save the error message and rethrow it
          // once the terminal gets activated
          FTerminationMessages->Strings[Index] = E.Message;
        }
      }
    }
  }

  TTerminalQueue * QueueWithEvent;
  TQueueEvent QueueEvent;

  do
  {
    QueueWithEvent = NULL;

    {
      TGuard Guard(FQueueSection);

      if (!FQueueEvents.empty())
      {
        QueueWithEvent = FQueueEvents[0].first;
        QueueEvent = FQueueEvents[0].second;
        FQueueEvents.erase(FQueueEvents.begin());
      }
    }

    if (QueueWithEvent != NULL)
    {
      int Index = FQueues->IndexOf(QueueWithEvent);
      // the session may not exist anymore
      if (Index >= 0)
      {
        TManagedTerminal * Terminal = Terminals[Index];
        // we can hardly have a queue event without explorer
        DebugAssert(ScpExplorer != NULL);
        if (ScpExplorer != NULL)
        {
          ScpExplorer->QueueEvent(Terminal, QueueWithEvent, QueueEvent);
        }
      }
    }
  }
  while (QueueWithEvent != NULL);
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::MasterPasswordPrompt()
{
  if (GetCurrentThreadId() == MainThreadID)
  {
    if (!DoMasterPasswordDialog())
    {
      Abort();
    }
  }
  else
  {
    // this can happen only when we keep cancelling all master password prompts
    // as long as the sessing finally connects (session password has to be
    // explictly typed in), and background transfer is started
    Abort();
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::Move(TTerminal * Source, TTerminal * Target)
{
  int SourceIndex = IndexOf(Source);
  int TargetIndex = IndexOf(Target);
  TTerminalList::Move(SourceIndex, TargetIndex);
  FQueues->Move(SourceIndex, TargetIndex);
  DoTerminalListChanged();
  // when there are indexed sessions with the same name,
  // the index may change when reordering the sessions
  UpdateAppTitle();
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::DoTerminalListChanged()
{
  if (OnTerminalListChanged)
  {
    OnTerminalListChanged(this);
  }
}
//---------------------------------------------------------------------------
void __fastcall TTerminalManager::SaveWorkspace(TList * DataList)
{
  for (int Index = 0; Index < Count; Index++)
  {
    TManagedTerminal * ManagedTerminal = Terminals[Index];
    TSessionData * Data = StoredSessions->SaveWorkspaceData(ManagedTerminal->StateData, Index);
    DataList->Add(Data);
  }
}
//---------------------------------------------------------------------------
bool __fastcall TTerminalManager::IsActiveTerminalForSite(TTerminal * Terminal, TSessionData * Data)
{
  bool Result = Terminal->Active;
  if (Result)
  {
    std::unique_ptr<TSessionData> TerminalData(Terminal->SessionData->Clone());
    Terminal->UpdateSessionCredentials(TerminalData.get());
    Result = TerminalData->IsSameSite(Data);
  }
  return Result;
}
//---------------------------------------------------------------------------
TTerminal * __fastcall TTerminalManager::FindActiveTerminalForSite(TSessionData * Data)
{
  TTerminal * Result = NULL;
  for (int Index = 0; (Result == NULL) && (Index < Count); Index++)
  {
    TTerminal * Terminal = Terminals[Index];
    if (IsActiveTerminalForSite(Terminal, Data))
    {
      Result = Terminal;
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
TTerminalQueue * __fastcall TTerminalManager::FindQueueForTerminal(TTerminal * Terminal)
{
  int Index = IndexOf(Terminal);
  return reinterpret_cast<TTerminalQueue *>(FQueues->Items[Index]);
}
//---------------------------------------------------------------------------
bool __fastcall TTerminalManager::UploadPublicKey(
  TTerminal * Terminal, TSessionData * Data, UnicodeString & FileName)
{
  std::unique_ptr<TOpenDialog> OpenDialog(new TOpenDialog(Application));
  OpenDialog->Title = LoadStr(LOGIN_PUBLIC_KEY_TITLE);
  OpenDialog->Filter = LoadStr(LOGIN_PUBLIC_KEY_FILTER);
  OpenDialog->DefaultExt = PuttyKeyExt;
  OpenDialog->FileName = FileName;

  bool Result = OpenDialog->Execute();
  if (Result)
  {
    Configuration->Usage->Inc(L"PublicKeyInstallation");
    FileName = OpenDialog->FileName;

    VerifyAndConvertKey(FileName, false);

    bool AdHocTerminal = (Terminal == NULL);
    std::unique_ptr<TTerminal> TerminalOwner;
    if (AdHocTerminal)
    {
      DebugAssert(Data != NULL);

      TAutoFlag KeepAuthenticateFormFlag(FKeepAuthenticateForm);
      try
      {
        TerminalOwner.reset(CreateTerminal(Data));
        Terminal = TerminalOwner.get();
        SetupTerminal(Terminal);
        Terminal->OnProgress = NULL;
        Terminal->OnFinished = NULL;
        DoConnectTerminal(Terminal, false, true);
      }
      catch (Exception & E)
      {
        CloseAutheticateForm();
        throw;
      }
    }

    UnicodeString Installed;
    try
    {
      UnicodeString SshImplementation = Terminal->GetSessionInfo().SshImplementation;
      UnicodeString NotOpenSSHMessage = FMTLOAD(LOGIN_NOT_OPENSSH, (SshImplementation));
      if (IsOpenSSH(SshImplementation) ||
          (MessageDialog(NotOpenSSHMessage, qtConfirmation, qaOK | qaCancel, HELP_LOGIN_AUTHORIZED_KEYS) == qaOK))
      {
        // Ad-hoc terminal
        if (FAuthenticateForm != NULL)
        {
          UnicodeString Comment;
          GetPublicKeyLine(FileName, Comment);
          FAuthenticateForm->Log(FMTLOAD(LOGIN_PUBLIC_KEY_UPLOAD, (Comment)));
        }

        Installed = Terminal->UploadPublicKey(FileName);
      }
    }
    __finally
    {
      CloseAutheticateForm(); // When uploading from Login dialog
    }

    if (!Installed.IsEmpty())
    {
      Terminal->LogEvent(L"Public key installation done.");
      if (AdHocTerminal)
      {
        TerminalOwner.reset(NULL);
      }
      else
      {
        Terminal->Log->AddSeparator();
      }

      MessageDialog(Installed, qtInformation, qaOK, HELP_LOGIN_AUTHORIZED_KEYS);
    }
  }

  return Result;
}
