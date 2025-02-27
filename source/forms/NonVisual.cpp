//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include <Common.h>

#include "NonVisual.h"

#include <CoreMain.h>
#include <TextsWin.h>
#include <Tools.h>
#include <Setup.h>

#include <Interface.h>
#include "WinConfiguration.h"
#include "TerminalManager.h"
#include "TBX.hpp"
#include "VCLCommon.h"
#include <HistoryComboBox.hpp>
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "TB2Item"
#pragma link "TBX"
#pragma link "TB2ExtItems"
#pragma link "TBXExtItems"
#pragma link "TBXToolPals"
#pragma resource "*.dfm"
TNonVisualDataModule *NonVisualDataModule;
//---------------------------------------------------------------------------
#define SCPCOMMANDER ((TScpCommanderForm *)ScpExplorer)
#define UPDEX(HandleAction, Condition, OtherEnabled, OtherDisabled) if (Action == HandleAction) { \
  ((TCustomAction *)Action)->Enabled = (Condition); \
  if (((TCustomAction *)Action)->Enabled) { OtherEnabled; } else { OtherDisabled; }; \
  Handled = true; } else
#define UPDEX1(HandleAction, Condition, Other) UPDEX(HandleAction, Condition, Other, Other)
#define UPD(HandleAction, Condition) if (Action == HandleAction) { \
  ((TCustomAction *)Action)->Enabled = (Condition); Handled = true; } else
#define UPDFUNC(HandleAction, Function) if (Action == HandleAction) { Function; Handled = true; } else
#define EXE(HandleAction, Command) if (Action == HandleAction) { \
  Command; Handled = true; } else
#define UPDACT(HandleAction, Command) \
  EXE(HandleAction, ((TCustomAction *)Action)->Enabled = true; Command)
#define UPDCOMP(COMP) if (Action == COMP ## Action) { COMP ## Action->Enabled = true; \
  COMP ## Action->Visible = ScpExplorer->IsComponentPossible(fc ## COMP); \
  COMP ## Action->Checked = ScpExplorer->ComponentVisible[fc ## COMP]; Handled = true; } else
#define EXECOMP(COMP) EXE(COMP ## Action, \
  ScpExplorer->ComponentVisible[fc ## COMP] = !ScpExplorer->ComponentVisible[fc ## COMP] )
#define COLPROPS(SIDE) \
  ((TCustomDirViewColProperties*)ScpExplorer->DirView(os ## SIDE)->ColProperties)
#define UPDSORT(SIDE, PREFIX, COL) if (Action == SIDE ## SortBy ## COL ## Action) { \
  SIDE ## SortBy ## COL ## Action->Enabled = true; Handled = true; \
  SIDE ## SortBy ## COL ## Action->Checked = (COLPROPS(SIDE)->SortColumn == PREFIX ## COL); } else
#define EXESORT(SIDE, PREFIX, COL) EXE(SIDE ## SortBy ## COL ## Action, \
    if (COLPROPS(SIDE)->SortColumn == PREFIX ## COL) \
      COLPROPS(SIDE)->SortAscending = !COLPROPS(SIDE)->SortAscending; \
    else COLPROPS(SIDE)->SortColumn = PREFIX ## COL )
#define UPDSORTA(SIDE) if (Action == SIDE ## SortAscendingAction) { \
  SIDE ## SortAscendingAction->Enabled = true; Handled = true; \
  SIDE ## SortAscendingAction->Checked = COLPROPS(SIDE)->SortAscending; } else
#define EXESORTA(SIDE) EXE(SIDE ## SortAscendingAction, \
  COLPROPS(SIDE)->SortAscending = !COLPROPS(SIDE)->SortAscending; )
#define UPDSORTC(LPREFIX, LCOL, RPREFIX, RCOL) if (Action == CurrentSortBy ## RCOL ## Action) { \
  CurrentSortBy ## RCOL ## Action->Enabled = ScpExplorer->AllowedAction(Action, aaShortCut); \
  if (CurrentSortBy ## RCOL ## Action->Enabled) { \
    if (ScpExplorer->DirView(osCurrent) == ScpExplorer->DirView(osRemote)) \
         CurrentSortBy ## RCOL ## Action->Checked = (COLPROPS(Current)->SortColumn == RPREFIX ## RCOL); \
    else CurrentSortBy ## RCOL ## Action->Checked = (COLPROPS(Current)->SortColumn == LPREFIX ## LCOL); \
  } else CurrentSortBy ## RCOL ## Action->Checked =  false; Handled = true; } else
#define EXESORTC(COL, LCOL, RCOL) \
  EXE(CurrentSortBy ## COL ## Action, \
    Integer NewSortCol = \
      ((ScpExplorer->DirView(osCurrent) == ScpExplorer->DirView(osRemote)) ? RCOL : LCOL); \
    if (COLPROPS(Current)->SortColumn == NewSortCol) \
      COLPROPS(Current)->SortAscending = !COLPROPS(Current)->SortAscending; \
    else COLPROPS(Current)->SortColumn = NewSortCol \
  )
#define UPDSHCOL(SIDE, PREFIX, COL) \
  EXE(ShowHide ## SIDE ## COL ## ColumnAction, \
    ShowHide ## SIDE ## COL ## ColumnAction->Checked = COLPROPS(SIDE)->Visible[PREFIX ## COL])
#define EXESHCOL(SIDE, PREFIX, COL) \
  EXE(ShowHide ## SIDE ## COL ## ColumnAction, \
    COLPROPS(SIDE)->Visible[PREFIX ## COL] = !COLPROPS(SIDE)->Visible[PREFIX ## COL])

#define BAND_COMPONENTS \
  EMIT_BAND_COMPONENT(ExplorerMenuBand) \
  EMIT_BAND_COMPONENT(ExplorerAddressBand) \
  EMIT_BAND_COMPONENT(ExplorerToolbarBand) \
  EMIT_BAND_COMPONENT(ExplorerSelectionBand) \
  EMIT_BAND_COMPONENT(ExplorerSessionBand) \
  EMIT_BAND_COMPONENT(ExplorerPreferencesBand) \
  EMIT_BAND_COMPONENT(ExplorerSortBand) \
  EMIT_BAND_COMPONENT(ExplorerUpdatesBand) \
  EMIT_BAND_COMPONENT(ExplorerTransferBand) \
  EMIT_BAND_COMPONENT(ExplorerCustomCommandsBand) \
  EMIT_BAND_COMPONENT(CommanderMenuBand) \
  EMIT_BAND_COMPONENT(CommanderSessionBand) \
  EMIT_BAND_COMPONENT(CommanderPreferencesBand) \
  EMIT_BAND_COMPONENT(CommanderSortBand) \
  EMIT_BAND_COMPONENT(CommanderCommandsBand) \
  EMIT_BAND_COMPONENT(CommanderUpdatesBand) \
  EMIT_BAND_COMPONENT(CommanderTransferBand) \
  EMIT_BAND_COMPONENT(CommanderCustomCommandsBand) \
  EMIT_BAND_COMPONENT(CommanderLocalHistoryBand) \
  EMIT_BAND_COMPONENT(CommanderLocalNavigationBand) \
  EMIT_BAND_COMPONENT(CommanderLocalFileBand) \
  EMIT_BAND_COMPONENT(CommanderLocalSelectionBand) \
  EMIT_BAND_COMPONENT(CommanderRemoteHistoryBand) \
  EMIT_BAND_COMPONENT(CommanderRemoteNavigationBand) \
  EMIT_BAND_COMPONENT(CommanderRemoteFileBand) \
  EMIT_BAND_COMPONENT(CommanderRemoteSelectionBand)
//---------------------------------------------------------------------------
__fastcall TNonVisualDataModule::TNonVisualDataModule(TComponent* Owner)
        : TDataModule(Owner)
{
  FListColumn = NULL;
  FSessionIdleTimerExecuting = false;
  FCustomizedToolbar = NULL;
  FBusy = 0;

  QueueSpeedComboBoxItem(QueuePopupSpeedComboBoxItem);
}
//---------------------------------------------------------------------------
__fastcall TNonVisualDataModule::~TNonVisualDataModule()
{
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::ExplorerActionsUpdate(
  TBasicAction * BasicAction, bool & Handled)
{
  TAction * Action = DebugNotNull(dynamic_cast<TAction *>(BasicAction));
  if (!ScpExplorer || !ScpExplorer->AllowedAction(Action, aaUpdate))
  {
    Action->Enabled = false;
    Handled = true;
    return;
  }
  void * Param;
  #define HasTerminal ScpExplorer->HasActiveTerminal()
  // CURRENT DIRVIEW
  #define EnabledSelectedOperation (ScpExplorer->EnableSelectedOperation[osCurrent])
  #define EnabledFocusedOperation (ScpExplorer->EnableFocusedOperation[osCurrent])
  #define EnabledSelectedFileOperation (ScpExplorer->EnableSelectedFileOperation[osCurrent])
  #define EnabledFocusedFileOperation (ScpExplorer->EnableFocusedFileOperation[osCurrent])
  #define EnabledLocalSelectedOperation (ScpExplorer->HasDirView[osLocal] && ScpExplorer->EnableSelectedOperation[osLocal])
  #define EnabledLocalFocusedOperation (ScpExplorer->HasDirView[osLocal] && ScpExplorer->EnableFocusedOperation[osLocal])
  #define EnabledLocalSelectedFileOperation (ScpExplorer->HasDirView[osLocal] && ScpExplorer->EnableSelectedFileOperation[osLocal])
  #define EnabledRemoteSelectedOperation (ScpExplorer->EnableSelectedOperation[osRemote])
  #define EnabledRemoteFocusedOperation (ScpExplorer->EnableFocusedOperation[osRemote])
  #define EnabledRemoteSelectedFileOperation (ScpExplorer->EnableSelectedFileOperation[osRemote])
  // focused operation
  UPD(CurrentDeleteFocusedAction, EnabledFocusedOperation)
  UPD(CurrentPropertiesFocusedAction, EnabledFocusedOperation)
  UPD(CurrentEditFocusedAction, EnabledFocusedFileOperation &&
    !WinConfiguration->DisableOpenEdit)
  UPD(CurrentSystemMenuFocusedAction, EnabledFocusedOperation)
  UPD(CurrentEditWithFocusedAction, EnabledFocusedFileOperation &&
    !WinConfiguration->DisableOpenEdit)
  UPD(CurrentEditInternalFocusedAction, EnabledFocusedFileOperation &&
    !WinConfiguration->DisableOpenEdit)
  UPD(CurrentCopyToClipboardFocusedAction, EnabledFocusedOperation)
  // file operation
  UPD(CurrentRenameAction, EnabledFocusedOperation &&
    ((ScpExplorer->HasDirView[osLocal] && DirView(osLocal) == DirView(osCurrent)) ||
      ScpExplorer->Terminal->IsCapable[fcRename]))
  UPD(CurrentEditAction, EnabledSelectedFileOperation &&
    !WinConfiguration->DisableOpenEdit)
  UPD(CurrentEditInternalAction, EnabledSelectedFileOperation &&
    !WinConfiguration->DisableOpenEdit)
  UPD(CurrentEditWithAction, EnabledSelectedFileOperation &&
    !WinConfiguration->DisableOpenEdit)
  UPD(CurrentOpenAction, EnabledFocusedOperation &&
    !WinConfiguration->DisableOpenEdit)
  UPDEX1(CurrentAddEditLinkContextAction, ScpExplorer->CanAddEditLink(osCurrent),
    Action->Visible = ScpExplorer->LinkFocused())
  UPD(NewLinkAction, ScpExplorer->CanAddEditLink(osCurrent))
  // selected operation
  UPD(CurrentDeleteAction, EnabledSelectedOperation)
  UPD(CurrentDeleteAlternativeAction, EnabledSelectedOperation)
  UPD(CurrentPropertiesAction, EnabledSelectedOperation)
  UPD(CurrentCopyToClipboardAction, EnabledSelectedOperation)
  UPD(RemoteMoveToAction, EnabledSelectedOperation &&
    (DirView(osRemote) == DirView(osCurrent)) &&
    ScpExplorer->Terminal->IsCapable[fcRemoteMove])
  UPD(RemoteCopyToAction, EnabledSelectedOperation &&
    (DirView(osRemote) == DirView(osCurrent)))
  UPD(FileListToCommandLineAction, EnabledSelectedOperation)
  UPD(FileListToClipboardAction, EnabledSelectedOperation)
  UPD(FullFileListToClipboardAction, EnabledSelectedOperation)
  UPD(FileGenerateUrlAction2, EnabledSelectedOperation && (DirView(osRemote) == DirView(osCurrent)))
  UPD(FileListFromClipboardAction, IsFormatInClipboard(CF_TEXT))
  UPD(CurrentAddEditLinkAction, ScpExplorer->CanAddEditLink(osCurrent))
  UPD(LockAction,
    EnabledSelectedOperation && (DirView(osRemote) == DirView(osCurrent)) &&
    ScpExplorer->Terminal->IsCapable[fcLocking])
  UPD(UnlockAction,
    EnabledSelectedOperation && (DirView(osRemote) == DirView(osCurrent)) &&
    ScpExplorer->Terminal->IsCapable[fcLocking])
  // local selected operation
  UPD(LocalCopyAction, HasTerminal && EnabledLocalSelectedOperation)
  UPD(LocalCopyQueueAction, HasTerminal && EnabledLocalSelectedOperation && ScpExplorer->Terminal->IsCapable[fcBackgroundTransfers])
  UPD(LocalCopyNonQueueAction, HasTerminal && EnabledLocalSelectedOperation)
  UPD(LocalRenameAction, EnabledLocalSelectedOperation)
  UPD(LocalEditAction, EnabledLocalSelectedFileOperation && !WinConfiguration->DisableOpenEdit)
  UPD(LocalMoveAction, HasTerminal && EnabledLocalSelectedOperation)
  UPD(LocalCreateDirAction2, true)
  UPD(LocalDeleteAction, EnabledLocalSelectedOperation)
  UPD(LocalPropertiesAction, EnabledLocalSelectedOperation)
  UPD(LocalAddEditLinkAction2, ScpExplorer->CanAddEditLink(osLocal))
  UPD(LocalNewFileAction, !WinConfiguration->DisableOpenEdit)
  // local focused operation
  UPD(LocalCopyFocusedAction, HasTerminal && EnabledLocalFocusedOperation)
  UPD(LocalCopyFocusedQueueAction, HasTerminal && EnabledLocalFocusedOperation && ScpExplorer->Terminal->IsCapable[fcBackgroundTransfers])
  UPD(LocalCopyFocusedNonQueueAction, HasTerminal && EnabledLocalFocusedOperation)
  UPD(LocalMoveFocusedAction, HasTerminal && EnabledLocalFocusedOperation)
  // remote selected operation
  UPD(RemoteCopyAction, EnabledRemoteSelectedOperation)
  UPD(RemoteCopyQueueAction, EnabledRemoteSelectedOperation && ScpExplorer->Terminal->IsCapable[fcBackgroundTransfers])
  UPD(RemoteCopyNonQueueAction, EnabledRemoteSelectedOperation)
  UPD(RemoteRenameAction, EnabledRemoteSelectedOperation &&
    ScpExplorer->Terminal->IsCapable[fcRename])
  UPD(RemoteEditAction, EnabledRemoteSelectedFileOperation && !WinConfiguration->DisableOpenEdit)
  UPD(RemoteMoveAction, EnabledRemoteSelectedOperation)
  UPD(RemoteCreateDirAction2, DirViewEnabled(osRemote))
  UPD(RemoteNewFileAction, DirViewEnabled(osRemote) && !WinConfiguration->DisableOpenEdit)
  UPD(RemoteDeleteAction, EnabledRemoteSelectedOperation)
  UPD(RemotePropertiesAction, EnabledRemoteSelectedOperation)
  UPD(RemoteAddEditLinkAction2, ScpExplorer->CanAddEditLink(osRemote))
  // remote focused operation
  UPD(RemoteCopyFocusedAction, EnabledRemoteFocusedOperation)
  UPD(RemoteCopyFocusedQueueAction, EnabledRemoteFocusedOperation && ScpExplorer->Terminal->IsCapable[fcBackgroundTransfers])
  UPD(RemoteCopyFocusedNonQueueAction, EnabledRemoteFocusedOperation)
  UPD(RemoteMoveFocusedAction, EnabledRemoteFocusedOperation)
  UPD(RemoteMoveToFocusedAction, EnabledFocusedOperation &&
    (DirView(osRemote) == DirView(osCurrent)) &&
    ScpExplorer->Terminal->IsCapable[fcRemoteMove])
  UPD(RemoteCopyToFocusedAction, EnabledFocusedOperation &&
    DirView(osRemote) == DirView(osCurrent))
  // directory
  UPD(CurrentCreateDirAction, DirViewEnabled(osCurrent))
  UPD(NewDirAction, DirViewEnabled(osCurrent))
  UPD(RemoteFindFilesAction, DirViewEnabled(osRemote))
  // selection
  UPD(SelectOneAction, DirView(osCurrent)->FilesCount)
  UPD(SelectAction, DirView(osCurrent)->FilesCount)
  UPD(UnselectAction, DirView(osCurrent)->SelCount)
  UPD(SelectAllAction, DirView(osCurrent)->FilesCount)
  UPD(InvertSelectionAction, DirView(osCurrent)->FilesCount)
  UPD(ClearSelectionAction, DirView(osCurrent)->SelCount)
  UPD(RestoreSelectionAction, DirView(osCurrent)->SelectedNamesSaved)
  UPD(SelectSameExtAction, EnabledFocusedFileOperation)
  UPD(UnselectSameExtAction, EnabledFocusedFileOperation)
  UPD(PasteAction2, ScpExplorer->CanPasteFromClipBoard())
  UPD(LocalSelectAction, ScpExplorer->HasDirView[osLocal] && DirView(osLocal)->FilesCount)
  UPD(LocalUnselectAction, ScpExplorer->HasDirView[osLocal] && DirView(osLocal)->SelCount)
  UPD(LocalSelectAllAction, ScpExplorer->HasDirView[osLocal] && DirView(osLocal)->FilesCount)
  UPD(RemoteSelectAction, DirView(osRemote)->FilesCount)
  UPD(RemoteUnselectAction, DirView(osRemote)->SelCount)
  UPD(RemoteSelectAllAction, DirView(osRemote)->FilesCount)

  //style
  UPDACT(CurrentCycleStyleAction,
    CurrentCycleStyleAction->ImageIndex = 8 + (DirView(osCurrent)->ViewStyle + 1) % 4)
  #define STYLEACTION(Style) UPDACT(Current ## Style ## Action, \
    Current ## Style ## Action->Checked = (DirView(osCurrent)->ViewStyle == vs ## Style))
  STYLEACTION(Icon)
  STYLEACTION(SmallIcon)
  STYLEACTION(List)
  STYLEACTION(Report)
  #undef STYLEACTION

  // REMOTE+LOCAL
  // back/forward
  #define HISTORYACTION(SIDE, DIRECTION, HINTFMT, DELTA) \
    UPDEX(SIDE ## DIRECTION ## Action, DirViewEnabled(os ## SIDE) && (DirView(os ## SIDE)->DIRECTION ## Count > 0), \
    SIDE ## DIRECTION ## Action->Hint = FMTLOAD(HINTFMT, (DirView(os ## SIDE)->HistoryPath[DELTA])), \
    SIDE ## DIRECTION ## Action->Hint = L"")
  HISTORYACTION(Local, Back, EXPLORER_BACK_HINT, -1)
  HISTORYACTION(Local, Forward, EXPLORER_FORWARD_HINT, 1)
  HISTORYACTION(Remote, Back, EXPLORER_BACK_HINT, -1)
  HISTORYACTION(Remote, Forward, EXPLORER_FORWARD_HINT, 1)
  #undef HISTORYACTION
  #define PANEL_ACTIONS(SIDE) \
    UPD(SIDE ## ParentDirAction, DirViewEnabled(os ## SIDE) && !DirView(os ## SIDE)->IsRoot) \
    UPD(SIDE ## RootDirAction, DirViewEnabled(os ## SIDE) &&!DirView(os ## SIDE)->IsRoot) \
    UPD(SIDE ## HomeDirAction, DirViewEnabled(os ## SIDE)) \
    UPD(SIDE ## RefreshAction, DirViewEnabled(os ## SIDE) && DirView(os ## SIDE)->DirOK) \
    UPD(SIDE ## OpenDirAction, DirViewEnabled(os ## SIDE)) \
    UPD(SIDE ## ChangePathAction, DirViewEnabled(os ## SIDE)) \
    UPD(SIDE ## AddBookmarkAction, DirViewEnabled(os ## SIDE)) \
    UPD(SIDE ## PathToClipboardAction, DirViewEnabled(os ## SIDE)) \
    UPDEX1(SIDE ## FilterAction, DirViewEnabled(os ## SIDE), Action->Checked = !DirView(os ## SIDE)->Mask.IsEmpty())
  PANEL_ACTIONS(Local)
  PANEL_ACTIONS(Remote)
  #undef PANEL_ACTIONS
  UPD(LocalExploreDirectoryAction, true)

  // HELP
  UPD(AboutAction, true)
  UPD(HomepageAction, true)
  UPD(HistoryPageAction, true)
  UPD(TableOfContentsAction, true)
  UPD(ForumPageAction, true)
  UPDACT(CheckForUpdatesAction, ShowUpdatesUpdate())
  UPD(UpdatesPreferencesAction, true)
  UPDEX1(DonatePageAction, true, DonatePageAction->Visible = !IsUWP())
  UPD(DownloadPageAction, true)
  UPDEX1(TipsAction, true, TipsAction->Visible = AnyTips())

  // VIEW
  UPDCOMP(SessionsTabs)
  UPDCOMP(StatusBar)
  UPDCOMP(ToolBar2)
  UPDCOMP(LocalStatusBar)
  UPDCOMP(RemoteStatusBar)
  UPDCOMP(CommandLinePanel)
  UPDCOMP(RemoteTree)
  UPDCOMP(LocalTree)
  #define EMIT_BAND_COMPONENT(COMP) UPDCOMP(COMP)
  BAND_COMPONENTS
  #undef EMIT_BAND_COMPONENT

  UPD(GoToCommandLineAction, true)
  UPD(GoToTreeAction, true)
  UPDEX(ShowHiddenFilesAction, true,
    ShowHiddenFilesAction->Checked = WinConfiguration->ShowHiddenFiles, )
  UPDEX(FormatSizeBytesNoneAction, true,
    FormatSizeBytesNoneAction->Checked = (WinConfiguration->FormatSizeBytes == fbNone), )
  UPDEX(FormatSizeBytesKilobytesAction, true,
    FormatSizeBytesKilobytesAction->Checked = (WinConfiguration->FormatSizeBytes == fbKilobytes), )
  UPDEX(FormatSizeBytesShortAction, true,
    FormatSizeBytesShortAction->Checked = (WinConfiguration->FormatSizeBytes == fbShort), )
  UPDEX(AutoReadDirectoryAfterOpAction, true,
    AutoReadDirectoryAfterOpAction->Checked = Configuration->AutoReadDirectoryAfterOp, )
  UPD(PreferencesAction, true)
  UPD(PresetsPreferencesAction, true)
  UPD(FileColorsPreferencesAction, true)
  UPDEX(LockToolbarsAction, true,
    LockToolbarsAction->Checked = WinConfiguration->LockToolbars, )
  UPDEX(SelectiveToolbarTextAction, true,
    SelectiveToolbarTextAction->Checked = WinConfiguration->SelectiveToolbarText, )
  UPDCOMP(CustomCommandsBand)
  UPD(ColorMenuAction, HasTerminal)
  UPD(GoToAddressAction, true)
  UPD(CustomizeToolbarAction, IsToolbarCustomizable())

  // SORT
  UPDSORTA(Local)
  UPDSORT(Local, dv, Name)
  UPDSORT(Local, dv, Ext)
  UPDSORT(Local, dv, Size)
  UPDSORT(Local, dv, Type)
  UPDSORT(Local, dv, Changed)
  UPDSORT(Local, dv, Attr)
  UPDSORTA(Remote)
  UPDSORT(Remote, uv, Name)
  UPDSORT(Remote, uv, Ext)
  UPDSORT(Remote, uv, Size)
  UPDSORT(Remote, uv, Changed)
  UPDSORT(Remote, uv, Rights)
  UPDSORT(Remote, uv, Owner)
  UPDSORT(Remote, uv, Group)
  UPDSORT(Remote, uv, Type)
  UPDSORTA(Current)
  UPDSORTC(dv, Name, uv, Name)
  UPDSORTC(dv, Ext, uv, Ext)
  UPDSORTC(dv, Size, uv, Size)
  UPDSORTC(dv, Type, uv, Type)
  UPDSORTC(dv, Changed, uv, Changed)
  UPDSORTC(dv, Attr, uv, Rights)
  UPDSORTC(dv, Name, uv, Owner)
  UPDSORTC(dv, Name, uv, Group)
  #define COLVIEWPROPS ((TCustomDirViewColProperties*)(((TCustomDirView*)(((TListColumns*)(ListColumn->Collection))->Owner()))->ColProperties))
  UPDEX(SortColumnAscendingAction, (ListColumn != NULL), SortColumnAscendingAction->Checked =
    (COLVIEWPROPS->SortColumn == ListColumn->Index) && COLVIEWPROPS->SortAscending, )
  UPDEX(SortColumnDescendingAction, (ListColumn != NULL), SortColumnDescendingAction->Checked =
    (COLVIEWPROPS->SortColumn == ListColumn->Index) && !COLVIEWPROPS->SortAscending, )
  #undef COLVIEWPROPS

  // SHOW/HIDE COLUMN
  UPDSHCOL(Local, dv, Name)
  UPDSHCOL(Local, dv, Ext)
  UPDSHCOL(Local, dv, Size)
  UPDSHCOL(Local, dv, Type)
  UPDSHCOL(Local, dv, Changed)
  UPDSHCOL(Local, dv, Attr)
  UPDSHCOL(Remote, uv, Name)
  UPDSHCOL(Remote, uv, Ext)
  UPDSHCOL(Remote, uv, Size)
  UPDSHCOL(Remote, uv, Changed)
  UPDSHCOL(Remote, uv, Rights)
  UPDSHCOL(Remote, uv, Owner)
  UPDSHCOL(Remote, uv, Group)
  UPDSHCOL(Remote, uv, LinkTarget)
  UPDSHCOL(Remote, uv, Type)
  UPD(HideColumnAction, (ListColumn != NULL))
  UPD(BestFitColumnAction, (ListColumn != NULL))

  // SESSION
  UPD(NewSessionAction, true)
  UPD(SiteManagerAction, true)
  UPD(DuplicateSessionAction, (ScpExplorer->Terminal != NULL))
  UPD(RenameSessionAction, (ScpExplorer->Terminal != NULL))
  UPD(CloseSessionAction2, (ScpExplorer->Terminal != NULL))
  UPDEX1(DisconnectSessionAction, HasTerminal, DisconnectSessionAction->Visible = (ScpExplorer->Terminal == NULL) || !ScpExplorer->Terminal->Disconnected)
  UPDEX1(ReconnectSessionAction, (ScpExplorer->Terminal != NULL) && ScpExplorer->Terminal->Disconnected, ReconnectSessionAction->Visible = ReconnectSessionAction->Enabled)
  UPD(SavedSessionsAction2, true)
  UPD(WorkspacesAction, StoredSessions->HasAnyWorkspace())
  UPD(OpenedSessionsAction, HasTerminal)
  UPD(SaveCurrentSessionAction2, HasTerminal)
  UPD(SaveWorkspaceAction, HasTerminal)

  // COMMAND
  UPD(CompareDirectoriesAction, HasTerminal)
  UPD(SynchronizeAction, HasTerminal)
  UPD(FullSynchronizeAction, HasTerminal)
  UPD(ConsoleAction, ScpExplorer->CanConsole())
  UPD(PuttyAction, HasTerminal && TTerminalManager::Instance()->CanOpenInPutty())
  UPD(SynchronizeBrowsingAction, HasTerminal)
  UPD(CloseApplicationAction, true)
  UPD(FileSystemInfoAction, HasTerminal)
  UPD(SessionGenerateUrlAction2, HasTerminal)
  UPD(ClearCachesAction, HasTerminal && !ScpExplorer->Terminal->AreCachesEmpty)
  UPD(NewFileAction, DirViewEnabled(osCurrent) && !WinConfiguration->DisableOpenEdit)
  UPD(EditorListCustomizeAction, true)
  UPD(ChangePasswordAction, ScpExplorer->CanChangePassword())
  UPD(PrivateKeyUploadAction, ScpExplorer->CanPrivateKeyUpload())

  // CUSTOM COMMANDS
  UPD(CustomCommandsFileAction, true)
  UPD(CustomCommandsNonFileAction, true)
  UPD(CustomCommandsEnterAction, true)
  UPD(CustomCommandsEnterFocusedAction, true)
  UPDFUNC(CustomCommandsLastAction, CustomCommandsLastUpdate(CustomCommandsLastAction))
  UPDFUNC(CustomCommandsLastFocusedAction, CustomCommandsLastUpdate(CustomCommandsLastFocusedAction))
  UPD(CustomCommandsCustomizeAction, true)

  // QUEUE
  UPDEX(QueueEnableAction, HasTerminal, Action->Checked = ScpExplorer->GetQueueEnabled(), )
  #define UPDQUEUE(OPERATION) UPD(Queue ## OPERATION ## Action, \
    ScpExplorer->AllowQueueOperation(qo ## OPERATION))
  UPDQUEUE(GoTo)
  UPDQUEUE(Preferences)
  UPDEX(QueueItemQueryAction, ScpExplorer->AllowQueueOperation(qoItemQuery),
    Action->Visible = true, Action->Visible = false)
  UPDEX(QueueItemErrorAction, ScpExplorer->AllowQueueOperation(qoItemError),
    Action->Visible = true, Action->Visible = false)
  UPDEX(QueueItemPromptAction, ScpExplorer->AllowQueueOperation(qoItemPrompt),
    Action->Visible = true, Action->Visible = false)
  UPDQUEUE(ItemDelete)
  UPDEX(QueueItemExecuteAction, ScpExplorer->AllowQueueOperation(qoItemExecute),
    Action->Visible = true, Action->Visible =
      !ScpExplorer->AllowQueueOperation(qoItemPause) &&
      !ScpExplorer->AllowQueueOperation(qoItemResume))
  UPDEX(QueueItemPauseAction, ScpExplorer->AllowQueueOperation(qoItemPause),
    Action->Visible = true, Action->Visible = false)
  UPDEX(QueueItemResumeAction, ScpExplorer->AllowQueueOperation(qoItemResume),
    Action->Visible = true, Action->Visible = false)
  UPDQUEUE(ItemUp)
  UPDQUEUE(ItemDown)
  UPDQUEUE(PauseAll)
  UPDQUEUE(ResumeAll)
  UPDQUEUE(DeleteAll)
  UPDQUEUE(DeleteAllDone)
  #undef UPDQUEUE
  UPDEX(QueueItemSpeedAction, ScpExplorer->AllowQueueOperation(qoItemSpeed, &Param),
    QueueItemSpeedAction->Text = SetSpeedLimit(reinterpret_cast<unsigned long>(Param)),
    QueueItemSpeedAction->Text = L"")
  UPDACT(QueueToggleShowAction,
    Action->Checked = ScpExplorer->ComponentVisible[fcQueueView])
  #define QUEUEACTION(SHOW) UPDACT(Queue ## SHOW ## Action, \
    Action->Checked = WinConfiguration->QueueView.Show == qv ## SHOW)
  QUEUEACTION(Show)
  QUEUEACTION(HideWhenEmpty)
  QUEUEACTION(Hide)
  #undef QUEUEACTION
  UPDEX1(QueueCycleOnceEmptyAction, ScpExplorer->AllowQueueOperation(qoOnceEmpty),
    QueueCycleOnceEmptyAction->ImageIndex = CurrentQueueOnceEmptyAction()->ImageIndex;
    QueueCycleOnceEmptyAction->Checked = !QueueIdleOnceEmptyAction->Checked)
  UPD(QueueIdleOnceEmptyAction, ScpExplorer->AllowQueueOperation(qoOnceEmpty))
  UPD(QueueDisconnectOnceEmptyAction2, ScpExplorer->AllowQueueOperation(qoOnceEmpty))
  UPD(QueueSuspendOnceEmptyAction2, ScpExplorer->AllowQueueOperation(qoOnceEmpty))
  UPD(QueueShutDownOnceEmptyAction2, ScpExplorer->AllowQueueOperation(qoOnceEmpty))
  UPDCOMP(CommanderPreferencesBand)
  UPDACT(QueueToolbarAction,
    Action->Enabled = ScpExplorer->ComponentVisible[fcQueueView];
    Action->Checked = ScpExplorer->ComponentVisible[fcQueueToolbar])
  UPDACT(QueueFileListAction,
    Action->Enabled = ScpExplorer->ComponentVisible[fcQueueView];
    Action->Checked = ScpExplorer->ComponentVisible[fcQueueFileList])
  ;
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::ExplorerActionsExecute(
  TBasicAction * BasicAction, bool & Handled)
{
  DebugAssert(ScpExplorer);
  TAction * Action = DebugNotNull(dynamic_cast<TAction *>(BasicAction));
  if (!ScpExplorer->AllowedAction(Action, aaExecute))
  {
    Handled = true;
    return;
  }
  ScpExplorer->BeforeAction();

  unsigned int ShortCutFlag = FLAGMASK((Action->ActionComponent == NULL), cocShortCutHint);

  {
    TAutoNestingCounter Counter(FBusy);
    // focused operation
    EXE(CurrentDeleteFocusedAction, ScpExplorer->ExecuteFileOperationCommand(foDelete, osCurrent, true))
    EXE(CurrentPropertiesFocusedAction, ScpExplorer->ExecuteFileOperationCommand(foSetProperties, osCurrent, true))
    EXE(CurrentEditFocusedAction, ScpExplorer->ExecuteFile(osCurrent, efDefaultEditor, NULL, true, true))
    EXE(CurrentSystemMenuFocusedAction, ScpExplorer->DisplaySystemContextMenu())
    EXE(CurrentEditWithFocusedAction, ScpExplorer->ExecuteCurrentFileWith(true))
    EXE(CurrentEditInternalFocusedAction, ScpExplorer->ExecuteFile(osCurrent, efInternalEditor, NULL, true, true))
    EXE(CurrentCopyToClipboardFocusedAction, ScpExplorer->CopyFilesToClipboard(osCurrent, true))
    // operation
    EXE(CurrentEditAction, ScpExplorer->ExecuteFile(osCurrent, efDefaultEditor, NULL, true, false))
    EXE(CurrentEditInternalAction, ScpExplorer->ExecuteFile(osCurrent, efInternalEditor, NULL, true, false))
    EXE(CurrentEditWithAction, ScpExplorer->ExecuteCurrentFileWith(false))
    EXE(CurrentOpenAction, ScpExplorer->ExecuteCurrentFile())
    EXE(CurrentAddEditLinkAction, ScpExplorer->AddEditLink(osCurrent, false))
    EXE(CurrentAddEditLinkContextAction, ScpExplorer->AddEditLink(osCurrent, false))
    EXE(NewLinkAction, ScpExplorer->AddEditLink(osCurrent, true))
    EXE(CurrentRenameAction, ScpExplorer->ExecuteFileOperationCommand(foRename, osCurrent, false))
    EXE(CurrentDeleteAction, ScpExplorer->ExecuteFileOperationCommand(foDelete, osCurrent, false))
    EXE(CurrentDeleteAlternativeAction, ScpExplorer->ExecuteFileOperationCommand(foDelete, osCurrent, false, false, (void*)true))
    EXE(CurrentPropertiesAction, ScpExplorer->ExecuteFileOperationCommand(foSetProperties, osCurrent, false))
    EXE(CurrentCopyToClipboardAction, ScpExplorer->CopyFilesToClipboard(osCurrent, false))
    EXE(FileListToCommandLineAction, ScpExplorer->PanelExport(osCurrent, peFileList, pedCommandLine))
    EXE(FileListToClipboardAction, ScpExplorer->PanelExport(osCurrent, peFileList, pedClipboard))
    EXE(FullFileListToClipboardAction, ScpExplorer->PanelExport(osCurrent, peFullFileList, pedClipboard))
    EXE(FileGenerateUrlAction2, ScpExplorer->FileGenerateUrl())
    EXE(FileListFromClipboardAction, ScpExplorer->FileListFromClipboard())
    EXE(LockAction, ScpExplorer->ExecuteFileOperationCommand(foLock, osCurrent, false))
    EXE(UnlockAction, ScpExplorer->ExecuteFileOperationCommand(foUnlock, osCurrent, false))
    // local selected operation
    EXE(LocalCopyAction, ScpExplorer->ExecuteCopyOperationCommand(osLocal, false, ShortCutFlag))
    EXE(LocalCopyQueueAction, ScpExplorer->ExecuteCopyOperationCommand(osLocal, false, cocQueue))
    EXE(LocalCopyNonQueueAction, ScpExplorer->ExecuteCopyOperationCommand(osLocal, false, cocNonQueue))
    EXE(LocalRenameAction, ScpExplorer->ExecuteFileOperationCommand(foRename, osLocal, false))
    EXE(LocalEditAction, ScpExplorer->ExecuteFile(osLocal, efDefaultEditor, NULL, true, false))
    EXE(LocalMoveAction, ScpExplorer->ExecuteFileOperationCommand(foMove, osLocal, false))
    EXE(LocalCreateDirAction2, ScpExplorer->CreateDirectory(osLocal))
    EXE(LocalDeleteAction, ScpExplorer->ExecuteFileOperationCommand(foDelete, osLocal, false))
    EXE(LocalPropertiesAction, ScpExplorer->ExecuteFileOperationCommand(foSetProperties, osLocal, false))
    EXE(LocalAddEditLinkAction2, ScpExplorer->AddEditLink(osLocal, false))
    EXE(LocalNewFileAction, ScpExplorer->EditNew(osLocal))
    // local focused operation
    EXE(LocalCopyFocusedAction, ScpExplorer->ExecuteCopyOperationCommand(osLocal, true, ShortCutFlag))
    EXE(LocalCopyFocusedQueueAction, ScpExplorer->ExecuteCopyOperationCommand(osLocal, true, cocQueue))
    EXE(LocalCopyFocusedNonQueueAction, ScpExplorer->ExecuteCopyOperationCommand(osLocal, true, cocNonQueue))
    EXE(LocalMoveFocusedAction, ScpExplorer->ExecuteFileOperationCommand(foMove, osLocal, true))
    // remote selected operation
    EXE(RemoteCopyAction, ScpExplorer->ExecuteCopyOperationCommand(osRemote, false, ShortCutFlag))
    EXE(RemoteCopyQueueAction, ScpExplorer->ExecuteCopyOperationCommand(osRemote, false, cocQueue))
    EXE(RemoteCopyNonQueueAction, ScpExplorer->ExecuteCopyOperationCommand(osRemote, false, cocNonQueue))
    EXE(RemoteRenameAction, ScpExplorer->ExecuteFileOperationCommand(foRename, osRemote, false))
    EXE(RemoteEditAction, ScpExplorer->ExecuteFile(osRemote, efDefaultEditor, NULL, true, false))
    EXE(RemoteMoveAction, ScpExplorer->ExecuteFileOperationCommand(foMove, osRemote, false))
    EXE(RemoteCreateDirAction2, ScpExplorer->CreateDirectory(osRemote))
    EXE(RemoteNewFileAction, ScpExplorer->EditNew(osRemote))
    EXE(RemoteDeleteAction, ScpExplorer->ExecuteFileOperationCommand(foDelete, osRemote, false))
    EXE(RemotePropertiesAction, ScpExplorer->ExecuteFileOperationCommand(foSetProperties, osRemote, false))
    EXE(RemoteMoveToAction, ScpExplorer->ExecuteFileOperationCommand(foRemoteMove, osCurrent, false))
    EXE(RemoteCopyToAction, ScpExplorer->ExecuteFileOperationCommand(foRemoteCopy, osCurrent, false))
    EXE(RemoteAddEditLinkAction2, ScpExplorer->AddEditLink(osRemote, false))
    // remote focused operation
    EXE(RemoteCopyFocusedAction, ScpExplorer->ExecuteCopyOperationCommand(osRemote, true, ShortCutFlag))
    EXE(RemoteCopyFocusedQueueAction, ScpExplorer->ExecuteCopyOperationCommand(osRemote, true, cocQueue))
    EXE(RemoteCopyFocusedNonQueueAction, ScpExplorer->ExecuteCopyOperationCommand(osRemote, true, cocNonQueue))
    EXE(RemoteMoveFocusedAction, ScpExplorer->ExecuteFileOperationCommand(foMove, osRemote, true))
    EXE(RemoteMoveToFocusedAction, ScpExplorer->ExecuteFileOperationCommand(foRemoteMove, osCurrent, true))
    EXE(RemoteCopyToFocusedAction, ScpExplorer->ExecuteFileOperationCommand(foRemoteCopy, osCurrent, true))
    // directory
    EXE(CurrentCreateDirAction, ScpExplorer->CreateDirectory(osCurrent))
    EXE(NewDirAction, ScpExplorer->CreateDirectory(osCurrent))
    EXE(RemoteFindFilesAction, ScpExplorer->RemoteFindFiles())
    //selection
    EXE(SelectOneAction, DirView(osCurrent)->SelectCurrentItem(DirView(osCurrent)->NortonLike))
    EXE(SelectAction, ScpExplorer->SelectByMask(osCurrent, true))
    EXE(UnselectAction, ScpExplorer->SelectByMask(osCurrent, false))
    EXE(SelectAllAction, ScpExplorer->SelectAll(osCurrent, smAll))
    EXE(InvertSelectionAction, ScpExplorer->SelectAll(osCurrent, smInvert))
    EXE(ClearSelectionAction, ScpExplorer->SelectAll(osCurrent, smNone))
    EXE(RestoreSelectionAction, ScpExplorer->RestoreSelectedNames(osCurrent))
    EXE(SelectSameExtAction, ScpExplorer->SelectSameExt(true))
    EXE(UnselectSameExtAction, ScpExplorer->SelectSameExt(false))
    EXE(LocalSelectAction, ScpExplorer->SelectByMask(osLocal, true))
    EXE(LocalUnselectAction, ScpExplorer->SelectByMask(osLocal, false))
    EXE(LocalSelectAllAction, ScpExplorer->SelectAll(osLocal, smAll))
    EXE(RemoteSelectAction, ScpExplorer->SelectByMask(osRemote, true))
    EXE(RemoteUnselectAction, ScpExplorer->SelectByMask(osRemote, false))
    EXE(RemoteSelectAllAction, ScpExplorer->SelectAll(osRemote, smAll))
    EXE(PasteAction2, ScpExplorer->PasteFromClipBoard())

    // style
    EXE(CurrentCycleStyleAction,
      if (DirView(osCurrent)->ViewStyle == vsReport) DirView(osCurrent)->ViewStyle = vsIcon;
        else DirView(osCurrent)->ViewStyle = (TViewStyle)(DirView(osCurrent)->ViewStyle + 1);
    )
    #define STYLEACTION(Style) EXE(Current ## Style ## Action, \
      DirView(osCurrent)->ViewStyle = vs ## Style)
    STYLEACTION(Icon)
    STYLEACTION(SmallIcon)
    STYLEACTION(List)
    STYLEACTION(Report)
    #undef STYLEACTION

    #define PANEL_ACTIONS(SIDE) \
      EXE(SIDE ## BackAction, ScpExplorer->HistoryGo(os ## SIDE, -1)) \
      EXE(SIDE ## ForwardAction, ScpExplorer->HistoryGo(os ## SIDE, 1)) \
      EXE(SIDE ## ParentDirAction, DirView(os ## SIDE)->ExecuteParentDirectory()) \
      EXE(SIDE ## RootDirAction, DirView(os ## SIDE)->ExecuteRootDirectory()) \
      EXE(SIDE ## HomeDirAction, ScpExplorer->HomeDirectory(os ## SIDE)) \
      EXE(SIDE ## RefreshAction, ScpExplorer->ReloadDirectory(os ## SIDE)) \
      EXE(SIDE ## OpenDirAction, ScpExplorer->OpenDirectory(os ## SIDE)) \
      EXE(SIDE ## ChangePathAction, ScpExplorer->ChangePath(os ## SIDE)) \
      EXE(SIDE ## AddBookmarkAction, ScpExplorer->AddBookmark(os ## SIDE)) \
      EXE(SIDE ## PathToClipboardAction, ScpExplorer->PanelExport(os ## SIDE, pePath, pedClipboard)) \
      EXE(SIDE ## FilterAction, ScpExplorer->Filter(os ## SIDE))
    PANEL_ACTIONS(Local)
    PANEL_ACTIONS(Remote)
    #undef PANEL_ACTIONS
    EXE(LocalExploreDirectoryAction, ScpExplorer->ExploreLocalDirectory())

    //HELP
    EXE(AboutAction, DoAboutDialog(Configuration))
    EXE(HomepageAction, OpenBrowser(LoadStr(HOMEPAGE_URL)))
    EXE(HistoryPageAction, OpenBrowser(LoadStr(HISTORY_URL)))
    EXE(TableOfContentsAction, Application->HelpSystem->ShowTableOfContents())
    EXE(ForumPageAction, OpenBrowser(LoadStr(FORUM_URL)))
    EXE(CheckForUpdatesAction, CheckForUpdates(false))
    EXE(UpdatesPreferencesAction, PreferencesDialog(pmUpdates))
    EXE(DonatePageAction, OpenBrowser(LoadStr(DONATE_URL)))
    EXE(DownloadPageAction, OpenBrowser(LoadStr(DOWNLOAD_URL)))
    EXE(TipsAction, ShowTips())

    // VIEW
    EXECOMP(SessionsTabs)
    EXECOMP(StatusBar)
    EXECOMP(ToolBar2)
    EXECOMP(LocalStatusBar)
    EXECOMP(RemoteStatusBar)
    #define EMIT_BAND_COMPONENT(COMP) EXECOMP(COMP)
    BAND_COMPONENTS
    #undef EMIT_BAND_COMPONENT
    EXECOMP(CommandLinePanel)
    EXECOMP(RemoteTree)
    EXECOMP(LocalTree)
    EXE(GoToCommandLineAction, ScpExplorer->GoToCommandLine())
    EXE(GoToTreeAction, ScpExplorer->GoToTree())

    EXE(ShowHiddenFilesAction, ScpExplorer->ToggleShowHiddenFiles())
    EXE(FormatSizeBytesNoneAction, ScpExplorer->SetFormatSizeBytes(fbNone))
    EXE(FormatSizeBytesKilobytesAction, ScpExplorer->SetFormatSizeBytes(fbKilobytes))
    EXE(FormatSizeBytesShortAction, ScpExplorer->SetFormatSizeBytes(fbShort))
    EXE(AutoReadDirectoryAfterOpAction, ScpExplorer->ToggleAutoReadDirectoryAfterOp())
    EXE(PreferencesAction, PreferencesDialog(::pmDefault) )
    EXE(PresetsPreferencesAction, PreferencesDialog(pmPresets) )
    EXE(FileColorsPreferencesAction, PreferencesDialog(pmFileColors) )
    EXE(LockToolbarsAction, WinConfiguration->LockToolbars = !WinConfiguration->LockToolbars)
    EXE(SelectiveToolbarTextAction, WinConfiguration->SelectiveToolbarText = !WinConfiguration->SelectiveToolbarText)
    EXECOMP(CustomCommandsBand)
    EXE(ColorMenuAction, CreateSessionColorMenu(ColorMenuAction))
    EXE(GoToAddressAction, ScpExplorer->GoToAddress())
    EXE(CustomizeToolbarAction, CreateToolbarButtonsList())

    #define COLVIEWPROPS ((TCustomDirViewColProperties*)(((TCustomDirView*)(((TListColumns*)(ListColumn->Collection))->Owner()))->ColProperties))
    // SORT
    EXESORTA(Local)
    EXESORT(Local, dv, Name)
    EXESORT(Local, dv, Ext)
    EXESORT(Local, dv, Size)
    EXESORT(Local, dv, Type)
    EXESORT(Local, dv, Changed)
    EXESORT(Local, dv, Attr)
    EXESORTA(Remote)
    EXESORT(Remote, uv, Name)
    EXESORT(Remote, uv, Ext)
    EXESORT(Remote, uv, Size)
    EXESORT(Remote, uv, Changed)
    EXESORT(Remote, uv, Rights)
    EXESORT(Remote, uv, Owner)
    EXESORT(Remote, uv, Group)
    EXESORT(Remote, uv, Type)
    EXESORTA(Current)
    EXESORTC(Name, dvName, uvName)
    EXESORTC(Ext, dvExt, uvExt)
    EXESORTC(Size, dvSize, uvSize)
    EXESORTC(Type, dvType, uvType)
    EXESORTC(Changed, dvChanged, uvChanged)
    EXESORTC(Rights, dvAttr, uvRights)
    EXESORTC(Owner, dvName, uvOwner)
    EXESORTC(Group, dvName, uvGroup)
    EXE(SortColumnAscendingAction, DebugAssert(ListColumn);
      COLVIEWPROPS->SortColumn = ListColumn->Index; COLVIEWPROPS->SortAscending = true; ListColumn = NULL )
    EXE(SortColumnDescendingAction, DebugAssert(ListColumn);
      COLVIEWPROPS->SortColumn = ListColumn->Index; COLVIEWPROPS->SortAscending = false; ListColumn = NULL )

    // SHOW/HIDE COLUMN
    EXESHCOL(Local, dv, Name)
    EXESHCOL(Local, dv, Ext)
    EXESHCOL(Local, dv, Size)
    EXESHCOL(Local, dv, Type)
    EXESHCOL(Local, dv, Changed)
    EXESHCOL(Local, dv, Attr)
    EXESHCOL(Remote, uv, Name)
    EXESHCOL(Remote, uv, Ext)
    EXESHCOL(Remote, uv, Size)
    EXESHCOL(Remote, uv, Changed)
    EXESHCOL(Remote, uv, Rights)
    EXESHCOL(Remote, uv, Owner)
    EXESHCOL(Remote, uv, Group)
    EXESHCOL(Remote, uv, LinkTarget)
    EXESHCOL(Remote, uv, Type)
    EXE(HideColumnAction, DebugAssert(ListColumn);
      COLVIEWPROPS->Visible[ListColumn->Index] = false; ListColumn = NULL )
    EXE(BestFitColumnAction, DebugAssert(ListColumn); ListColumn = NULL ) // TODO
    #undef COLVIEWPROPS

    // SESSION
    EXE(NewSessionAction, ScpExplorer->NewSession())
    EXE(SiteManagerAction, ScpExplorer->NewSession())
    EXE(DuplicateSessionAction, ScpExplorer->DuplicateSession())
    EXE(RenameSessionAction, ScpExplorer->RenameSession())
    EXE(CloseSessionAction2, ScpExplorer->CloseSession())
    EXE(DisconnectSessionAction, ScpExplorer->DisconnectSession())
    EXE(ReconnectSessionAction, ScpExplorer->ReconnectSession())
    EXE(SavedSessionsAction2, CreateSessionListMenu(SavedSessionsAction2))
    EXE(WorkspacesAction, CreateWorkspacesMenu(WorkspacesAction))
    EXE(OpenedSessionsAction, CreateOpenedSessionListMenu(OpenedSessionsAction))
    EXE(SaveCurrentSessionAction2, ScpExplorer->SaveCurrentSession())
    EXE(SaveWorkspaceAction, ScpExplorer->SaveWorkspace(false))

    // COMMAND
    EXE(CompareDirectoriesAction, ScpExplorer->CompareDirectories())
    EXE(SynchronizeAction, ScpExplorer->SynchronizeDirectories())
    EXE(FullSynchronizeAction, ScpExplorer->FullSynchronizeDirectories())
    EXE(ConsoleAction, ScpExplorer->OpenConsole())
    EXE(PuttyAction, TTerminalManager::Instance()->OpenInPutty())
    EXE(SynchronizeBrowsingAction, ScpExplorer->SynchronizeBrowsingChanged())
    EXE(CloseApplicationAction, ScpExplorer->CloseApp())
    EXE(FileSystemInfoAction, ScpExplorer->FileSystemInfo())
    EXE(SessionGenerateUrlAction2, ScpExplorer->SessionGenerateUrl())
    EXE(ClearCachesAction, ScpExplorer->Terminal->ClearCaches())
    EXE(NewFileAction, ScpExplorer->EditNew(osCurrent))
    EXE(EditorListCustomizeAction, PreferencesDialog(pmEditor))
    EXE(ChangePasswordAction, ScpExplorer->ChangePassword())
    EXE(PrivateKeyUploadAction, ScpExplorer->PrivateKeyUpload())

    // CUSTOM COMMANDS
    EXE(CustomCommandsFileAction, CreateCustomCommandsMenu(CustomCommandsFileAction, ccltFile))
    EXE(CustomCommandsNonFileAction, CreateCustomCommandsMenu(CustomCommandsNonFileAction, ccltNonFile))
    EXE(CustomCommandsEnterAction, ScpExplorer->AdHocCustomCommand(false))
    EXE(CustomCommandsEnterFocusedAction, ScpExplorer->AdHocCustomCommand(true))
    EXE(CustomCommandsLastAction, ScpExplorer->LastCustomCommand(false))
    EXE(CustomCommandsLastFocusedAction, ScpExplorer->LastCustomCommand(true))
    EXE(CustomCommandsCustomizeAction, CustomCommandsCustomize(NULL))

    // QUEUE
    EXE(QueueEnableAction, ScpExplorer->ToggleQueueEnabled())
    #define EXEQUEUE(OPERATION) EXE(Queue ## OPERATION ## Action, \
      ScpExplorer->ExecuteQueueOperation(qo ## OPERATION))
    EXEQUEUE(GoTo)
    EXEQUEUE(Preferences)
    EXEQUEUE(ItemQuery)
    EXEQUEUE(ItemError)
    EXEQUEUE(ItemPrompt)
    EXEQUEUE(ItemDelete)
    EXEQUEUE(ItemExecute)
    EXEQUEUE(ItemPause)
    EXEQUEUE(ItemResume)
    EXEQUEUE(ItemUp)
    EXEQUEUE(ItemDown)
    EXEQUEUE(PauseAll)
    EXEQUEUE(ResumeAll)
    EXEQUEUE(DeleteAll)
    EXEQUEUE(DeleteAllDone)
    #undef EXEQUEUE
    EXE(QueueToggleShowAction, ScpExplorer->ToggleQueueVisibility())
    #define QUEUEACTION(SHOW) EXE(Queue ## SHOW ## Action, \
      TQueueViewConfiguration Config = WinConfiguration->QueueView; \
      if (Config.Show != qvShow) Config.LastHideShow = Config.Show; \
      Config.Show = qv ## SHOW; \
      WinConfiguration->QueueView = Config)
    QUEUEACTION(Show)
    QUEUEACTION(HideWhenEmpty)
    QUEUEACTION(Hide)
    #undef QUEUEACTION
    EXE(QueueCycleOnceEmptyAction, CycleQueueOnceEmptyAction())
    EXE(QueueIdleOnceEmptyAction, SetQueueOnceEmptyAction(QueueIdleOnceEmptyAction))
    EXE(QueueDisconnectOnceEmptyAction2, SetQueueOnceEmptyAction(QueueDisconnectOnceEmptyAction2))
    EXE(QueueSuspendOnceEmptyAction2, SetQueueOnceEmptyAction(QueueSuspendOnceEmptyAction2))
    EXE(QueueShutDownOnceEmptyAction2, SetQueueOnceEmptyAction(QueueShutDownOnceEmptyAction2))
    EXECOMP(QueueToolbar)
    EXECOMP(QueueFileList)
    EXE(QueueItemSpeedAction, )
    ;
  }

  DoIdle();
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::UpdateNonVisibleActions()
{
  // following actions needs to be updated even when all clients
  // are invisible, so the queue list toolbar button can be shown
  NonVisualDataModule->QueueItemQueryAction->Update();
  NonVisualDataModule->QueueItemErrorAction->Update();
  NonVisualDataModule->QueueItemPromptAction->Update();
}
//---------------------------------------------------------------------------
#define CTRL TShiftState() << ssCtrl
#define ALT TShiftState() << ssAlt
#define SHIFT TShiftState() << ssShift
#define CTRLSHIFT TShiftState() << ssCtrl << ssShift
#define CTRLALT TShiftState() << ssCtrl << ssAlt
#define NONE TShiftState()
void __fastcall TNonVisualDataModule::ExplorerShortcuts()
{
  // Directory
  CurrentCreateDirAction->ShortCut = ShortCut(L'D', CTRL);
  // File operation
  CurrentRenameAction->ShortCut = ShortCut(VK_F2, NONE);
  CurrentEditAction->ShortCut = ShortCut(L'E', CTRL);
  CurrentAddEditLinkAction->ShortCut = ShortCut(L'L', CTRLALT);
  CurrentEditInternalAction->ShortCut = 0;
  CurrentEditInternalFocusedAction->ShortCut = 0;
  // Focused operation
  RemoteCopyAction->ShortCut = ShortCut(L'T', CTRL);
  RemoteMoveAction->ShortCut = ShortCut(L'M', CTRL);
  CurrentDeleteFocusedAction->ShortCut = ShortCut(VK_DELETE, NONE);
  CurrentPropertiesFocusedAction->ShortCut = ShortCut(VK_RETURN, ALT);
  RemoteMoveToFocusedAction->ShortCut = ShortCut(L'M', CTRLALT);
  // remote directory
  RemoteOpenDirAction->ShortCut = ShortCut(L'O', CTRL);
  RemoteRefreshAction->ShortCut = ShortCut(VK_F5, NONE);
  RemoteHomeDirAction->ShortCut = ShortCut(L'H', CTRL);
  RemotePathToClipboardAction->ShortCut = ShortCut(L'P', CTRLSHIFT);
  // selected operation
  CurrentDeleteAlternativeAction->ShortCut = ShortCut(VK_DELETE, SHIFT);
  RemoteMoveToAction->ShortCut = ShortCut(L'M', CTRLALT);
  // commands
  NewFileAction->ShortCut = ShortCut(L'E', CTRLSHIFT);
  RemoteFindFilesAction->ShortCut = ShortCut(VK_F3, NONE);

  CloseApplicationAction->ShortCut = ShortCut(VK_F4, ALT);

  CloneShortcuts();
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::CommanderShortcuts()
{
  bool ExplorerKeyboardShortcuts = WinConfiguration->ScpCommander.ExplorerKeyboardShortcuts;
  // Directory
  CurrentCreateDirAction->ShortCut = ShortCut(VK_F7, NONE);
  // File operation
  CurrentRenameAction->ShortCut = ShortCut(VK_F2, NONE);
  CurrentEditAction->ShortCut = ShortCut(VK_F4, NONE);
  CurrentAddEditLinkAction->ShortCut = ShortCut(VK_F6, ALT);
  CurrentEditInternalAction->ShortCut = ShortCut(VK_F4, CTRLALT);
  CurrentEditInternalFocusedAction->ShortCut = ShortCut(VK_F4, CTRLALT);
  // Focused operation
  RemoteCopyAction->ShortCut =
    ExplorerKeyboardShortcuts ? ShortCut(L'K', CTRL) : ShortCut(VK_F5, NONE);
  RemoteMoveAction->ShortCut = ShortCut(VK_F6, NONE);
  CurrentDeleteFocusedAction->ShortCut = ShortCut(VK_F8, NONE);
  CurrentPropertiesFocusedAction->ShortCut = ShortCut(VK_F9, NONE);
  RemoteMoveToFocusedAction->ShortCut = ShortCut(VK_F6, SHIFT);
  RemoteCopyToFocusedAction->ShortCut = ShortCut(VK_F5, SHIFT);
  // remote directory
  RemoteOpenDirAction->ShortCut = ShortCut(L'O', CTRL);
  RemoteRefreshAction->ShortCut =
    ExplorerKeyboardShortcuts ? ShortCut(VK_F5, NONE) : ShortCut(L'R', CTRL);
  RemoteHomeDirAction->ShortCut = ShortCut(L'H', CTRL);
  RemotePathToClipboardAction->ShortCut = ShortCut(VK_OEM_6 /* ] */, CTRL);
  // local directory
  LocalPathToClipboardAction->ShortCut = ShortCut(VK_OEM_4 /* [ */, CTRL);
  // selected operation
  CurrentDeleteAction->SecondaryShortCuts->Clear();
  CurrentDeleteAction->SecondaryShortCuts->Add(ShortCutToText(ShortCut(VK_DELETE, NONE)));
  CurrentDeleteAlternativeAction->ShortCut = ShortCut(VK_F8, SHIFT);
  CurrentDeleteAlternativeAction->SecondaryShortCuts->Clear();
  CurrentDeleteAlternativeAction->SecondaryShortCuts->Add(ShortCutToText(ShortCut(VK_DELETE, SHIFT)));
  RemoteMoveToAction->ShortCut = ShortCut(VK_F6, SHIFT);
  RemoteCopyToAction->ShortCut = ShortCut(VK_F5, SHIFT);
  // selection
  SelectOneAction->ShortCut = VK_INSERT;
  // commands
  NewFileAction->ShortCut = ShortCut(VK_F4, SHIFT);
  RemoteFindFilesAction->ShortCut =
    ExplorerKeyboardShortcuts ? ShortCut(VK_F3, NONE) : ShortCut(VK_F7, ALT);
  // legacy shortcut (can be removed when necessary)
  NewFileAction->SecondaryShortCuts->Clear();
  NewFileAction->SecondaryShortCuts->Add(ShortCutToText(ShortCut(VK_F4, CTRLSHIFT)));
  // Backward compatibility, can be abandoned, once there's a better use for Ctrl+T
  ConsoleAction->SecondaryShortCuts->Clear();
  ConsoleAction->SecondaryShortCuts->Add(ShortCutToText(ShortCut(L'T', CTRL)));

  CloseApplicationAction->ShortCut = ShortCut(VK_F10, NONE);

  TShortCut CtrlF4 = ShortCut(VK_F4, CTRL);
  LocalSortByExtAction->ShortCut = ExplorerKeyboardShortcuts ? TShortCut(0) : CtrlF4;
  RemoteSortByExtAction->ShortCut = LocalSortByExtAction->ShortCut;
  CurrentSortByExtAction->ShortCut = LocalSortByExtAction->ShortCut;
  int Index = CloseSessionAction2->SecondaryShortCuts->IndexOfShortCut(CtrlF4);
  if (ExplorerKeyboardShortcuts && (Index < 0))
  {
    CloseSessionAction2->SecondaryShortCuts->Add(ShortCutToText(CtrlF4));
  }
  else if (!ExplorerKeyboardShortcuts && (Index >= 0))
  {
    CloseSessionAction2->SecondaryShortCuts->Delete(Index);
  }

  CloneShortcuts();
}
#undef CTRL
#undef ALT
#undef NONE
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::CloneShortcuts()
{
  // Commands
  NewDirAction->ShortCut = CurrentCreateDirAction->ShortCut;
  // File operation
  CurrentAddEditLinkContextAction->ShortCut = CurrentAddEditLinkAction->ShortCut;
  LocalAddEditLinkAction2->ShortCut = CurrentAddEditLinkAction->ShortCut;
  RemoteAddEditLinkAction2->ShortCut = CurrentAddEditLinkAction->ShortCut;
  RemoteNewFileAction->ShortCut = NewFileAction->ShortCut;
  LocalNewFileAction->ShortCut = NewFileAction->ShortCut;
  // local directory
  LocalOpenDirAction->ShortCut = RemoteOpenDirAction->ShortCut;
  LocalRefreshAction->ShortCut = RemoteRefreshAction->ShortCut;
  LocalHomeDirAction->ShortCut = RemoteHomeDirAction->ShortCut;
  // selected operation
  CurrentDeleteAction->ShortCut = CurrentDeleteFocusedAction->ShortCut;
  CurrentPropertiesAction->ShortCut = CurrentPropertiesFocusedAction->ShortCut;
  // local selected operation
  LocalCopyAction->ShortCut = RemoteCopyAction->ShortCut;
  LocalRenameAction->ShortCut = CurrentRenameAction->ShortCut;
  LocalEditAction->ShortCut = CurrentEditAction->ShortCut;
  LocalMoveAction->ShortCut = RemoteMoveAction->ShortCut;
  LocalCreateDirAction2->ShortCut = CurrentCreateDirAction->ShortCut;
  LocalDeleteAction->ShortCut = CurrentDeleteAction->ShortCut;
  LocalPropertiesAction->ShortCut = CurrentPropertiesAction->ShortCut;
  // local focused operation
  LocalCopyFocusedAction->ShortCut = LocalCopyAction->ShortCut;
  LocalMoveFocusedAction->ShortCut = LocalMoveAction->ShortCut;
  // remote selected operation
  RemoteRenameAction->ShortCut = CurrentRenameAction->ShortCut;
  RemoteEditAction->ShortCut = CurrentEditAction->ShortCut;
  RemoteCreateDirAction2->ShortCut = CurrentCreateDirAction->ShortCut;
  RemoteDeleteAction->ShortCut = CurrentDeleteAction->ShortCut;
  RemotePropertiesAction->ShortCut = CurrentPropertiesAction->ShortCut;
  // remote focused operation
  RemoteCopyFocusedAction->ShortCut = RemoteCopyAction->ShortCut;
  RemoteMoveFocusedAction->ShortCut = RemoteMoveAction->ShortCut;
  // selection
  LocalSelectAction->ShortCut = SelectAction->ShortCut;
  LocalUnselectAction->ShortCut = UnselectAction->ShortCut;
  LocalSelectAllAction->ShortCut = SelectAllAction->ShortCut;
  RemoteSelectAction->ShortCut = SelectAction->ShortCut;
  RemoteUnselectAction->ShortCut = UnselectAction->ShortCut;
  RemoteSelectAllAction->ShortCut = SelectAllAction->ShortCut;
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::SetScpExplorer(TCustomScpExplorerForm * value)
{
  FScpExplorer = value;
  SessionIdleTimer->Enabled = (FScpExplorer != NULL);
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::SessionIdleTimerTimer(
      TObject */*Sender*/)
{
  DoIdle();
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::DoIdle()
{
  if (!FSessionIdleTimerExecuting)
  {
    FSessionIdleTimerExecuting = true;
    try
    {
      DebugAssert(ScpExplorer);
      ScpExplorer->Idle();
    }
    __finally
    {
      FSessionIdleTimerExecuting = false;
    }
  }
}
//---------------------------------------------------------------------------
UnicodeString __fastcall TNonVisualDataModule::CustomCommandCaption(const TCustomCommandType * Command, bool Toolbar)
{
  UnicodeString Result = Command->Name;
  if (Toolbar)
  {
    Result = StripHotkey(Result);
    Result = EscapeHotkey(Result);
  }
  return Result;
}
//---------------------------------------------------------------------------
UnicodeString __fastcall TNonVisualDataModule::CustomCommandHint(const TCustomCommandType * Command)
{
  UnicodeString Name = StripHotkey(Command->Name);
  UnicodeString LongHint =
    !Command->Description.IsEmpty() ? Command->Description : FMTLOAD(CUSTOM_COMMAND_HINT_LONG, (Name, Command->Command));
  UnicodeString Result = FORMAT(L"%s|%s", (Name, LongHint));
  return Result;
}
//---------------------------------------------------------------------------
const int CustomCommandOnFocused = 0x0100;
const int CustomCommandBoth = 0x0200;
const int CustomCommandExtension = 0x0400;
const int CustomCommandIndexMask = 0x00FF;
//---------------------------------------------------------------------------
// See IsValidIdent
static UnicodeString MakeIdent(const UnicodeString & S)
{
  UnicodeString Result;
  for (int Index = 1; Index <= S.Length(); Index++)
  {
    wchar_t C = S[Index];
    if (IsLetter(C) ||
        (!Result.IsEmpty() && IsDigit(C)))
    {
      Result += C;
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
int __fastcall TNonVisualDataModule::CreateCustomCommandsListMenu(
  TCustomCommandList * List, TTBCustomItem * Menu, bool OnFocused, bool Toolbar, TCustomCommandListType ListType,
  int Tag, TStrings * HiddenCommands)
{
  int Result = 0;

  for (int Index = 0; Index < List->Count; Index++)
  {
    const TCustomCommandType * Command = List->Commands[Index];
    int State = ScpExplorer->CustomCommandState(*Command, OnFocused, ListType);

    if (State >= 0)
    {
      TTBCustomItem * Item = new TTBXItem(Owner);
      Item->Caption = CustomCommandCaption(Command, Toolbar);
      Item->Tag = Index | Tag;
      Item->Enabled = (State > 0);
      if (Toolbar)
      {
        UnicodeString Name = ExtractFileName(Command->Id);
        if (Name.IsEmpty())
        {
          Name = Command->Name;
        }
        Name = MakeIdent(Name);
        Name += L"CustomCommand";
        // This is only the last resort to avoid run-time errors.
        // If there are duplicates, button hidding won't be deterministic.
        int X = 0;
        UnicodeString UniqueName = Name;
        while (Owner->FindComponent(UniqueName) != NULL)
        {
          X++;
          UniqueName = FORMAT(L"%s%d", (Name, X));
        }
        Item->Name = UniqueName;
        Item->Visible = (HiddenCommands->IndexOf(Item->Name) < 0);
      }
      if (OnFocused)
      {
        Item->Tag = Item->Tag | CustomCommandOnFocused;
      }
      if (ListType == ccltBoth)
      {
        Item->Tag = Item->Tag | CustomCommandBoth;
      }
      Item->Hint = CustomCommandHint(Command);
      Item->ShortCut = Command->ShortCut;
      Item->OnClick = CustomCommandClick;

      Menu->Add(Item);

      Result++;
    }
  }

  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::CustomCommandsCustomize(TObject *)
{
  PreferencesDialog(pmCustomCommands);
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::CreateCustomCommandsMenu(
  TTBCustomItem * Menu, bool OnFocused, bool Toolbar, TCustomCommandListType ListType, TStrings * HiddenCommands)
{
  CreateCustomCommandsListMenu(WinConfiguration->CustomCommandList, Menu, OnFocused, Toolbar, ListType, 0, HiddenCommands);

  TTBCustomItem * Item;

  if ((ListType == ccltAll) || (ListType == ccltFile) || (ListType == ccltNonFile))
  {
    Item = new TTBXItem(Menu);
    Item->Action = OnFocused ? CustomCommandsEnterFocusedAction : CustomCommandsEnterAction;
    Menu->Add(Item);
    GiveTBItemPriority(Item);

    Item = new TTBXItem(Menu);
    Item->Action = OnFocused ? CustomCommandsLastFocusedAction : CustomCommandsLastAction;
    if (Toolbar)
    {
      Item->Caption = EscapeHotkey(StripHotkey(LoadStr(CUSTOM_COMMAND_LAST_SHORT)));
    }
    Menu->Add(Item);
    GiveTBItemPriority(Item);
  }

  TTBXSeparatorItem * Separator = AddMenuSeparator(Menu);
  int ExtensionItems =
    CreateCustomCommandsListMenu(WinConfiguration->ExtensionList, Menu, OnFocused, Toolbar, ListType, CustomCommandExtension, HiddenCommands);
  Separator->Visible = (ExtensionItems > 0);

  AddMenuSeparator(Menu);

  if (((ListType == ccltFile) || ((ListType == ccltNonFile) && !OnFocused)) && DebugAlwaysTrue(!Toolbar))
  {
    Item = new TTBXSubmenuItem(Menu);
    // copy the texts from the action, but do not use is as a handler, because it will duplicate the auxiliary commands in the submenu
    Item->Action = (ListType == ccltFile) ? CustomCommandsNonFileAction : CustomCommandsFileAction;
    Item->Action = NULL;
    TCustomCommandListType SubListType = (ListType == ccltFile) ? ccltNonFile : ccltFile;
    int CustomCommandItems = CreateCustomCommandsListMenu(WinConfiguration->CustomCommandList, Item, OnFocused, Toolbar, SubListType, 0, HiddenCommands);
    TTBXSeparatorItem * Separator = AddMenuSeparator(Item);
    int ExtensionItems = CreateCustomCommandsListMenu(WinConfiguration->ExtensionList, Item, OnFocused, Toolbar, SubListType, CustomCommandExtension, HiddenCommands);
    Separator->Visible = (ExtensionItems > 0);
    Item->Enabled = (CustomCommandItems + ExtensionItems > 0);

    Menu->Add(Item);
  }

  if (!Toolbar && (ListType != ccltBoth))
  {
    Item = new TTBXItem(Menu);
    Item->Action = CustomCommandsBandAction;
    Menu->Add(Item);
    GiveTBItemPriority(Item);
  }

  Item = new TTBXItem(Menu);
  Item->Action = CustomCommandsCustomizeAction;
  if (ListType == ccltBoth)
  {
    // Hack. Bypass the Busy test in ExplorerActionsExecute.
    Item->OnClick = CustomCommandsCustomize;
  }
  Menu->Add(Item);
  GiveTBItemPriority(Item);
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::CreateCustomCommandsMenu(
  TAction * Action, bool OnFocused, TCustomCommandListType ListType)
{
  DebugAssert(Action);
  TTBCustomItem * Menu = dynamic_cast<TTBCustomItem *>(Action->ActionComponent);
  if (Menu)
  {
    int PrevCount = Menu->Count;

    CreateCustomCommandsMenu(Menu, OnFocused, false, ListType, NULL);

    for (int Index = 0; Index < PrevCount; Index++)
    {
      Menu->Delete(0);
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::CreateCustomCommandsMenu(TAction * Action, TCustomCommandListType ListType)
{
  TTBCustomItem * Menu = dynamic_cast<TTBCustomItem *>(Action->ActionComponent);
  if (DebugAlwaysTrue(Menu != NULL))
  {
    bool OnFocused =
      (Menu == RemoteDirViewPopupCustomCommandsMenu) || (Menu == LocalFilePopupCustomCommandsMenu) || (Menu == RemoteFilePopupCustomCommandsMenu);
    CreateCustomCommandsMenu(Action, OnFocused, ListType);
  }
}
//---------------------------------------------------------------------------
bool __fastcall TNonVisualDataModule::CheckCustomCommandsToolbarList(TTBXToolbar * Toolbar, TCustomCommandList * List, int & Index)
{
  bool Changed = false;
  int CommandIndex = 0;
  while (!Changed && (CommandIndex < List->Count))
  {
    TTBCustomItem * Item = Toolbar->Items->Items[Index];
    const TCustomCommandType * Command = List->Commands[CommandIndex];

    UnicodeString Caption = CustomCommandCaption(Command, true);
    UnicodeString Hint = CustomCommandHint(Command);
    Changed =
      (Item->Caption != Caption) ||
      (Item->Hint != Hint);

    Index++;
    CommandIndex++;
  }
  return Changed;
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::UpdateCustomCommandsToolbarList(TTBXToolbar * Toolbar, TCustomCommandList * List, int & Index)
{
  for (int CommandIndex = 0; CommandIndex < List->Count; CommandIndex++, Index++)
  {
    TTBCustomItem * Item = Toolbar->Items->Items[Index];
    DebugAssert((Item->Tag & CustomCommandIndexMask) == CommandIndex);
    int State = ScpExplorer->CustomCommandState(*List->Commands[CommandIndex], false, ccltAll);
    DebugAssert(State >= 0);
    Item->Enabled = (State > 0);
  }
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::UpdateCustomCommandsToolbar(TTBXToolbar * Toolbar)
{
  TCustomCommandList * CustomCommandList = WinConfiguration->CustomCommandList;
  TCustomCommandList * ExtensionList = WinConfiguration->ExtensionList;
  int AfterCustomCommandsCommandCount = 2; // ad hoc, last
  int AdditionalCommands = AfterCustomCommandsCommandCount + 3; // custom/ext separator + separator, customize
  int CommandCount = CustomCommandList->Count + ExtensionList->Count;
  bool Changed = (CommandCount + AdditionalCommands != Toolbar->Items->Count);
  if (!Changed)
  {
    int Index = 0;
    Changed = CheckCustomCommandsToolbarList(Toolbar, CustomCommandList, Index);

    if (!Changed)
    {
      Index += AfterCustomCommandsCommandCount;
      Changed = (dynamic_cast<TTBXSeparatorItem *>(Toolbar->Items->Items[Index]) == NULL);
      Index++;

      if (!Changed)
      {
        Changed = CheckCustomCommandsToolbarList(Toolbar, ExtensionList, Index);
      }
    }
  }

  if (Changed)
  {
    Toolbar->BeginUpdate();
    try
    {
      std::unique_ptr<TStrings> HiddenCommands(CreateSortedStringList());
      for (int Index = 0; Index < Toolbar->Items->Count; Index++)
      {
        TTBCustomItem * Item = Toolbar->Items->Items[Index];
        if (IsCustomizableToolbarItem(Item) && !Item->Visible)
        {
          HiddenCommands->Add(Item->Name);
        }
      }
      Toolbar->Items->Clear();
      CreateCustomCommandsMenu(Toolbar->Items, false, true, ccltAll, HiddenCommands.get());
      DebugAssert(CommandCount + AdditionalCommands == Toolbar->Items->Count);
    }
    __finally
    {
      Toolbar->EndUpdate();
    }
  }
  else
  {
    int Index = 0;
    UpdateCustomCommandsToolbarList(Toolbar, CustomCommandList, Index);
    Index += AfterCustomCommandsCommandCount + 1;
    UpdateCustomCommandsToolbarList(Toolbar, ExtensionList, Index);
  }
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::CustomCommandClick(TObject * Sender)
{
  TTBCustomItem * Item = dynamic_cast<TTBCustomItem *>(Sender);
  DebugAssert(Item);
  const TCustomCommandList * List = FLAGSET(Item->Tag, CustomCommandExtension) ? WinConfiguration->ExtensionList : WinConfiguration->CustomCommandList;
  const TCustomCommandType * Command = List->Commands[Item->Tag & CustomCommandIndexMask];
  if (FLAGCLEAR(Item->Tag, CustomCommandBoth))
  {
    ScpExplorer->ExecuteFileOperationCommand(foCustomCommand, osRemote,
      FLAGSET(Item->Tag, CustomCommandOnFocused), false, const_cast<TCustomCommandType *>(Command));
  }
  else
  {
    ScpExplorer->BothCustomCommand(*Command);
  }
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::CreateSessionColorMenu(TAction * Action)
{
  if (DebugAlwaysTrue(Action->ActionComponent != NULL))
  {
    ::CreateSessionColorMenu(Action->ActionComponent, ScpExplorer->SessionColor,
      SessionColorChange);
  }
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::SessionColorChange(TColor Color)
{
  ScpExplorer->SessionColor = Color;
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::CreateSessionListMenu(TAction * Action)
{
  StoredSessions->Reload();
  CreateSessionListMenuLevel(
    dynamic_cast<TTBCustomItem *>(Action->ActionComponent), 0, 0);
}
//---------------------------------------------------------------------------
UnicodeString __fastcall TNonVisualDataModule::GetSessionFolderRoot(
  TSessionData * Data, int Level)
{
  UnicodeString Path = Data->Name;
  UnicodeString Root;
  for (int ALevel = 0; ALevel < Level; ALevel++)
  {
    Root.Insert(CutToChar(Path, L'/', false) + L'/', Root.Length() + 1);
  }
  return Root;
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::CreateSessionListMenuLevel(
  TTBCustomItem * Menu, int Index, int Level)
{
  Menu->Clear();

  TTBCustomItem * Item = new TTBXItem(Menu);

  UnicodeString Root;
  if (Level == 0)
  {
    Item->Action = SiteManagerAction;
    Root = L"";
  }
  else
  {
    DebugAssert(Index < StoredSessions->Count);
    TSessionData * Data = StoredSessions->Sessions[Index];

    Root = GetSessionFolderRoot(Data, Level);

    Item->Caption = LoadStr(SAVEDSESSIONFOLDER_THIS_OPEN);
    Item->Tag = MAKELONG(Index, Level);
    UnicodeString FolderName = Root;
    if (DebugAlwaysTrue(!FolderName.IsEmpty() && FolderName[FolderName.Length()] == L'/'))
    {
      FolderName.Delete(FolderName.Length(), 1);
    }
    Item->Hint = FMTLOAD(SAVEDSESSIONFOLDER_THIS_HINT, (FolderName));
    Item->OnClick = SessionFolderThisItemClick;
  }

  Menu->Insert(Menu->Count, Item);

  AddMenuSeparator(Menu);

  int FirstSession = Menu->Count;
  UnicodeString PrevName;
  while (Index < StoredSessions->Count)
  {
    TSessionData * Data = StoredSessions->Sessions[Index];
    if (!AnsiSameText(Data->Name.SubString(1, Root.Length()), Root))
    {
      // Sessions are sorted, so no chance further sessions may match
      break;
    }
    else if (!Data->IsWorkspace)
    {
      UnicodeString Name = Data->Name.SubString(Root.Length() + 1, Data->Name.Length() - Root.Length());
      int P = Name.Pos(L'/');
      if (P > 0)
      {
        Name.SetLength(P - 1);
      }

      if (Name != PrevName)
      {
        if (P > 0)
        {
          TTBCustomItem * Item = new TTBXSubmenuItem(Menu);
          Item->Caption = Name;
          Item->Tag = ((Level + 1) << 16) | Index; // MAKELONG
          Item->ImageIndex = SavedSessionsAction2->ImageIndex;
          Item->OnClick = SessionFolderItemClick;

          Menu->Insert(FirstSession, Item);
          FirstSession++;
        }
        else
        {
          TTBCustomItem * Item = new TTBXItem(Menu);
          Item->Caption = Name;
          Item->Tag = Index;
          Item->Hint = FMTLOAD(SAVEDSESSION_HINT, (Data->Name));
          Item->OnClick = SessionItemClick;
          Menu->Insert(Menu->Count, Item);
        }

        PrevName = Name;
      }
    }
    Index++;
  }
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::SessionFolderItemClick(TObject * Sender)
{
  TTBCustomItem * Item = dynamic_cast<TTBCustomItem *>(Sender);
  DebugAssert(Item != NULL);
  CreateSessionListMenuLevel(Item, LOWORD(Item->Tag), HIWORD(Item->Tag));
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::SessionFolderThisItemClick(TObject * Sender)
{
  TTBCustomItem * Item = DebugNotNull(dynamic_cast<TTBCustomItem *>(Sender));
  int Index = LOWORD(Item->Tag);
  int Level = HIWORD(Item->Tag);
  UnicodeString Folder = GetSessionFolderRoot(StoredSessions->Sessions[Index], Level);
  ScpExplorer->OpenFolderOrWorkspace(Folder);
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::SessionItemClick(TObject * Sender)
{
  DebugAssert(StoredSessions && (((TTBCustomItem *)Sender)->Tag < StoredSessions->Count));
  ScpExplorer->OpenStoredSession(StoredSessions->Sessions[((TTBCustomItem *)Sender)->Tag]);
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::CreateWorkspacesMenu(TAction * Action)
{
  StoredSessions->Reload();
  if (!StoredSessions->HasAnyWorkspace())
  {
    Abort();
  }
  else
  {
    TTBCustomItem * Menu =
      DebugNotNull(dynamic_cast<TTBCustomItem *>(Action->ActionComponent));

    Menu->Clear();

    std::unique_ptr<TStrings> Workspaces(StoredSessions->GetWorkspaces());
    for (int Index = 0; Index < Workspaces->Count; Index++)
    {
      TTBCustomItem * Item = new TTBXItem(Menu);
      Item->Caption = Workspaces->Strings[Index];
      Item->Tag = Index;
      Item->Hint = FMTLOAD(WORKSPACE_HINT, (Workspaces->Strings[Index]));
      Item->OnClick = WorkspaceItemClick;
      Menu->Insert(Menu->Count, Item);
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::WorkspaceItemClick(TObject * Sender)
{
  std::unique_ptr<TStrings> Workspaces(StoredSessions->GetWorkspaces());
  ScpExplorer->OpenFolderOrWorkspace(
    Workspaces->Strings[DebugNotNull(dynamic_cast<TTBCustomItem *>(Sender))->Tag]);
}
//---------------------------------------------------------------------------
TShortCut __fastcall TNonVisualDataModule::OpenSessionShortCut(int Index)
{
  if (Index >= 0 && Index < 10)
  {
    return ShortCut((Word)(Index < 9 ? L'0' + 1 + Index : L'0'),
      TShiftState() << ssAlt);
  }
  else
  {
    return scNone;
  }
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::CreateOpenedSessionListMenu(TAction * Action)
{
  TTBCustomItem * OpenedSessionsMenu = dynamic_cast<TTBCustomItem *>(Action->ActionComponent);
  DebugAssert(OpenedSessionsMenu != NULL);
  TTerminalManager * Manager = TTerminalManager::Instance();
  TStrings * TerminalList = Manager->TerminalList;
  int PrevCount = OpenedSessionsMenu->Count;
  for (int Index = 0; Index < TerminalList->Count; Index++)
  {
    TTerminal * Terminal = dynamic_cast<TTerminal *>(TerminalList->Objects[Index]);
    DebugAssert(Terminal);
    TTBCustomItem * Item = new TTBXItem(OpenedSessionsMenu);
    Item->Caption = TerminalList->Strings[Index];
    Item->Tag = int(Terminal);
    Item->Hint = FMTLOAD(OPENEDSESSION_HINT, (Item->Caption));
    Item->Checked = (Manager->ActiveTerminal == Terminal);
    Item->ShortCut = OpenSessionShortCut(Index);
    Item->OnClick = OpenedSessionItemClick;
    Item->RadioItem = true;
    OpenedSessionsMenu->Add(Item);
  }
  for (int Index = 0; Index < PrevCount; Index++)
  {
    OpenedSessionsMenu->Delete(0);
  }
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::OpenedSessionItemClick(TObject * Sender)
{
  TTerminalManager::Instance()->ActiveTerminal = (TManagedTerminal*)(((TMenuItem *)Sender)->Tag);
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::CreateEditorListMenu(TTBCustomItem * Menu, bool OnFocused)
{
  DebugAssert(Menu != NULL);
  int PrevCount = Menu->Count;

  TTBCustomItem * Item;

  Item = new TTBXItem(Menu);
  Item->Action = OnFocused ? CurrentEditFocusedAction : CurrentEditAction;
  Menu->Add(Item);

  AddMenuSeparator(Menu);

  Item = new TTBXItem(Menu);
  Item->Action = OnFocused ? CurrentEditInternalFocusedAction : CurrentEditInternalAction;
  Menu->Add(Item);

  AddMenuSeparator(Menu);

  TStringList * UsedEditors = CreateSortedStringList();
  try
  {
    const TEditorList * EditorList = WinConfiguration->EditorList;
    for (int Index = 0; Index < EditorList->Count; Index++)
    {
      const TEditorPreferences * Editor = EditorList->Editors[Index];

      if ((Editor->Data->Editor == edExternal) &&
          (UsedEditors->IndexOf(Editor->Data->ExternalEditor) < 0))
      {
        UsedEditors->Add(Editor->Data->ExternalEditor);

        TTBCustomItem * Item = new TTBXItem(Menu);
        Item->Caption = Editor->Name;
        Item->Tag = Index;
        Item->Hint = FMTLOAD(EXTERNAL_EDITOR_HINT, (Editor->Name));
        if (OnFocused)
        {
          Item->OnClick = EditorItemClickFocused;
        }
        else
        {
          Item->OnClick = EditorItemClick;
        }
        Menu->Add(Item);
      }
    }

    Item = new TTBXItem(Menu);
    Item->Action = OnFocused ? CurrentEditWithFocusedAction : CurrentEditWithAction;
    Menu->Add(Item);

    AddMenuSeparator(Menu);

    Item = new TTBXItem(Menu);
    Item->Action = EditorListCustomizeAction;
    Menu->Add(Item);

    for (int Index = 0; Index < PrevCount; Index++)
    {
      Menu->Delete(0);
    }
  }
  __finally
  {
    delete UsedEditors;
  }
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::DoEditorItemClick(TObject * Sender, bool OnFocused)
{
  int Tag = dynamic_cast<TTBXItem*>(Sender)->Tag;
  const TEditorList * EditorList = WinConfiguration->EditorList;
  // sanity check
  if (Tag < EditorList->Count)
  {
    ScpExplorer->ExecuteFile(osCurrent, efExternalEditor, EditorList->Editors[Tag]->Data,
      true, OnFocused);
  }
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::EditorItemClick(TObject * Sender)
{
  DoEditorItemClick(Sender, false);
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::EditorItemClickFocused(TObject * Sender)
{
  DoEditorItemClick(Sender, true);
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::QueuePopupPopup(TObject * /*Sender*/)
{
  TAction * Action = NULL;

  switch (ScpExplorer->DefaultQueueOperation())
  {
    case qoItemQuery:
      Action = QueueItemQueryAction;
      break;

    case qoItemError:
      Action = QueueItemErrorAction;
      break;

    case qoItemPrompt:
      Action = QueueItemPromptAction;
      break;

    case qoItemExecute:
      Action = QueueItemExecuteAction;
      break;

    case qoItemPause:
      Action = QueueItemPauseAction;
      break;

    case qoItemResume:
      Action = QueueItemResumeAction;
      break;
  }

  TTBCustomItem * Item;
  for (int Index = 0; Index < QueuePopup->Items->Count; Index++)
  {
    Item = QueuePopup->Items->Items[Index];
    TTBItemOptions O = Item->Options;
    if ((Action != NULL) && (Item->Action == Action))
    {
      O << tboDefault;
    }
    else
    {
      O >> tboDefault;
    }
    Item->Options = O;
  }

  QueueSpeedComboBoxItemUpdate(QueuePopupSpeedComboBoxItem);
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::ShowUpdatesUpdate()
{
  TUpdatesConfiguration Updates = WinConfiguration->Updates;
  unsigned short H, M, S, MS;
  DecodeTime(Now(), H, M, S, MS);
  int CurrentCompoundVer = Configuration->CompoundVersion;
  CheckForUpdatesAction->ImageIndex =
    ((Updates.HaveResults && (Updates.Results.ForVersion == CurrentCompoundVer) &&
      !Updates.Results.Disabled &&
      ((Updates.Results.Critical && !Updates.ShownResults && (MS >= 500)) ||
       ((!Updates.Results.Critical || Updates.ShownResults) &&
        ((Updates.Results.Version > CurrentCompoundVer) ||
         !Updates.Results.Message.IsEmpty())))) ? 80 :
     ((int(Updates.Period) <= 0) ? 81 : 63));
  CheckForUpdatesAction->Visible = !IsUWP();
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::PreferencesDialog(TPreferencesMode APreferencesMode)
{
  if (ScpExplorer != NULL)
  {
    ScpExplorer->PreferencesDialog(APreferencesMode);
  }
  else
  {
    DoPreferencesDialog(APreferencesMode);
  }
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::CustomCommandsLastUpdate(TAction * Action)
{
  TCustomCommandType Command;
  int State;
  bool Defined = ScpExplorer->GetLastCustomCommand(
    (Action == CustomCommandsLastFocusedAction), Command, State);
  Action->Visible = Defined;
  if (Defined)
  {
    UnicodeString TitleCommand = Command.Command;
    int MaxTitleCommandLen = 20;
    if (TitleCommand.Length() > MaxTitleCommandLen)
    {
      TitleCommand = TitleCommand.SubString(1, MaxTitleCommandLen - 3) + Ellipsis;
    }
    Action->Caption = FMTLOAD(CUSTOM_COMMAND_LAST, (EscapeHotkey(TitleCommand)));
    Action->Hint = Command.Command;
    Action->Enabled = (State > 0);
  }
}
//---------------------------------------------------------------------------
UnicodeString __fastcall TNonVisualDataModule::QueueItemSpeed(const UnicodeString & Text,
  TTBXComboBoxItem * Item)
{
  // Keep in sync with TProgressForm::ItemSpeed
  unsigned long Speed = GetSpeedLimit(Text);
  ScpExplorer->ExecuteQueueOperation(qoItemSpeed, reinterpret_cast<void*>(Speed));

  UnicodeString Result = SetSpeedLimit(Speed);
  SaveToHistory(Item->Strings, Result);
  CustomWinConfiguration->History[L"SpeedLimit"] = Item->Strings;

  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::QueuePopupSpeedComboBoxItemItemClick(
  TObject * Sender)
{
  TTBXComboBoxItem * Item = dynamic_cast<TTBXComboBoxItem *>(Sender);
  QueueItemSpeedAction->Text = QueueItemSpeed(Item->Text, Item);
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::QueueSpeedComboBoxItemAcceptText(
  TObject * Sender, UnicodeString & NewText, bool & /*Accept*/)
{
  TTBXComboBoxItem * Item = dynamic_cast<TTBXComboBoxItem *>(Sender);
  NewText = QueueItemSpeed(NewText, Item);
  QueueItemSpeedAction->Text = NewText;
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::QueueSpeedComboBoxItem(TTBXComboBoxItem * Item)
{
  // IDE often looses this link
  Item->OnAcceptText = QueueSpeedComboBoxItemAcceptText;
  Item->OnItemClick = QueuePopupSpeedComboBoxItemItemClick;
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::QueueSpeedComboBoxItemUpdate(TTBXComboBoxItem * Item)
{
  CopySpeedLimits(CustomWinConfiguration->History[L"SpeedLimit"], Item->Strings);
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::SetQueueOnceEmptyAction(TAction * Action)
{
  TAction * Current = CurrentQueueOnceEmptyAction();
  if (Current != Action)
  {
    Current->Checked = false;
    Action->Checked = true;
  }
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::CycleQueueOnceEmptyAction()
{
  TAction * Current = CurrentQueueOnceEmptyAction();
  Current->Checked = false;
  if (Current == QueueIdleOnceEmptyAction)
  {
    QueueDisconnectOnceEmptyAction2->Checked = true;
  }
  else if (Current == QueueDisconnectOnceEmptyAction2)
  {
    QueueSuspendOnceEmptyAction2->Checked = true;
  }
  else if (Current == QueueSuspendOnceEmptyAction2)
  {
    QueueShutDownOnceEmptyAction2->Checked = true;
  }
  else if (Current == QueueShutDownOnceEmptyAction2)
  {
    QueueIdleOnceEmptyAction->Checked = true;
  }
  else
  {
    DebugFail();
  }
}
//---------------------------------------------------------------------------
TAction * __fastcall TNonVisualDataModule::CurrentQueueOnceEmptyAction()
{
  TAction * Result;
  if (QueueIdleOnceEmptyAction->Checked)
  {
    Result = QueueIdleOnceEmptyAction;
  }
  else if (QueueDisconnectOnceEmptyAction2->Checked)
  {
    Result = QueueDisconnectOnceEmptyAction2;
  }
  else if (QueueSuspendOnceEmptyAction2->Checked)
  {
    Result = QueueSuspendOnceEmptyAction2;
  }
  else if (QueueShutDownOnceEmptyAction2->Checked)
  {
    Result = QueueShutDownOnceEmptyAction2;
  }
  else
  {
    DebugFail();
  }
  return Result;
}
//---------------------------------------------------------------------------
TOnceDoneOperation __fastcall TNonVisualDataModule::CurrentQueueOnceEmptyOperation()
{
  TOnceDoneOperation Result;
  TBasicAction * Current = CurrentQueueOnceEmptyAction();
  if (Current == QueueIdleOnceEmptyAction)
  {
    Result = odoIdle;
  }
  else if (Current == QueueDisconnectOnceEmptyAction2)
  {
    Result = odoDisconnect;
  }
  else if (Current == QueueSuspendOnceEmptyAction2)
  {
    Result = odoSuspend;
  }
  else if (Current == QueueShutDownOnceEmptyAction2)
  {
    Result = odoShutDown;
  }
  else
  {
    DebugFail();
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::ResetQueueOnceEmptyOperation()
{
  SetQueueOnceEmptyAction(QueueIdleOnceEmptyAction);
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::StartBusy()
{
  FBusy++;
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::EndBusy()
{
  FBusy--;
}
//---------------------------------------------------------------------------
bool __fastcall TNonVisualDataModule::GetBusy()
{
  return
    (FBusy > 0) ||
    ((ScpExplorer != NULL) && ScpExplorer->IsBusy());
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::FocusedEditMenuItemPopup(TTBCustomItem * Sender,
  bool /*FromLink*/)
{
  CreateEditorListMenu(Sender, true);
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::EditMenuItemPopup(TTBCustomItem * Sender,
  bool /*FromLink*/)
{
  CreateEditorListMenu(Sender, false);
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::QueuePopupSpeedComboBoxItemAdjustImageIndex(
  TTBXComboBoxItem * Sender, const UnicodeString /*AText*/, int /*AIndex*/, int & ImageIndex)
{
  // Use fixed image (do not change image by item index)
  ImageIndex = Sender->ImageIndex;
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::ToolbarButtonItemClick(TObject * Sender)
{
  TTBCustomItem * Item = DebugNotNull(dynamic_cast<TTBCustomItem *>(Sender));
  TTBCustomItem * ButtonItem = reinterpret_cast<TTBCustomItem *>(Item->Tag);
  ButtonItem->Visible = !ButtonItem->Visible;
}
//---------------------------------------------------------------------------
bool __fastcall TNonVisualDataModule::IsCustomizableToolbarItem(TTBCustomItem * Item)
{
  return
    ((dynamic_cast<TTBXItem *>(Item) != NULL) ||
     (dynamic_cast<TTBXSubmenuItem *>(Item) != NULL)) &&
    (Item->Action != CustomCommandsLastAction) &&
    DebugAlwaysTrue(Item->Action != CustomCommandsLastFocusedAction);
}
//---------------------------------------------------------------------------
bool __fastcall TNonVisualDataModule::IsToolbarCustomizable()
{
  bool Result = false;
  if (FCustomizedToolbar != NULL)
  {
    for (int Index = 0; Index < FCustomizedToolbar->Items->Count; Index++)
    {
      if (IsCustomizableToolbarItem(FCustomizedToolbar->Items->Items[Index]))
      {
        Result = true;
        break;
      }
    }
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::CreateToolbarButtonsList()
{
  if (FCustomizedToolbar != NULL)
  {
    TTBCustomItem * CustomizeItem = DebugNotNull(dynamic_cast<TTBCustomItem *>(CustomizeToolbarAction->ActionComponent));
    CustomizeItem->Clear();

    for (int Index = 0; Index < FCustomizedToolbar->Items->Count; Index++)
    {
      TTBCustomItem * ButtonItem = FCustomizedToolbar->Items->Items[Index];

      TTBCustomItem * Item = NULL;
      if (dynamic_cast<TTBSeparatorItem *>(ButtonItem) != NULL)
      {
        Item = new TTBSeparatorItem(CustomizeItem);
      }
      else if (IsCustomizableToolbarItem(ButtonItem))
      {
        Item = new TTBXItem(CustomizeItem);
        Item->Caption = StripEllipsis(ButtonItem->Caption);
        Item->ImageIndex = ButtonItem->ImageIndex;
        Item->Tag = reinterpret_cast<int>(ButtonItem);
        Item->OnClick = ToolbarButtonItemClick;
        Item->Checked = ButtonItem->Visible;
      }

      if (Item != NULL)
      {
        CustomizeItem->Insert(CustomizeItem->Count, Item);
      }
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TNonVisualDataModule::ControlContextPopup(TObject * Sender, const TPoint & MousePos)
{
  TTBDock * Dock = dynamic_cast<TTBDock *>(Sender);
  if (Dock != NULL)
  {
    // While we can identify toolbar for which context menu is popping up in OnExecute,
    // we cannot in OnUpdate, so we have to remember it here.
    FCustomizedToolbar = dynamic_cast<TTBCustomToolbar *>(Dock->ControlAtPos(MousePos, true, true, false));
  }
  else
  {
    FCustomizedToolbar = NULL;
  }
}
