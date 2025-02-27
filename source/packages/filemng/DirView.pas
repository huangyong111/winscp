unit DirView;
{===============================================================
 Component TDirView / Version 2.6, January 2000
 ===============================================================


    Description:
    ============
    Displays files of a single directory as listview with shell
    icons. Complete drag&Drop support for files and directories.


    Author:
    =======
    (c) Ingo Eckel 1998, 1999
    Sodener Weg 38
    65812 Bad Soden
    Germany

    Modifications (for WinSCP):
    ===========================
    (c) Martin Prikryl 2001- 2004

    V2.6:
    - Shows "shared"-symbol with directories
    - Delphi5 compatible

    For detailed documentation and history see TDirView.htm.

 ===============================================================}

{Required compiler options for TDirView:}
{$A+,B-,X+,H+,P+}

interface

{$WARN UNIT_PLATFORM OFF}
{$WARN SYMBOL_PLATFORM OFF}

uses
  Windows, ShlObj, ComCtrls, CompThread, CustomDirView, ListExt,
  ExtCtrls, Graphics, FileOperator, DiscMon, Classes, DirViewColProperties,
  DragDrop, Messages, ListViewColProperties, CommCtrl, DragDropFilesEx,
  FileCtrl, SysUtils, BaseUtils, Controls, CustomDriveView, System.Generics.Collections, Winapi.ShellAPI;

type
  TVolumeDisplayStyle = (doPrettyName, doDisplayName); {Diplaytext of drive node}

const
  msThreadChangeDelay = 10; {TDiscMonitor: change delay}
  MaxWaitTimeOut = 10; {TFileDeleteThread: wait nn seconds for deleting files or directories}
{$WARN SYMBOL_DEPRECATED OFF}
  FileAttr = SysUtils.faAnyFile and (not SysUtils.faVolumeID);
{$WARN SYMBOL_DEPRECATED ON}
  SpecialExtensions = 'EXE,LNK,ICO,ANI,CUR,PIF,JOB,CPL';
  ExeExtension = 'EXE';

type
  {Exceptions:}
  EIUThread = class(Exception);
  EDragDrop = class(Exception);
  EInvalidFileName = class(Exception);
  ERenameFileFailed = class(Exception);

  TClipboardOperation = (cboNone, cboCut, cboCopy);

  {Record for each file item:}
  PFileRec = ^TFileRec;
  TFileRec = record
    Empty: Boolean;
    IconEmpty: Boolean;
    IsDirectory: Boolean;
    IsRecycleBin: Boolean;
    IsParentDir: Boolean;
    FileName: string;
    Displayname: string;
    FileExt: string;
    TypeName: string;
    ImageIndex: Integer;
    Size: Int64;
    Attr: LongWord;
    FileTime: TFileTime;
    PIDL: PItemIDList; {Fully qualified PIDL}
  end;

  {Record for fileinfo caching:}
  PInfoCache = ^TInfoCache;
  TInfoCache = record
    FileExt: string;
    TypeName: string;
    ImageIndex: Integer;
  end;

{Additional events:}
type
  TDirViewFileSizeChanged = procedure(Sender: TObject; Item: TListItem) of object;
  TDirViewFileIconForName = procedure(Sender: TObject; Item: TListItem; var FileName: string) of object;

type
  TDirView = class;

  {  TIconUpdateThread (Fetch shell icons via thread) }
  TIconUpdateThread = class(TCompThread)
  private
    FOwner: TDirView;
    FIndex: Integer;
    FMaxIndex: Integer;
    FNewIcons: Boolean;
    FSyncIcon: Integer;
    CurrentIndex: Integer;
    CurrentFilePath: string;
    CurrentItemData: TFileRec;
    InvalidItem: Boolean;

    procedure SetIndex(Value: Integer);
    procedure SetMaxIndex(Value: Integer);

  protected
    constructor Create(Owner: TDirView);
    procedure DoFetchData;
    procedure DoUpdateIcon;
    procedure Execute; override;

    property Index: Integer read FIndex write SetIndex;
    property MaxIndex: Integer read FMaxIndex write SetMaxIndex;

  public
    procedure Terminate; override;
  end;

  { TDirView }
  TDirView = class(TCustomDirView)
  private
    FConfirmDelete: Boolean;
    FConfirmOverwrite: Boolean;
    FUseIconCache: Boolean;
    FInfoCacheList: TListExt;
    FDriveView: TCustomDriveView;
    FChangeTimer: TTimer;
    FChangeInterval: Cardinal;
    FUseIconUpdateThread: Boolean;
    FIUThreadFinished: Boolean;
    FDriveType: Integer;
    FParentFolder: IShellFolder;
    FDesktopFolder: IShellFolder;
    FDirOK: Boolean;
    FPath: string;
    SelectNewFiles: Boolean;
    FHiddenCount: Integer;
    FFilteredCount: Integer;
    FNotRelative: Boolean;

    {shFileOperation-shell component TFileOperator:}
    FFileOperator: TFileOperator;
    {Additional thread components:}
    FIconUpdateThread: TIconUpdateThread;
    FDiscMonitor: TDiscMonitor;
    FHomeDirectory: string;

    {Additional events:}
    FOnFileIconForName: TDirViewFileIconForName;

    iRecycleFolder: iShellFolder;
    PIDLRecycle: PItemIDList;

    FLastPath: TDictionary<string, string>;
    FTimeoutShellIconRetrieval: Boolean;

    {Drag&Drop:}
    function GetDirColProperties: TDirViewColProperties;
    function GetHomeDirectory: string;

    {Drag&drop helper functions:}
    procedure SignalFileDelete(Sender: TObject; Files: TStringList);
    procedure PerformDragDropFileOperation(TargetPath: string; Effect: Integer;
      RenameOnCollision: Boolean; Paste: Boolean);
    procedure SetDirColProperties(Value: TDirViewColProperties);

  protected
    function NewColProperties: TCustomListViewColProperties; override;
    function SortAscendingByDefault(Index: Integer): Boolean; override;
    procedure Notification(AComponent: TComponent; Operation: TOperation); override;

    procedure Delete(Item: TListItem); override;
    procedure DDError(ErrorNo: TDDError);
    function GetCanUndoCopyMove: Boolean; virtual;
    {Shell namespace functions:}
    function GetShellFolder(Dir: string): iShellFolder;
    function GetDirOK: Boolean; override;
    procedure GetDisplayInfo(ListItem: TListItem; var DispInfo: TLVItem); override;

    procedure DDDragDetect(grfKeyState: Longint; DetectStart, Point: TPoint;
      DragStatus: TDragDetectStatus); override;
    procedure DDMenuPopup(Sender: TObject; AMenu: HMenu; DataObj: IDataObject;
      AMinCustCmd:integer; grfKeyState: Longint; pt: TPoint); override;
    procedure DDMenuDone(Sender: TObject; AMenu: HMenu); override;
    procedure DDDropHandlerSucceeded(Sender: TObject; grfKeyState: Longint;
      Point: TPoint; dwEffect: Longint); override;
    procedure DDChooseEffect(grfKeyState: Integer; var dwEffect: Integer; PreferredEffect: Integer); override;

    function GetPathName: string; override;
    procedure SetChangeInterval(Value: Cardinal); virtual;
    procedure LoadFromRecycleBin(Dir: string); virtual;
    procedure SetLoadEnabled(Value: Boolean); override;
    function GetPath: string; override;
    procedure SetPath(Value: string); override;
    procedure PathChanged; override;
    procedure SetItemImageIndex(Item: TListItem; Index: Integer); override;
    procedure ChangeDetected(Sender: TObject; const Directory: string;
      var SubdirsChanged: Boolean);
    procedure ChangeInvalid(Sender: TObject; const Directory: string; const ErrorStr: string);
    procedure TimerOnTimer(Sender: TObject);
    procedure ResetItemImage(Index: Integer);
    procedure SetWatchForChanges(Value: Boolean); override;
    procedure AddParentDirItem;
    procedure AddToDragFileList(FileList: TFileList; Item: TListItem); override;
    function DragCompleteFileList: Boolean; override;
    procedure ExecuteFile(Item: TListItem); override;
    function GetIsRoot: Boolean; override;
    procedure InternalEdit(const HItem: TLVItem); override;
    function ItemColor(Item: TListItem): TColor; override;
    function ItemFileExt(Item: TListItem): string;
    function ItemFileNameOnly(Item: TListItem): string;
    function ItemImageIndex(Item: TListItem; Cache: Boolean): Integer; override;
    function ItemIsFile(Item: TListItem): Boolean; override;
    function ItemIsRecycleBin(Item: TListItem): Boolean; override;
    function ItemMatchesFilter(Item: TListItem; const Filter: TFileFilter): Boolean; override;
    function FileMatches(FileName: string; const SearchRec: TSearchRec): Boolean;
    function ItemOverlayIndexes(Item: TListItem): Word; override;
    procedure LoadFiles; override;
    procedure PerformItemDragDropOperation(Item: TListItem; Effect: Integer; Paste: Boolean); override;
    procedure SortItems; override;
    procedure StartFileDeleteThread;
    procedure WMDestroy(var Msg: TWMDestroy); message WM_DESTROY;
    procedure CMRecreateWnd(var Message: TMessage); message CM_RECREATEWND;
    procedure Load(DoFocusSomething: Boolean); override;
    function GetFileInfo(pszPath: LPCWSTR; dwFileAttributes: DWORD; var psfi: TSHFileInfoW; cbFileInfo, uFlags: UINT): DWORD_PTR;
    function DoCopyToClipboard(Focused: Boolean; Cut: Boolean; Operation: TClipBoardOperation): Boolean;

    function HiddenCount: Integer; override;
    function FilteredCount: Integer; override;

  public
    {Runtime, readonly properties:}
    property DriveType: Integer read FDriveType;
    {Linked component TDriveView:}
    property DriveView: TCustomDriveView read FDriveView write FDriveView;
    { required, otherwise AV generated, when dragging columns}
    property Columns stored False;
    property ParentFolder: IShellFolder read FParentFolder;
    {Drag&Drop runtime, readonly properties:}
    property CanUndoCopyMove: Boolean read GetCanUndoCopyMove;
    property DDFileOperator: TFileOperator read FFileOperator;
    {Drag&Drop fileoperation methods:}
    function UndoCopyMove: Boolean; dynamic;
    {Clipboard fileoperation methods (requires drag&drop enabled):}
    procedure EmptyClipboard; dynamic;
    function CopyToClipBoard(Focused: Boolean): Boolean; dynamic;
    function CutToClipBoard(Focused: Boolean): Boolean; dynamic;
    function PasteFromClipBoard(TargetPath: string = ''): Boolean; override;
    function DuplicateSelectedFiles: Boolean; dynamic;
    procedure DisplayPropertiesMenu; override;
    procedure DisplayContextMenu(Where: TPoint); override;

    procedure ExecuteParentDirectory; override;
    procedure ExecuteRootDirectory; override;
    function ItemIsDirectory(Item: TListItem): Boolean; override;
    function ItemFullFileName(Item: TListItem): string; override;
    function ItemIsParentDirectory(Item: TListItem): Boolean; override;
    function ItemFileName(Item: TListItem): string; override;
    function ItemFileSize(Item: TListItem): Int64; override;
    function ItemFileTime(Item: TListItem; var Precision: TDateTimePrecision): TDateTime; override;
    procedure OpenFallbackPath(Value: string);

    {Thread handling: }
    procedure StartWatchThread;
    procedure StopWatchThread;
    function WatchThreadActive: Boolean;
    procedure StartIconUpdateThread;
    procedure StopIconUpdateThread;
    procedure TerminateThreads;

    {Other additional functions: }
    procedure ClearIconCache;

    {Create a new subdirectory:}
    procedure CreateDirectory(DirName: string); override;
    {Delete all selected files:}

    {Check, if file or files still exists:}
    procedure ValidateFile(Item: TListItem); overload;
    procedure ValidateFile(FileName:TFileName); overload;
    procedure ValidateSelectedFiles; dynamic;

    {Access the internal data-structures:}
    function AddItem(SRec: SysUtils.TSearchRec): TListItem; reintroduce;
    procedure GetDisplayData(Item: TListItem; FetchIcon: Boolean);
    function GetFileRec(Index: Integer): PFileRec;

    {Populate / repopulate the filelist:}
    procedure Reload(CacheIcons : Boolean); override;
    procedure Reload2;

    function FormatFileTime(FileTime: TFileTime): string; virtual;
    function GetAttrString(Attr: Integer): string; virtual;

    constructor Create(AOwner: TComponent); override;
    destructor Destroy; override;
    procedure ExecuteHomeDirectory; override;
    procedure ReloadDirectory; override;
    procedure ExecuteDrive(Drive: string);
    property HomeDirectory: string read GetHomeDirectory write FHomeDirectory;
    property TimeoutShellIconRetrieval: Boolean read FTimeoutShellIconRetrieval write FTimeoutShellIconRetrieval;

  published
    property DirColProperties: TDirViewColProperties read GetDirColProperties write SetDirColProperties;
    property PathLabel;
    property OnUpdateStatusBar;

    property DimmHiddenFiles;
    property ShowHiddenFiles;
    property WantUseDragImages;
    property TargetPopupMenu;
    property AddParentDir;
    property OnSelectItem;
    property OnStartLoading;
    property OnLoaded;
    property OnDDDragEnter;
    property OnDDDragLeave;
    property OnDDDragOver;
    property OnDDDrop;
    property OnDDQueryContinueDrag;
    property OnDDGiveFeedback;
    property OnDDDragDetect;
    property OnDDCreateDragFileList;
    property OnDDEnd;
    property OnDDCreateDataObject;
    property OnDDTargetHasDropHandler;
    {Drag&Drop:}
    property DDLinkOnExeDrag default True;
    property OnDDProcessDropped;
    property OnDDError;
    property OnDDExecuted;
    property OnDDFileOperation;
    property OnDDFileOperationExecuted;

    property OnExecFile;
    property OnMatchMask;
    property OnGetOverlay;
    property OnGetItemColor;

    {Confirm deleting files}
    property ConfirmDelete: Boolean
      read FConfirmDelete write FConfirmDelete default True;
    {Confirm overwriting files}
    property ConfirmOverwrite: Boolean
      read FConfirmOverwrite write fConfirmOverwrite default True;
    {Reload the directory after only the interval:}
    property ChangeInterval: Cardinal
      read FChangeInterval write SetChangeInterval default MSecsPerSec;
    {Fetch shell icons by thread:}
    property UseIconUpdateThread: Boolean
      read FUseIconUpdateThread write FUseIconUpdateThread default False;
    {Enables or disables icon caching for registered file extensions. Caching enabled
     enhances the performance but does not take care about installed icon handlers, wich
     may modify the display icon for registered files. Only the iconindex is cached not the
     icon itself:}
    property UseIconCache: Boolean
      read FUseIconCache write FUseIconCache default False;
    {Watch current directory for filename changes (create, rename, delete files)}
    property WatchForChanges;

    {Additional events:}
    property OnFileIconForName: TDirViewFileIconForName
      read FOnFileIconForName write FOnFileIconForName;
    property UseSystemContextMenu;
    property OnContextPopup;
    property OnHistoryChange;
    property OnHistoryGo;
    property OnPathChange;
    property OnBusy;
    property OnChangeFocus;

    property ColumnClick;
    property MultiSelect;
    property ReadOnly;

    // The only way to make Items stored automatically and survive handle recreation.
    // Though we should implement custom persisting to avoid publishing this
    property Items;
  end; {Type TDirView}

procedure Register;

{Returns True, if the specified extension matches one of the extensions in ExtList:}
function MatchesFileExt(Ext: string; const FileExtList: string): Boolean;

function DropLink(Item: PFDDListItem; TargetPath: string): Boolean;
function DropFiles(
  DragDropFilesEx: TCustomizableDragDropFilesEx; Effect: Integer; FileOperator: TFileOperator; TargetPath: string;
  RenameOnCollision: Boolean; IsRecycleBin: Boolean; ConfirmDelete: Boolean; ConfirmOverwrite: Boolean; Paste: Boolean;
  Sender: TObject; OnDDFileOperation: TDDFileOperationEvent;
  out SourcePath: string; out SourceIsDirectory: Boolean): Boolean;
procedure CheckCanOpenDirectory(Path: string);

var
  LastClipBoardOperation: TClipBoardOperation;

implementation

uses
  DriveView, OperationWithTimeout,
  PIDL, Forms, Dialogs,
  ComObj,
  ActiveX, ImgList,
  ShellDialogs, IEDriveInfo,
  FileChanges, Math, PasTools, StrUtils, Types, UITypes;

var
  DaylightHack: Boolean;

procedure Register;
begin
  RegisterComponents('DriveDir', [TDirView]);
end; {Register}

function CompareInfoCacheItems(I1, I2: Pointer): Integer;
begin
  if PInfoCache(I1)^.FileExt < PInfoCache(I2)^.FileExt then Result := fLess
    else
  if PInfoCache(I1)^.FileExt > PInfoCache(I2)^.FileExt then Result := fGreater
    else Result := fEqual;
end; {CompareInfoCacheItems}

function MatchesFileExt(Ext: string; const FileExtList: string): Boolean;
begin
  Result := (Length(Ext) = 3) and (Pos(Ext, FileExtList) <> 0);
end; {MatchesFileExt}

function FileTimeToDateTime(FileTime: TFileTime): TDateTime;
var
  SysTime: TSystemTime;
  UniverzalSysTime: TSystemTime;
  LocalFileTime: TFileTime;
begin
  // duplicated in Common.cpp

  // The 0xFFF... is sometime seen for invalid timestamps,
  // it would cause failure in SystemTimeToDateTime below
  if FileTime.dwLowDateTime = High(DWORD) then
  begin
    Result := MinDateTime;
  end
    else
  begin
    if not DaylightHack then
    begin
      FileTimeToSystemTime(FileTime, UniverzalSysTime);
      SystemTimeToTzSpecificLocalTime(nil, UniverzalSysTime, SysTime);
    end
      else
    begin
      FileTimeToLocalFileTime(FileTime, LocalFileTime);
      FileTimeToSystemTime(LocalFileTime, SysTime);
    end;
    Result := SystemTimeToDateTime(SysTime);
  end;
end;

function SizeFromSRec(const SRec: SysUtils.TSearchRec): Int64;
begin
  with SRec do
  begin
    // Hopefuly TSearchRec.FindData is available with all Windows versions
    {if Size >= 0 then Result := Size
      else}
{$WARNINGS OFF}
    Result := Int64(FindData.nFileSizeHigh) shl 32 + FindData.nFileSizeLow;
{$WARNINGS ON}
  end;
end;

function DropLink(Item: PFDDListItem; TargetPath: string): Boolean;
var
  Drive: string;
  SourcePath: string;
  SourceFile: string;
begin
  SourceFile := Item.Name;
  if IsRootPath(SourceFile) then
  begin
    Drive := DriveInfo.GetDriveKey(SourceFile);
    SourcePath := Copy(DriveInfo.Get(Drive).PrettyName, 4, 255) + ' (' + Drive + ')'
  end
    else
  begin
    SourcePath := ExtractFileName(SourceFile);
  end;

  Result :=
    CreateFileShortCut(SourceFile,
      IncludeTrailingBackslash(TargetPath) + ChangeFileExt(SourcePath, '.lnk'),
      ExtractFileNameOnly(SourceFile));
end;

function DropFiles(
  DragDropFilesEx: TCustomizableDragDropFilesEx; Effect: Integer; FileOperator: TFileOperator; TargetPath: string;
  RenameOnCollision: Boolean; IsRecycleBin: Boolean; ConfirmDelete: Boolean; ConfirmOverwrite: Boolean; Paste: Boolean;
  Sender: TObject; OnDDFileOperation: TDDFileOperationEvent;
  out SourcePath: string; out SourceIsDirectory: Boolean): Boolean;
var
  Index: Integer;
  DoFileOperation: Boolean;
begin
  SourcePath := '';

  {Set the source filenames:}
  for Index := 0 to DragDropFilesEx.FileList.Count - 1 do
  begin
    FileOperator.OperandFrom.Add(
      TFDDListItem(DragDropFilesEx.FileList[Index]^).Name);
    if DragDropFilesEx.FileNamesAreMapped then
      FileOperator.OperandTo.Add(IncludeTrailingPathDelimiter(TargetPath) +
        TFDDListItem(DragDropFilesEx.FileList[Index]^).MappedName);

    if SourcePath = '' then
    begin
      if DirectoryExists(TFDDListItem(DragDropFilesEx.FileList[Index]^).Name) then
      begin
        SourcePath := TFDDListItem(DragDropFilesEx.FileList[Index]^).Name;
        SourceIsDirectory := True;
      end
        else
      begin
        SourcePath := ExtractFilePath(TFDDListItem(DragDropFilesEx.FileList[Index]^).Name);
        SourceIsDirectory := False;
      end;
    end;
  end;

  FileOperator.Flags := [foAllowUndo, foNoConfirmMkDir];
  if RenameOnCollision then
  begin
    FileOperator.Flags := FileOperator.Flags + [foRenameOnCollision];
    FileOperator.WantMappingHandle := True;
  end
    else FileOperator.WantMappingHandle := False;

  {Set the target directory or the target filenames:}
  if DragDropFilesEx.FileNamesAreMapped and (not IsRecycleBin) then
  begin
    FileOperator.Flags := FileOperator.Flags + [foMultiDestFiles];
  end
    else
  begin
    FileOperator.Flags := FileOperator.Flags - [foMultiDestFiles];
    FileOperator.OperandTo.Clear;
    FileOperator.OperandTo.Add(TargetPath);
  end;

  {if the target directory is the recycle bin, then delete the selected files:}
  if IsRecycleBin then
  begin
    FileOperator.Operation := foDelete;
  end
    else
  begin
    case Effect of
      DROPEFFECT_COPY: FileOperator.Operation := foCopy;
      DROPEFFECT_MOVE: FileOperator.Operation := foMove;
    end;
  end;

  if IsRecycleBin then
  begin
    if not ConfirmDelete then
      FileOperator.Flags := FileOperator.Flags + [foNoConfirmation];
  end
    else
  begin
    if not ConfirmOverwrite then
      FileOperator.Flags := FileOperator.Flags + [foNoConfirmation];
  end;

  DoFileOperation := True;
  if Assigned(OnDDFileOperation) then
  begin
    OnDDFileOperation(Sender, Effect, SourcePath, TargetPath, False, DoFileOperation);
  end;

  Result := DoFileOperation and (FileOperator.OperandFrom.Count > 0);
  if Result then
  begin
    FileOperator.Execute;
    if DragDropFilesEx.FileNamesAreMapped then
      FileOperator.ClearUndo;
  end;
end;

function GetShellDisplayName(
  const ShellFolder: IShellFolder; IDList: PItemIDList; Flags: DWORD; var Name: string): Boolean;
var
  Str: TStrRet;
begin
  Result := True;
  Name := '';
  if ShellFolder.GetDisplayNameOf(IDList, Flags, Str) = NOERROR then
  begin
    case Str.uType of
      STRRET_WSTR: Name := WideCharToString(Str.pOleStr);
      STRRET_OFFSET: Name := PChar(UINT(IDList) + Str.uOffset);
      STRRET_CSTR: Name := string(Str.cStr);
      else Result := False;
    end;
  end
    else Result := False;
end; {GetShellDisplayName}

procedure CheckCanOpenDirectory(Path: string);
var
  DosError: Integer;
  SRec: SysUtils.TSearchRec;
begin
  if not DirectoryExistsFix(Path) then
    raise Exception.CreateFmt(SDirNotExists, [Path]);
  DosError := SysUtils.FindFirst(ApiPath(IncludeTrailingPathDelimiter(Path) + '*.*'), FileAttr, SRec);
  if DosError = ERROR_SUCCESS then
  begin
    FindClose(SRec);
  end
    else
  begin
    // File not found is expected when accessing a root folder of an empty drive
    if DosError <> ERROR_FILE_NOT_FOUND then
    begin
      RaiseLastOSError;
    end;
  end;
end;

{ TIconUpdateThread }

constructor TIconUpdateThread.Create(Owner: TDirView);
begin
  inherited Create(True);
  FOwner := Owner;
  FIndex := 0;
  FNewIcons := False;
  if (FOwner.ViewStyle = vsReport) or (FOwner.ViewStyle = vsList) then
    FMaxIndex := FOwner.VisibleRowCount
      else FMaxIndex := 0;
  FOwner.FIUThreadFinished := False;
end; {TIconUpdateThread.Create}

procedure TIconUpdateThread.SetMaxIndex(Value: Integer);
var
  Point: TPoint;
  Item: TListItem;
begin
  if Value <> MaxIndex then
  begin
    FNewIcons := True;
    if Value < FMaxIndex then
    begin
      if Suspended then FIndex := Value
        else
      begin
        Point.X := 0;
        Point.X := 0;
        Item := FOwner.GetNearestItem(Point, TSearchDirection(sdAbove));
        if Assigned(Item) then FIndex := Item.Index
          else FIndex := Value;
      end;
    end
      else FMaxIndex := Value;
  end;
end; {SetMaxIndex}

procedure TIconUpdateThread.SetIndex(Value: Integer);
var
  PageSize: Integer;
begin
  if Value <> Index then
  begin
    PageSize := FOwner.VisibleRowCount;
    FIndex := Value;
    FNewIcons := True;
    if FOwner.ViewStyle = vsList then FMaxIndex := Value + 2 * PageSize
      else FMaxIndex := Value + PageSize;
  end;
end; {SetIndex}

procedure TIconUpdateThread.Execute;
var
  FileInfo: TShFileInfo;
  Count: Integer;
  Eaten: ULONG;
  ShAttr: ULONG;
  FileIconForName: string;
  ForceByName: Boolean;
begin
  if Assigned(FOwner.TopItem) then FIndex := FOwner.TopItem.Index
    else FIndex := 0;

  FNewIcons := (FIndex > 0);

  while not Terminated do
  begin
    if FIndex > FMaxIndex then Suspend;
    Count := FOwner.Items.Count;
    if not Terminated and ((FIndex >= Count) or (Count = 0)) then
      Suspend;
    InvalidItem := True;
    if Terminated then Break;
    Synchronize(DoFetchData);
    if (not InvalidItem) and (not Terminated) and
      CurrentItemData.IconEmpty then
    begin
      try
        ForceByName := False;
        FileIconForName := CurrentFilePath;
        if Assigned(FOwner.FOnFileIconForName) then
        begin
          FOwner.FOnFileIconForName(FOwner, nil, FileIconForName);
          ForceByName := (FileIconForName <> CurrentFilePath);
        end;

        if not Assigned(CurrentItemData.PIDL) then
        begin
          ShAttr := 0;
          FOwner.FDesktopFolder.ParseDisplayName(FOwner.ParentForm.Handle, nil,
            PChar(CurrentFilePath), Eaten, CurrentItemData.PIDL, ShAttr);
        end;

        if (not ForceByName) and Assigned(CurrentItemData.PIDL) then
        begin
          SHGetFileInfo(PChar(CurrentItemData.PIDL), 0, FileInfo, SizeOf(FileInfo),
            SHGFI_TYPENAME or SHGFI_USEFILEATTRIBUTES or SHGFI_SYSICONINDEX or SHGFI_PIDL)
        end
          else
        begin
          SHGetFileInfo(PChar(FileIconForName), 0, FileInfo, SizeOf(FileInfo),
            SHGFI_TYPENAME or SHGFI_USEFILEATTRIBUTES or SHGFI_SYSICONINDEX);
        end;
      except
        {Capture exceptions generated by the shell}
        FSyncIcon := UnKnownFileIcon;
      end;
      if Terminated then
      begin
        FreePIDL(CurrentItemData.PIDL);
        Break;
      end;
      FSyncIcon := FileInfo.iIcon;
      if FSyncIcon <> CurrentItemData.ImageIndex then
        FNewIcons := True;
      if not Terminated then
      begin
        Synchronize(DoUpdateIcon);
      end;

      FreePIDL(CurrentItemData.PIDL);
    end;
    SetLength(CurrentFilePath, 0);
    if CurrentIndex = FIndex then Inc(FIndex);
    SetLength(CurrentFilePath, 0);
  end;
end; {TIconUpdateThread.Execute}

procedure TIconUpdateThread.DoFetchData;
begin
  CurrentIndex := fIndex;
  if not Terminated and
     (Pred(FOwner.Items.Count) >= CurrentIndex) and
     Assigned(FOwner.Items[CurrentIndex]) and
     Assigned(FOwner.Items[CurrentIndex].Data) then
  begin
    CurrentFilePath := FOwner.ItemFullFileName(FOwner.Items[CurrentIndex]);
    CurrentItemData := PFileRec(FOwner.Items[CurrentIndex].Data)^;
    InvalidItem := False;
  end
    else InvalidItem := True;
end; {TIconUpdateThread.DoFetchData}

procedure TIconUpdateThread.DoUpdateIcon;
var
  LVI: TLVItem;
begin
  if (FOwner.Items.Count > CurrentIndex) and
     not fOwner.Loading and not Terminated and
     Assigned(FOwner.Items[CurrentIndex]) and
     Assigned(FOwner.Items[CurrentIndex].Data) then
    with FOwner.Items[CurrentIndex] do
    begin
      if (FSyncIcon >= 0) and (PFileRec(Data)^.ImageIndex <> FSyncIcon) then
      begin
        with PFileRec(Data)^ do
          ImageIndex := FSyncIcon;

        {To avoid flickering of the display use Listview_SetItem
        instead of using the property ImageIndex:}
        LVI.mask := LVIF_IMAGE;
        LVI.iItem := CurrentIndex;
        LVI.iSubItem := 0;
        LVI.iImage := I_IMAGECALLBACK;
        if not Terminated then
          ListView_SetItem(FOwner.Handle, LVI);
        FNewIcons := True;
      end;
      PFileRec(Data)^.IconEmpty := False;
    end;
end; {TIconUpdateThread.DoUpdateIcon}

procedure TIconUpdateThread.Terminate;
begin
  FOwner.FIUThreadFinished := True;
  inherited;
end; {TIconUpdateThread.Terminate}

{ TDirView }

constructor TDirView.Create(AOwner: TComponent);
begin
  inherited Create(AOwner);

  FInfoCacheList := TListExt.Create(SizeOf(TInfoCache));
  FDriveType := DRIVE_UNKNOWN;
  FUseIconCache := False;
  FConfirmDelete := True;
  FParentFolder := nil;
  FDesktopFolder := nil;
  SelectNewFiles := False;
  DragOnDriveIsMove := True;
  FHiddenCount := 0;
  FFilteredCount := 0;
  FNotRelative := False;

  FFileOperator := TFileOperator.Create(Self);
  FFileOperator.Flags := [foAllowUndo, foNoConfirmMkDir];
  FDirOK := True;
  FPath := '';

  FDiscMonitor := nil;

  {ChangeTimer: }
  if FChangeInterval = 0 then FChangeInterval := MSecsPerSec;
  FChangeTimer := TTimer.Create(Self);
  FChangeTimer.Interval := FChangeInterval;
  FChangeTimer.Enabled := False;
  FChangeTimer.OnTimer := TimerOnTimer;

  {Drag&drop:}
  FConfirmOverwrite := True;
  DDLinkOnExeDrag := True;

  with DragDropFilesEx do
  begin
    SourceEffects := DragSourceEffects;
    TargetEffects := [deCopy, deMove, deLink];

    ShellExtensions.DragDropHandler := True;
    ShellExtensions.DropHandler := True;
  end;

  FLastPath := nil;
end; {Create}

destructor TDirView.Destroy;
begin
  if Assigned(PIDLRecycle) then FreePIDL(PIDLRecycle);

  FLastPath.Free;
  FInfoCacheList.Free;
  FFileOperator.Free;
  FChangeTimer.Free;

  inherited Destroy;
  FPath := '';
end; {Destroy}

procedure TDirView.WMDestroy(var Msg: TWMDestroy);
begin
  Selected := nil;
  ClearItems;
  TerminateThreads;
  inherited;
end; {WMDestroy}

procedure TDirView.CMRecreateWnd(var Message: TMessage);
begin
  // see comment in TDirView.StopIconUpdateThread
  if not (csRecreating in ControlState) then
  begin
    inherited;
  end;
end;

procedure TDirView.TerminateThreads;
begin
  StopIconUpdateThread;
  StopWatchThread;
  if Assigned(FDiscMonitor) then
  begin
    FDiscMonitor.Free;
    FDiscMonitor := nil;
  end;
end; {TerminateThreads}

function TDirView.GetHomeDirectory: string;
begin
  if FHomeDirectory <> '' then Result := FHomeDirectory
    else
  begin
    Result := UserDocumentDirectory;
    // in rare case the CSIDL_PERSONAL cannot be resolved
    if Result = '' then
    begin
      Result := DriveInfo.AnyValidPath;
    end;
  end;
end; { GetHomeDirectory }

function TDirView.GetIsRoot: Boolean;
begin
  Result := IsRootPath(Path);
end;

function TDirView.GetPath: string;
begin
  Result := FPath;
end;

procedure TDirView.PathChanged;
var
  Expanded: string;
begin
  inherited;

  // make sure to use PathName as Path maybe just X: what
  // ExpandFileName resolves to current working directory
  // on the drive, not to root path
  Expanded := ExpandFileName(PathName);
  if not Assigned(FLastPath) then
  begin
    FLastPath := TDictionary<string, string>.Create;
  end;
  FLastPath.AddOrSetValue(DriveInfo.GetDriveKey(Expanded), Expanded);
end;

procedure TDirView.SetPath(Value: string);
begin
  // do checks before passing directory to drive view, because
  // it would truncate non-existing directory to first superior existing
  Value := ReplaceStr(Value, '/', '\');

  CheckCanOpenDirectory(Value);

  if Assigned(FDriveView) and
     (FDriveView.Directory <> Value) then
  begin
    FDriveView.Directory := Value;
  end
    else
  if FPath <> Value then
  try
    while ExcludeTrailingPathDelimiter(Value) <> Value do
    begin
      Value := ExcludeTrailingPathDelimiter(Value);
    end;
    PathChanging(not FNotRelative);
    FPath := Value;
    Load(True);
  finally
    PathChanged;
  end;
end;

procedure TDirView.OpenFallbackPath(Value: string);
var
  APath: string;
begin
  while True do
  begin
    APath := ExtractFileDir(Value);
    if (APath = '') or (APath = Value) then
    begin
      Break;
    end
      else
    begin
      try
        Path := APath;
        Break;
      except
        Value := APath;
      end;
    end;
  end;
end;

procedure TDirView.SetLoadEnabled(Value: Boolean);
begin
  if Value <> LoadEnabled then
  begin
    FLoadEnabled := Enabled;
    if LoadEnabled and Dirty then
    begin
      if Items.Count > 100 then Reload2
        else Reload(True);
    end;
  end;
end; {SetLoadEnabled}

function TDirView.GetPathName: string;
begin
  if IsRoot then Result := IncludeTrailingBackslash(Path)
    else Result := Path;
end; {GetPathName}

function TDirView.GetFileRec(Index: Integer): PFileRec;
begin
  if Index > Pred(Items.Count) then Result := nil
    else Result := Items[index].Data;
end; {GetFileRec}

function TDirView.HiddenCount: Integer;
begin
  Result := FHiddenCount;
end;

function TDirView.FilteredCount: Integer;
begin
  Result := FFilteredCount;
end;

function TDirView.AddItem(SRec: SysUtils.TSearchRec): TListItem;
var
  PItem: PFileRec;
  Item: TListItem;
begin
  Item := TListItem.Create(Items);
  New(PItem);
  with PItem^ do
  begin
    // must be set as soon as possible, at least before Caption is set,
    // because if come column is "autosized" setting Caption invokes some callbacks
    Item.Data := PItem;

    FileName := SRec.Name;
    FileExt := UpperCase(ExtractFileExt(Srec.Name));
    FileExt := Copy(FileExt, 2, Length(FileExt) - 1);
    DisplayName := FileName;
{$WARNINGS OFF}
    Attr := SRec.FindData.dwFileAttributes;
{$WARNINGS ON}
    IsParentDir := False;
    IsDirectory := ((Attr and SysUtils.faDirectory) <> 0);
    IsRecycleBin := IsDirectory and (Length(Path) = 2) and
      Bool(Attr and SysUtils.faSysFile) and
      ((UpperCase(FileName) = 'RECYCLED') or (UpperCase(FileName) = 'RECYCLER'));
    if not IsDirectory then Size := SizeFromSRec(SRec)
      else Size := -1;

{$WARNINGS OFF}
    FileTime := SRec.FindData.ftLastWriteTime;
{$WARNINGS ON}
    Empty := True;
    IconEmpty := True;
    if Size > 0 then Inc(FFilesSize, Size);
    PIDL := nil;

    // Need to add before assigning to .Caption and .OverlayIndex,
    // as the setters these call back to owning view.
    // Assignment is redundant
    Item := Items.AddItem(Item);

    if not Self.IsRecycleBin then Item.Caption := SRec.Name;
    if FileExt = 'LNK' then Item.OverlayIndex := 1;
  end;
  if SelectNewFiles then Item.Selected := True;

  Result := Item;
end; {AddItem}

procedure TDirView.AddParentDirItem;
var
  PItem: PFileRec;
  Item: TListItem;
  SRec: SysUtils.TSearchRec;
begin
  FHasParentDir := True;
  Item := Items.Add;
  New(PItem);
  if FindFirst(ApiPath(FPath), faAnyFile, SRec) = 0 then
    FindClose(SRec);
  with PItem^ do
  begin
    Item.Data := PItem;
    FileName := '..';
    FileExt := '';
    DisplayName := '..';
    Attr := SRec.Attr;
    IsDirectory := True;
    IsRecycleBin := False;
    IsParentDir := True;
    Size := -1;

    Item.Caption := '..';

{$WARNINGS OFF}
    FileTime := SRec.FindData.ftLastWriteTime;
{$WARNINGS ON}
    Empty := True;
    IconEmpty := False;
    PIDL := nil;

    ImageIndex := StdDirIcon;

    TypeName := SParentDir;
    Empty := False;
  end;
end; {AddParentDirItem}

procedure TDirView.LoadFromRecycleBin(Dir: string);
var
  PIDLRecycleLocal: PItemIDList;
  PCurrList: PItemIDList;
  FQPIDL: PItemIDList;
  EnumList: IEnumIDList;
  Fetched: ULONG;
  SRec: SysUtils.TSearchRec;
  DisplayName: string;
  FullPath: string;
  NewItem: TListItem;
  FileRec: PFileRec;
  FileInfo: TSHFileInfo;
  DosError: Integer;

begin
  if not Assigned(iRecycleFolder) then
  begin
    PIDLRecycleLocal := nil;
    try
      OLECheck(SHGetSpecialFolderLocation(Self.Handle,
        CSIDL_BITBUCKET, PIDLRecycleLocal));
      PIDLRecycle := PIDL_Concatenate(nil, PIDLRecycleLocal);

      if not SUCCEEDED(FDesktopFolder.BindToObject(PIDLRecycle, nil,
        IID_IShellFolder, Pointer(iRecycleFolder))) then Exit;
    finally
      if Assigned(PIDLRecycleLocal) then
        FreePIDL(PIDLRecycleLocal);
    end;
  end;
  FParentFolder := iRecycleFolder;

  if AddParentDir then AddParentDirItem;

  FHiddenCount := 0;
  FFilteredCount := 0;

  if SUCCEEDED(iRecycleFolder.EnumObjects(Self.Handle,
    SHCONTF_FOLDERS or SHCONTF_NONFOLDERS or SHCONTF_INCLUDEHIDDEN, EnumList)) then
  begin
     while (EnumList.Next(1, PCurrList, Fetched) = S_OK) and not AbortLoading do
     begin
       if Assigned(PCurrList) then
       try
         FQPIDL := PIDL_Concatenate(PIDLRecycle, PCurrList);

         {Physical filename:}
         SetLength(FullPath, MAX_PATH);
         if shGetPathFromIDList(FQPIDL, PChar(FullPath)) then
           SetLength(FullPath, StrLen(PChar(FullPath)));

         {Filesize, attributes and -date:}
         DosError := FindFirst(ApiPath(FullPath), faAnyFile, SRec);
         FindClose(Srec);
         SRec.Name := ExtractFilePath(FullPath) + SRec.Name;

         {Displayname:}
         GetShellDisplayName(iRecycleFolder, PCurrList, SHGDN_FORPARSING, DisplayName);

         if (DosError = 0) and
            (((SRec.Attr and faDirectory) <> 0) or
             FileMatches(DisplayName, SRec)) then
         begin
           {Filetype and icon:}
           SHGetFileInfo(PChar(FQPIDL), 0, FileInfo, SizeOf(FileInfo),
            SHGFI_PIDL or SHGFI_TYPENAME or SHGFI_SYSICONINDEX);

           NewItem := AddItem(Srec);
           NewItem.Caption := DisplayName;
           FileRec := NewItem.Data;
           FileRec^.Empty := False;
           FileRec^.IconEmpty := False;
           FileRec^.DisplayName := DisplayName;
           FileRec^.PIDL := FQPIDL;
           FileRec^.TypeName := FileInfo.szTypeName;
           if FileRec^.Typename = EmptyStr then
             FileRec^.TypeName := Format(STextFileExt, [FileRec.FileExt]);
           FileRec^.ImageIndex := FileInfo.iIcon;
         end
           else
         begin
           FreePIDL(FQPIDL);
         end;

         FreePIDL(PCurrList);
       except
         if Assigned(PCurrList) then
         try
           FreePIDL(PCurrList);
         except
         end;
       end;
     end; {While EnumList ...}
  end;
end; {LoadFromRecycleBin}

function TDirView.GetShellFolder(Dir: string): iShellFolder;
var
  Eaten: ULONG;
  Attr: ULONG;
  NewPIDL: PItemIDList;
begin
  Result := nil;
  if not Assigned(FDesktopFolder) then
    SHGetDesktopFolder(FDesktopFolder);

  if Assigned(FDesktopFolder) then
  begin
    Attr := 0;
    if Succeeded(FDesktopFolder.ParseDisplayName(
         ParentForm.Handle, nil, PChar(Dir), Eaten, NewPIDL, Attr)) then
    begin
      try
        assert(Assigned(NewPIDL));
        FDesktopFolder.BindToObject(NewPIDL, nil, IID_IShellFolder, Pointer(Result));
        Assert(Assigned(Result));
      finally
        FreePIDL(NewPIDL);
      end;
    end;
  end;
end; {GetShellFolder}

function TDirView.ItemIsDirectory(Item: TListItem): Boolean;
begin
  Result :=
    (Assigned(Item) and Assigned(Item.Data) and
    PFileRec(Item.Data)^.IsDirectory);
end;

function TDirView.ItemIsFile(Item: TListItem): Boolean;
begin
  Result :=
    (Assigned(Item) and Assigned(Item.Data) and
     (not PFileRec(Item.Data)^.IsParentDir));
end;

function TDirView.ItemIsParentDirectory(Item: TListItem): Boolean;
begin
  Result :=
    (Assigned(Item) and Assigned(Item.Data) and
    PFileRec(Item.Data)^.IsParentDir);
end;

function TDirView.ItemIsRecycleBin(Item: TListItem): Boolean;
begin
  Result := (Assigned(Item) and Assigned(Item.Data) and
    PFileRec(Item.Data)^.IsRecycleBin);
end;

function TDirView.ItemMatchesFilter(Item: TListItem; const Filter: TFileFilter): Boolean;
var
  FileRec: PFileRec;
begin
  Assert(Assigned(Item) and Assigned(Item.Data));
  FileRec := PFileRec(Item.Data);

  Result :=
    ((Filter.Masks = '') or
     FileNameMatchesMasks(FileRec^.FileName, FileRec^.IsDirectory,
       FileRec^.Size, FileTimeToDateTime(FileRec^.FileTime), Filter.Masks, False) or
     (FileRec^.IsDirectory and Filter.Directories and
      FileNameMatchesMasks(FileRec^.FileName, False,
       FileRec^.Size, FileTimeToDateTime(FileRec^.FileTime), Filter.Masks, False)));
end;

function TDirView.FileMatches(FileName: string; const SearchRec: TSearchRec): Boolean;
var
  Directory: Boolean;
  FileSize: Int64;
begin
  Result := (ShowHiddenFiles or ((SearchRec.Attr and SysUtils.faHidden) = 0));
  if not Result then
  begin
    Inc(FHiddenCount);
  end
    else
  if Mask <> '' then
  begin
    Directory := ((SearchRec.Attr and faDirectory) <> 0);
    if Directory then FileSize := 0
      else FileSize := SizeFromSRec(SearchRec);

    Result :=
      FileNameMatchesMasks(
        FileName,
        Directory,
        FileSize,
        FileTimeToDateTime(SearchRec.FindData.ftLastWriteTime),
        Mask, True);

    if not Result then
    begin
      Inc(FFilteredCount);
    end;
  end;
end;

function TDirView.ItemOverlayIndexes(Item: TListItem): Word;
begin
  Result := inherited ItemOverlayIndexes(Item);
  if Assigned(Item) and Assigned(Item.Data) then
  begin
    if PFileRec(Item.Data)^.IsParentDir then
      Inc(Result, oiDirUp);
  end;
end;

procedure TDirView.Load(DoFocusSomething: Boolean);
begin
  try
    StopIconUpdateThread;
    StopWatchThread;

    FChangeTimer.Enabled  := False;
    FChangeTimer.Interval := 0;

    inherited;
  finally
    if DirOK and not AbortLoading then
    begin
      if FUseIconUpdateThread and (not IsRecycleBin) then
        StartIconUpdateThread;
      StartWatchThread;
    end;
  end;
end;

procedure TDirView.LoadFiles;
var
  SRec: SysUtils.TSearchRec;
  DosError: Integer;
  DirsCount: Integer;
  SelTreeNode: TTreeNode;
  Node: TTreeNode;
  Drive: string;
begin
  FHiddenCount := 0;
  FFilteredCount := 0;

  try
    if Length(FPath) > 0 then
    begin
      Drive := DriveInfo.GetDriveKey(FPath);
      DriveInfo.ReadDriveStatus(Drive, dsSize);
      FDriveType := DriveInfo.Get(Drive).DriveType;
      FDirOK := DriveInfo.Get(Drive).DriveReady and DirectoryExists(FPath);
    end
      else
    begin
      FDriveType := DRIVE_UNKNOWN;
      FDirOK := False;
    end;

    if DirOK then
    begin
      if Assigned(FDriveView) then
        SelTreeNode := TDriveView(FDriveView).FindNodeToPath(FPath)
        else SelTreeNode := nil;

      if Assigned(FDriveView) and Assigned(SelTreeNode) then
          FIsRecycleBin := TNodeData(SelTreeNode.Data).IsRecycleBin
        else
      FIsRecycleBin :=
        (Uppercase(Copy(FPath, 2, 10)) = ':\RECYCLED') or
        (Uppercase(Copy(FPath, 2, 10)) = ':\RECYCLER');

      if not Assigned(FDesktopFolder) then
        SHGetDesktopFolder(FDesktopFolder);

      if IsRecycleBin then LoadFromRecycleBin(Path)
        else
      begin
        FParentFolder := GetShellFolder(PathName);

        DosError := SysUtils.FindFirst(ApiPath(IncludeTrailingPathDelimiter(FPath) + '*.*'),
          FileAttr, SRec);
        while (DosError = 0) and (not AbortLoading) do
        begin
          if (SRec.Attr and faDirectory) = 0 then
          begin
            if FileMatches(SRec.Name, SRec) then
            begin
              AddItem(SRec);
            end;
          end;
          DosError := FindNext(SRec);
        end;
        SysUtils.FindClose(SRec);

        if AddParentDir and (not IsRoot) then
        begin
          AddParentDirItem;
        end;

        {Search for directories:}
        DirsCount := 0;
        DosError := SysUtils.FindFirst(ApiPath(IncludeTrailingPathDelimiter(FPath) + '*.*'),
          DirAttrMask, SRec);
        while (DosError = 0) and (not AbortLoading) do
        begin
          if (SRec.Name <> '.') and (SRec.Name <> '..') and
             ((Srec.Attr and faDirectory) <> 0) then
          begin
            Inc(DirsCount);

            if FileMatches(SRec.Name, SRec) then
            begin
              AddItem(Srec);
            end;
          end;
          DosError := FindNext(SRec);
        end;
        SysUtils.FindClose(SRec);

        {Update TDriveView's subdir indicator:}
        if Assigned(FDriveView) and (FDriveType = DRIVE_REMOTE) then
          with TDriveView(FDriveView) do
          begin
            Node := FindNodeToPath(PathName);
            if Assigned(Node) and Assigned(Node.Data) and
               not TNodeData(Node.Data).Scanned then
            begin
              if DirsCount = 0 then
              begin
                Node.HasChildren := False;
                TNodeData(Node.Data).Scanned := True;
              end;
            end;
          end;
      end; {not isRecycleBin}
    end
      else FIsRecycleBin := False;

  finally
    //if Assigned(Animate) then Animate.Free;
    FInfoCacheList.Sort(CompareInfoCacheItems);
  end; {Finally}
end;

procedure TDirView.Reload2;
type
  PEFileRec = ^TEFileRec;
  TEFileRec = record
    iSize: Int64;
    iAttr: Integer;
    iFileTime: TFileTime;
    iIndex: Integer;
  end;
var
  Index: Integer;
  EItems: TStringList;
  FItems: TStringList;
  NewItems: TStringList;
  Srec: SysUtils.TSearchRec;
  DosError: Integer;
  PSrec: ^SysUtils.TSearchRec;
  Dummy: Integer;
  ItemIndex: Integer;
  AnyUpdate: Boolean;
  PUpdate: Boolean;
  PEFile: PEFileRec;
  SaveCursor: TCursor;
  FSize: Int64;
  FocusedIsVisible: Boolean;
  R: TRect;
begin
  if (not Loading) and LoadEnabled then
  begin
    if IsRecycleBin then Reload(True)
      else
    begin
      if not DirectoryExists(Path) then
      begin
        ClearItems;
        FDirOK := False;
        FDirty := False;
      end
        else
      begin
        if Assigned(ItemFocused) then
        begin
          R := ItemFocused.DisplayRect(drBounds);
          // btw, we use vsReport only, nothing else was tested
          Assert(ViewStyle = vsReport);
          case ViewStyle of
            vsReport:
              FocusedIsVisible := (TopItem.Index <= ItemFocused.Index) and
                (ItemFocused.Index < TopItem.Index + VisibleRowCount);

            vsList:
              // do not know how to implement that
              FocusedIsVisible := False;

            else // vsIcon and vsSmallIcon
              FocusedIsVisible :=
                IntersectRect(R,
                  Classes.Rect(ViewOrigin, Point(ViewOrigin.X + ClientWidth, ViewOrigin.Y + ClientHeight)),
                  ItemFocused.DisplayRect(drBounds));
          end;
        end
          else FocusedIsVisible := False; // shut up

        SaveCursor := Screen.Cursor;
        Screen.Cursor := crHourGlass;
        FChangeTimer.Enabled  := False;
        FChangeTimer.Interval := 0;

        EItems := TStringlist.Create;
        EItems.CaseSensitive := True; // We want to reflect changes in file name case
        FItems := TStringlist.Create;
        FItems.CaseSensitive := True;
        NewItems := TStringlist.Create;

        PUpdate := False;
        AnyUpdate := False;

        FHiddenCount := 0;
        FFilteredCount := 0;

        try
          {Store existing files and directories:}
          for Index := 0 to Items.Count - 1 do
          begin
            New(PEFile);
            with PFileRec(Items[Index].Data)^ do
            begin
              PEFile^.iSize := Size;
              PEFile^.iAttr := Attr;
              PEFile^.iFileTime := FileTime;
              PEFile^.iIndex := Index;
            end;
            EItems.AddObject(PFileRec(Items[Index].Data)^.FileName, Pointer(PEFile));
          end;
          EItems.Sort;

          DosError := SysUtils.FindFirst(ApiPath(IncludeTrailingPathDelimiter(FPath) + '*.*'),
            FileAttr, SRec);
          while DosError = 0 do
          begin
            if (SRec.Attr and faDirectory) = 0 then
            begin
              if FileMatches(SRec.Name, SRec) then
              begin
                ItemIndex := -1;
                if not EItems.Find(SRec.Name, ItemIndex) then
                begin
                  New(PSrec);
                  PSRec^ := SRec;
                  NewItems.AddObject(SRec.Name, Pointer(PSrec));
                  FItems.Add(Srec.Name);
                end
                  else
                begin
                  FSize := SizeFromSRec(SRec);
                  with PEFileRec(EItems.Objects[ItemIndex])^ do
{$WARNINGS OFF}
                  if (iSize <> FSize) or (iAttr <> SRec.Attr) or
                     not CompareMem(@iFileTime, @SRec.FindData.ftLastWriteTime,
                        SizeOf(iFileTime)) Then
{$WARNINGS ON}
                  begin
                    with PFileRec(Items[iIndex].Data)^ do
                    begin
                      Dec(FFilesSize, Size);
                      Inc(FFilesSize, FSize);
                      if Items[iIndex].Selected then
                      begin
                        Dec(FFilesSelSize, Size);
                        Inc(FFilesSelSize, FSize);
                      end;

                      Size := FSize;
                      Attr := SRec.Attr;
{$WARNINGS OFF}
                      FileTime := SRec.FindData.ftLastWriteTime;
{$WARNINGS ON}
                    end;
                    // alternative to TListItem.Update (which causes flicker)
                    R := Items[iIndex].DisplayRect(drBounds);
                    InvalidateRect(Handle, @R, True);
                    AnyUpdate := True;
                  end;
                  FItems.Add(Srec.Name);
                end;
              end;
            end;
            DosError := FindNext(Srec);
          end;
          SysUtils.FindClose(Srec);

          {Search new directories:}
          DosError := SysUtils.FindFirst(ApiPath(FPath + '\*.*'), DirAttrMask, SRec);
          while DosError = 0 do
          begin
            if (Srec.Attr and faDirectory) <> 0 then
            begin
              if (SRec.Name <> '.') and (SRec.Name <> '..') then
              begin
                if not EItems.Find(SRec.Name, ItemIndex) then
                begin
                  if FileMatches(SRec.Name, SRec) then
                  begin
                    New(PSrec);
                    PSrec^ := SRec;
                    NewItems.AddObject(Srec.Name, Pointer(PSrec));
                    FItems.Add(SRec.Name);
                  end;
                end
                  else
                begin
                  FItems.Add(SRec.Name);
                end;
              end
                else
              begin
                FItems.Add(SRec.Name);
              end;
            end;
            DosError := FindNext(SRec);
          end;
          SysUtils.FindClose(SRec);

          {Check wether displayed Items still exists:}
          FItems.Sort;
          for Index := Items.Count - 1 downto 0 do
          begin
            if not FItems.Find(PFileRec(Items[Index].Data)^.FileName, Dummy) then
            begin
              if not PUpdate then
              begin
                PUpdate := True;
                Items.BeginUpdate;
              end;
              AnyUpdate := True;

              with PFileRec(Items[Index].Data)^ do
              begin
                Dec(FFilesSize, Size);
                // No need to decrease FFilesSelSize here as LVIF_STATE/deselect
                // is called for item being deleted
              end;

              Items[Index].Delete;
            end;
          end;

        finally
          try
            for Index := 0 to EItems.Count - 1  do
              Dispose(PEFileRec(EItems.Objects[Index]));
            EItems.Free;
            FItems.Free;
            for Index := 0 to NewItems.Count - 1 do
            begin
              if not PUpdate then
              begin
                PUpdate := True;
                Items.BeginUpdate;
              end;
              AnyUpdate := True;
              PSrec := Pointer(NewItems.Objects[Index]);
              AddItem(PSrec^);
              Dispose(PSrec);
            end;
            NewItems.Free;
            // if we are sorted by name and there were only updates to existing
            // items, there is no need for sorting
            if PUpdate or
               (AnyUpdate and (DirColProperties.SortDirColumn <> dvName)) then
            begin
              SortItems;
            end;
            if PUpdate then
              Items.EndUpdate;
          finally
            FDirOK := True;
            FDirty := false;
            if FUseIconUpdateThread and (not FisRecycleBin) then
              StartIconUpdateThread;
            StartWatchThread;
            // make focused item visible, only if it was before
            if FocusedIsVisible and Assigned(ItemFocused) then
              ItemFocused.MakeVisible(False);

            DoUpdateStatusBar;

            Screen.Cursor := SaveCursor;
          end;
        end; {Finally}
      end;

      if Assigned(FDriveView) then
      begin
        TDriveView(FDriveView).ValidateCurrentDirectoryIfNotMonitoring;
      end;
    end;
  end;
end; {Reload2}

procedure TDirView.PerformItemDragDropOperation(Item: TListItem; Effect: Integer; Paste: Boolean);
var
  TargetPath: string;
  RenameOnCollision: Boolean;
begin
  TargetPath := '';
  RenameOnCollision := False;

  if Assigned(Item) then
  begin
    if Assigned(Item.Data) then
    begin
      if ItemIsParentDirectory(Item) then
        TargetPath := ExcludeTrailingPathDelimiter(ExtractFilePath(Path))
      else
        TargetPath := IncludeTrailingPathDelimiter(PathName) + ItemFileName(Item);
    end;
  end
    else
  begin
    TargetPath := PathName;
    RenameOnCollision := DDOwnerIsSource and (Effect = DROPEFFECT_COPY);
  end;

  if TargetPath <> '' then
    PerformDragDropFileOperation(TargetPath, Effect, RenameOnCollision, Paste);
end;

procedure TDirView.ReLoad(CacheIcons: Boolean);
begin
  if not FLoadEnabled then FDirty := True
    else inherited;
end; {ReLoad}

procedure TDirView.ClearIconCache;
begin
  if Assigned(FInfoCacheList) then
    FInfoCacheList.Clear;
end; {ClearIconCache}

function TDirView.FormatFileTime(FileTime: TFileTime): string;
begin
  Result := FormatDateTime(DateTimeFormatStr,
    FileTimeToDateTime(FileTime));
end; {FormatFileTime}

function TDirView.GetAttrString(Attr: Integer): string;
const
  Attrs: array[1..5] of Integer =
    (FILE_ATTRIBUTE_COMPRESSED, FILE_ATTRIBUTE_ARCHIVE,
     FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_HIDDEN,
     FILE_ATTRIBUTE_READONLY);
  AttrChars: array[1..5] of Char = ('c', 'a', 's', 'h', 'r');
var
  Index: Integer;
  LowBound: Integer;
begin
  Result := '';
  if Attr <> 0 then
  begin
    LowBound := Low(Attrs);

    for Index := LowBound to High(Attrs) do
      if (Attr and Attrs[Index] <> 0) then
        Result := Result + AttrChars[Index]
      else
        Result := Result;
  end;
end; {GetAttrString}

function TDirView.GetFileInfo(
  pszPath: LPCWSTR; dwFileAttributes: DWORD; var psfi: TSHFileInfoW; cbFileInfo, uFlags: UINT): DWORD_PTR;
begin
  if TimeoutShellIconRetrieval then
  begin
     Result := SHGetFileInfoWithTimeout(pszPath, dwFileAttributes, psfi, cbFileInfo, uFlags, MSecsPerSec div 4);
  end
    else
  begin
    Result := SHGetFileInfo(pszPath, dwFileAttributes, psfi, cbFileInfo, uFlags);
  end;
end;

procedure TDirView.GetDisplayData(Item: TListItem; FetchIcon: Boolean);
var
  FileInfo: TShFileInfo;
  Index: Integer;
  PExtItem: PInfoCache;
  CacheItem: TInfoCache;
  IsSpecialExt: Boolean;
  ForceByName: Boolean;
  Eaten: ULONG;
  shAttr: ULONG;
  FileIconForName, FullName: string;
begin
  Assert(Assigned(Item) and Assigned(Item.Data));
  with PFileRec(Item.Data)^ do
  begin
    IsSpecialExt := MatchesFileExt(FileExt, SpecialExtensions);
    if FUseIconCache and not IsSpecialExt and not IsDirectory then
    begin
      CacheItem.FileExt := FileExt;
      Index := FInfoCacheList.FindSequential(Addr(CacheItem), CompareInfoCacheItems);
      if Index >= 0 then
      begin
        TypeName := PInfoCache(FInfoCacheList[Index])^.TypeName;
        ImageIndex  := PInfoCache(FInfoCacheList[Index])^.ImageIndex;
        Empty := False;
        IconEmpty := False;
      end;
    end;

    FetchIcon := IconEmpty and (FetchIcon or not IsSpecialExt);

    if Empty or FetchIcon then
    begin
      if FetchIcon then
      begin
        {Fetch the Item FQ-PIDL:}
        if not Assigned(PIDL) and IsSpecialExt then
        begin
          try
            ShAttr := 0;
            FDesktopFolder.ParseDisplayName(ParentForm.Handle, nil,
              PChar(FPath + '\' + FileName), Eaten, PIDL, ShAttr);
          except
          end;
        end;

        if IsDirectory then
        begin
          if FDriveType = DRIVE_FIXED then
          begin
            try
              {Retrieve icon and typename for the directory}
              if Assigned(PIDL) then
              begin
                SHGetFileInfo(PChar(PIDL), 0, FileInfo, SizeOf(FileInfo),
                  SHGFI_TYPENAME or SHGFI_SYSICONINDEX or SHGFI_PIDL)
              end
                else
              begin
                SHGetFileInfo(PChar(FPath + '\' + FileName), 0, FileInfo, SizeOf(FileInfo),
                  SHGFI_TYPENAME or SHGFI_SYSICONINDEX);
              end;

              if (FileInfo.iIcon <= 0) or (FileInfo.iIcon > SmallImages.Count) then
              begin
                {Invalid icon returned: retry with access file attribute flag:}
                SHGetFileInfo(PChar(FPath + '\' + FileName), FILE_ATTRIBUTE_DIRECTORY,
                  FileInfo, SizeOf(FileInfo),
                  SHGFI_TYPENAME or SHGFI_SYSICONINDEX or SHGFI_USEFILEATTRIBUTES);
              end;
              TypeName := FileInfo.szTypeName;
              if FetchIcon then
              begin
                ImageIndex := FileInfo.iIcon;
                IconEmpty := False;
              end;
            {Capture exceptions generated by the shell}
            except
              ImageIndex := StdDirIcon;
              IconEmpty := False;
            end; {Except}
          end
            else
          begin
            TypeName := StdDirTypeName;
            ImageIndex := StdDirIcon;
            IconEmpty  := False;
          end;
        end
          else
        begin
          {Retrieve icon and typename for the file}
          try
            ForceByName := False;
            FullName := FPath + '\' + FileName;
            FileIconForName := FullName;
            if Assigned(OnFileIconForName) then
            begin
              OnFileIconForName(Self, Item, FileIconForName);
              ForceByName := (FileIconForName <> FullName);
            end;
            if (not ForceByName) and Assigned(PIDL) then
            begin
              // Files with PIDL are typically .exe files.
              // It may take long to retrieve an icon from exe file.
              // We typically do not get here, now that we have UseIconUpdateThread enabled.
              if GetFileInfo(
                   PChar(PIDL), FILE_ATTRIBUTE_NORMAL, FileInfo, SizeOf(FileInfo),
                   SHGFI_TYPENAME or SHGFI_USEFILEATTRIBUTES or SHGFI_SYSICONINDEX or SHGFI_PIDL) = 0 then
              begin
                FileInfo.szTypeName[0] := #0;
                FileInfo.iIcon := DefaultExeIcon;
              end;
            end
              else
            begin
              GetFileInfo(PChar(FileIconForName), FILE_ATTRIBUTE_NORMAL, FileInfo, SizeOf(FileInfo),
                SHGFI_TYPENAME or SHGFI_USEFILEATTRIBUTES or SHGFI_SYSICONINDEX);
            end;

            TypeName := FileInfo.szTypeName;
            ImageIndex := FileInfo.iIcon;
            IconEmpty := False;

          {Capture exceptions generated by the shell}
          except
            ImageIndex := UnKnownFileIcon;
            IconEmpty := False;
          end; {Except}
        end;

        if (Length(TypeName) > 0) then
        begin
          {Fill FileInfoCache:}
          if FUseIconCache and not IsSpecialExt and not IconEmpty and not IsDirectory then
          begin
            GetMem(PExtItem, SizeOf(TInfoCache));
            PExtItem.FileExt := FileExt;
            PExtItem.TypeName := TypeName;
            PExtItem.ImageIndex := ImageIndex;
            FInfoCacheList.Add(PExtItem);
          end;
        end
          else TypeName := Format(STextFileExt, [FileExt]);
      end {If FetchIcon}
        else
      begin
        try
          if IsDirectory then
            SHGetFileInfo(PChar(FPath + '\' + FileName), FILE_ATTRIBUTE_DIRECTORY, FileInfo, SizeOf(FileInfo),
              SHGFI_TYPENAME or SHGFI_USEFILEATTRIBUTES)
          else
            SHGetFileInfo(PChar(FPath + '\' + FileName), FILE_ATTRIBUTE_NORMAL, FileInfo, SizeOf(FileInfo),
              SHGFI_TYPENAME or SHGFI_USEFILEATTRIBUTES);
          TypeName := FileInfo.szTypeName;
        except
          {Capture exceptions generated by the shell}
          TypeName := '';
        end;

        if IconEmpty then
        begin
          if FileExt = ExeExtension then ImageIndex := DefaultExeIcon
            else ImageIndex  := UnKnownFileIcon;
        end;
      end;
      Empty := False;
    end;
  end;
end; {GetDisplayData}

function TDirView.GetDirOK: Boolean;
begin
  Result := FDirOK;
end;

function TDirView.ItemFullFileName(Item: TListItem): string;
begin
  if Assigned(Item) and Assigned(Item.Data) then
  begin
    if not IsRecycleBin then
    begin
      if PFileRec(Item.Data)^.IsParentDir then
      begin
        Result := ExcludeTrailingBackslash(ExtractFilePath(FPath));
      end
        else
      begin
        Result := FPath + '\' + PFileRec(Item.Data)^.FileName;
      end;
    end
      else
    Result := PFileRec(Item.Data)^.FileName;
  end
    else
  Result := EmptyStr;
end; {ItemFullFileName}

function TDirView.ItemFileNameOnly(Item: TListItem): string;
begin
  Assert(Assigned(Item) and Assigned(Item.Data));
  Result := PFileRec(Item.Data)^.FileName;
  SetLength(Result, Length(Result) - Length(ItemFileExt(Item)));
end; {ItemFileNameOnly}

function TDirView.ItemFileExt(Item: TListItem): string;
begin
  Assert(Assigned(Item) and Assigned(Item.Data));
  Result := ExtractFileExt(PFileRec(Item.Data)^.FileName);
end; {ItemFileExt}

function CompareFileType(I1, I2: TListItem; P1, P2: PFileRec): Integer;
var
  Key1, Key2: string;
begin
  if P1.Empty then TDirView(I1.ListView).GetDisplayData(I1, False);
  if P2.Empty then TDirView(I2.ListView).GetDisplayData(I2, False);
  if P1.IsDirectory then
  begin
    Key1 := P1.TypeName + ' ' + P1.DisplayName;
    Key2 := P2.TypeName + ' ' + P2.DisplayName;
  end
    else
  begin
    Key1 := P1.TypeName + ' ' + P1.FileExt + ' ' + P1.DisplayName;
    Key2 := P2.TypeName + ' ' + P2.FileExt + ' ' + P2.DisplayName;
  end;
  Result := CompareLogicalTextPas(Key1, Key2, TDirView(I1.ListView).NaturalOrderNumericalSorting);
end;

function CompareFileTime(P1, P2: PFileRec): Integer;
var
  Time1, Time2: Int64;
begin
  Time1 := Int64(P1.FileTime.dwHighDateTime) shl 32 + P1.FileTime.dwLowDateTime;
  Time2 := Int64(P2.FileTime.dwHighDateTime) shl 32 + P2.FileTime.dwLowDateTime;
  if Time1 < Time2 then Result := fLess
    else
  if Time1 > Time2 then Result := fGreater
    else Result := fEqual; // fallback
end;

function CompareFile(I1, I2: TListItem; AOwner: TDirView): Integer; stdcall;
var
  ConsiderDirection: Boolean;
  P1, P2: PFileRec;
begin
  ConsiderDirection := True;
  if I1 = I2 then Result := fEqual
    else
  if I1 = nil then Result := fLess
    else
  if I2 = nil then Result := fGreater
    else
  begin
    P1 := PFileRec(I1.Data);
    P2 := PFileRec(I2.Data);
    if P1.isParentDir then
    begin
      Result := fLess;
      ConsiderDirection := False;
    end
      else
    if P2.isParentDir then
    begin
      Result := fGreater;
      ConsiderDirection := False;
    end
      else
    {Directories should always appear "grouped":}
    if P1.isDirectory <> P2.isDirectory then
    begin
      if P1.isDirectory then
      begin
        Result := fLess;
        ConsiderDirection := False;
      end
        else
      begin
        Result := fGreater;
        ConsiderDirection := False;
      end;
    end
      else
    begin
      Result := fEqual;

      case AOwner.DirColProperties.SortDirColumn of
        dvName:
          ; // fallback

        dvSize:
          if P1.Size < P2.Size then Result := fLess
            else
          if P1.Size > P2.Size then Result := fGreater
            else ; // fallback

        dvType:
          Result := CompareFileType(I1, I2, P1, P2);

        dvChanged:
          Result := CompareFileTime(P1, P2);

        dvAttr:
          if P1.Attr < P2.Attr then Result := fLess
            else
          if P1.Attr > P2.Attr then Result := fGreater
            else ; // fallback

        dvExt:
          if not P1.isDirectory then
          begin
            Result := CompareLogicalTextPas(
              P1.FileExt + ' ' + P1.DisplayName, P2.FileExt + ' ' + P2.DisplayName,
              AOwner.NaturalOrderNumericalSorting);
          end
            else ; //fallback

        else
          ; // fallback
      end;

      if Result = fEqual then
      begin
        Result := CompareLogicalTextPas(P1.DisplayName, P2.DisplayName, AOwner.NaturalOrderNumericalSorting)
      end;
    end;
  end;

  if ConsiderDirection and (not AOwner.SortAscending) then
  begin
    Result := -Result;
  end;
end;

procedure TDirView.SortItems;
begin
  if HandleAllocated then
  begin
    StopIconUpdateThread;
    try
      CustomSortItems(@CompareFile);
    finally
      if (not Loading) and FUseIconUpdateThread then
        StartIconUpdateThread;
    end;
  end
end;

procedure TDirView.ValidateFile(Item : TListItem);
var
  Index: Integer;
begin
  if Assigned(Item) and Assigned(Item.Data) then
  begin
    Index := Item.Index;
    if not FileExists(ApiPath(ItemFullFileName(Items[Index]))) then
    begin
      Item.Delete;
    end;
  end;
end; {ValidateFile}

procedure TDirView.ValidateFile(FileName: TFileName);
var
  FilePath: string;
begin
  FilePath := ExcludeTrailingPathDelimiter(ExtractFilePath(FileName));
  if IsRecycleBin then ValidateFile(FindFileItem(FileName))
    else
  if FilePath = Path then
    ValidateFile(FindFileItem(ExtractFileName(FileName)));
end; {ValidateFile}

procedure TDirView.ValidateSelectedFiles;
var
  FileList: TStrings;
  i: Integer;
  ToDelete: Boolean;
  Updating: Boolean;
  Updated: Boolean;
  Item: TListItem;
begin
  if SelCount > 50 then Reload2
    else
  begin
    Updating := False;
    Updated := False;
    FileList := CustomCreateFileList(True, False, True, nil, True);
    try
      for i := 0 to FileList.Count - 1 do
      begin
        Item := TListItem(FileList.Objects[i]);
        if ItemIsDirectory(Item) then
          ToDelete := not DirectoryExists(ApiPath(FileList[i]))
        else
          ToDelete := not FileExists(ApiPath(FileList[i]));

        if ToDelete then
        begin
          if (SelCount > 10) and (not Updating) then
          begin
            Items.BeginUpdate;
            Updating := True;
          end;
          with PFileRec(Item.Data)^ do
          begin
            Dec(FFilesSize, Size);
            // No need to decrease FFilesSelSize here as LVIF_STATE/deselect
            // is called for item being deleted
          end;
          Item.Delete;
          Updated := True;
        end;
      end;
    finally
      if Updating then
        Items.EndUpdate;
      if Updated then
        DoUpdateStatusBar;
      FileList.Free;
    end;
  end;
end; {ValidateSelectedFiles}

procedure TDirView.CreateDirectory(DirName: string);
var
  SRec: SysUtils.TSearchRec;
  Item: TListItem;
begin
  // keep absolute path as is
  if ExtractFileDrive(DirName) = '' then
    DirName := Path + '\' + DirName;

  if WatchForChanges then StopWatchThread;

  if Assigned(FDriveView) then
    TDriveView(FDriveView).StopWatchThread;

  StopIconUpdateThread;
  try
    {create the physical directory:}
    Win32Check(Windows.CreateDirectory(PChar(ApiPath(DirName)), nil));

    if IncludeTrailingBackslash(ExtractFilePath(ExpandFileName(DirName))) =
         IncludeTrailingBackslash(Path) then
    begin
      {Create the TListItem:}
      if FindFirst(ApiPath(DirName), faAnyFile, SRec) = 0 then
      begin
        Item := AddItem(SRec);
        ItemFocused := FindFileItem(GetFileRec(Item.Index)^.FileName);
        SortItems;
        if Assigned(ItemFocused) then
        begin
          ItemFocused.MakeVisible(False);
        end;
      end;
      FindClose(SRec);
    end;

  finally
    if FUseIconUpdateThread then
      StartIconUpdateThread;

    if WatchForChanges then StartWatchThread;
    if Assigned(DriveView) then
      with DriveView do
      begin
        if Assigned(Selected) then
          ValidateDirectory(Selected);
        TDriveView(FDriveView).StartWatchThread;
      end;

  end;
end; {CreateDirectory}

procedure TDirView.DisplayContextMenu(Where: TPoint);
var
  FileList: TStringList;
  Index: Integer;
  Item: TListItem;
  DefDir: string;
  Verb: string;
  PIDLArray: PPIDLArray;
  Count: Integer;
  DiffSelectedPath: Boolean;
  WithEdit: Boolean;
  PIDLRel: PItemIDList;
  PIDLPath: PItemIDList;
  Handled: Boolean;
begin
  GetDir(0, DefDir);
  ChDir(PathName);
  Verb := EmptyStr;
  StopWatchThread;
  try
    try
      if Assigned(OnContextPopup) then
      begin
        Handled := False;
        OnContextPopup(Self, ScreenToClient(Where), Handled);
        if Handled then Abort;
      end;
      if (MarkedCount > 1) and
         ((not Assigned(ItemFocused)) or ItemFocused.Selected) then
      begin
        if FIsRecycleBin then
        begin
          Count := 0;
          GetMem(PIDLArray, SizeOf(PItemIDList) * SelCount);
          try
            FillChar(PIDLArray^, Sizeof(PItemIDList) * SelCount, #0);
            for Index := Selected.Index to Items.Count - 1 do
              if Items[Index].Selected then
              begin
                PIDL_GetRelative(PFileRec(Items[Index].Data)^.PIDL, PIDLPath, PIDLRel);
                FreePIDL(PIDLPath);
                PIDLArray^[Count] := PIDLRel;
                Inc(Count);
              end;

            try
              ShellDisplayContextMenu(ParentForm.Handle, Where, iRecycleFolder, Count,
                PidlArray^[0], False, Verb, False);
            finally
              for Index := 0 to Count - 1 do
                FreePIDL(PIDLArray[Index]);
            end;
          finally
            FreeMem(PIDLArray, Count);
          end;
        end
          else
        begin
          FileList := TStringList.Create;
          CreateFileList(False, True, FileList);
          for Index := 0 to FileList.Count - 1 do
            FileList[Index] := ExtractFileName(FileList[Index]);
          ShellDisplayContextMenu(ParentForm.Handle, Where, PathName,
            FileList, Verb, False);
          FileList.Destroy;
        end;

        {------------ Cut -----------}
        if Verb = shcCut then
        begin
          LastClipBoardOperation := cboCut;
          {Clear items previous marked as cut:}
          Item := GetNextItem(nil, sdAll, [isCut]);
          while Assigned(Item) do
          begin
            Item.Cut := False;
            Item := GetNextItem(Item, sdAll, [isCut]);
          end;
          {Set property cut to TRUE for all selected items:}
          Item := GetNextItem(nil, sdAll, [isSelected]);
          while Assigned(Item) do
          begin
            Item.Cut := True;
            Item := GetNextItem(Item, sdAll, [isSelected]);
          end;
        end
          else
        {----------- Copy -----------}
        if Verb = shcCopy then LastClipBoardOperation := cboCopy
          else
        {----------- Paste ----------}
        if Verb = shcPaste then
            PasteFromClipBoard(ItemFullFileName(Selected))
          else
        if not FIsRecycleBin then Reload2;
      end
        else
      if Assigned(ItemFocused) and Assigned(ItemFocused.Data) then
      begin
        Verb := EmptyStr;
        WithEdit := not FisRecycleBin and CanEdit(ItemFocused);
        LoadEnabled := True;

        if FIsRecycleBin then
        begin
          PIDL_GetRelative(PFileRec(ItemFocused.Data)^.PIDL, PIDLPath, PIDLRel);
          ShellDisplayContextMenu(ParentForm.Handle, Where,
            iRecycleFolder, 1, PIDLRel, False, Verb, False);
          FreePIDL(PIDLRel);
          FreePIDL(PIDLPath);
        end
          else
        begin
          ShellDisplayContextMenu(ParentForm.Handle, Where,
            ItemFullFileName(ItemFocused), WithEdit, Verb,
            not PFileRec(ItemFocused.Data)^.isDirectory);
          LoadEnabled := True;
        end; {not FisRecycleBin}

        {---------- Rename ----------}
        if Verb = shcRename then ItemFocused.EditCaption
          else
        {------------ Cut -----------}
        if Verb = shcCut then
        begin
          LastClipBoardOperation := cboCut;

          Item := GetNextItem(nil, sdAll, [isCut]);
          while Assigned(Item) do
          begin
            Item.Cut := False;
            Item := GetNextItem(ITem, sdAll, [isCut]);
          end;
          ItemFocused.Cut := True;
        end
          else
        {----------- Copy -----------}
        if Verb = shcCopy then LastClipBoardOperation := cboCopy
          else
        {----------- Paste ----------}
        if Verb = shcPaste then
        begin
          if PFileRec(ItemFocused.Data)^.IsDirectory then
          PasteFromClipBoard(ItemFullFileName(ItemFocused));
        end
          else
        if not FIsRecycleBin then Reload2;
      end;
    finally
      ChDir(DefDir);
    end;

    if IsRecycleBin and (Verb <> shcCut) and (Verb <> shcProperties) and (SelCount > 0) then
    begin
      DiffSelectedPath := False;
      for Index := Selected.Index to Items.Count - 1 do
        if ExtractFilePath(PFileRec(Items[Index].Data)^.FileName) <> FPath + '\' then
        begin
          DiffSelectedPath := True;
          Break;
        end;

      if DiffSelectedPath then
      begin
        StartFileDeleteThread;
        Exit;
      end;
    end;

    Sleep(250);
    ValidateSelectedFiles;

  finally
    StartWatchThread;
  end;
end;

procedure TDirView.GetDisplayInfo(ListItem: TListItem;
  var DispInfo: TLVItem);
var
  Value: string;
begin
  Assert(Assigned(ListItem) and Assigned(ListItem.Data));
  with PFileRec(ListItem.Data)^, DispInfo  do
  begin
    {Fetch display data of current file:}
    if Empty then
      GetDisplayData(ListItem, IconEmpty and
        (not FUseIconUpdateThread or
         (ViewStyle <> vsReport)));

    if IconEmpty and
       (not FUseIconUpdateThread or
        (ViewStyle <> vsReport)) and
       ((DispInfo.Mask and LVIF_IMAGE) <> 0) then
      GetDisplayData(ListItem, True);

    {Set IconUpdatethread :}
    if IconEmpty and Assigned(FIconUpdateThread) then
    begin
      if Assigned(TopItem) then
      {Viewstyle is vsReport or vsList:}
        FIconUpdateThread.Index := Self.TopItem.Index
      else
        {Viewstyle is vsIcon or vsSmallIcon:}
        FIconUpdateThread.MaxIndex := ListItem.Index;

      if FIconUpdateThread.Suspended and not FIsRecycleBin then
        FIconUpdateThread.Resume;
    end;

    if (DispInfo.Mask and LVIF_TEXT) <> 0 then
    begin
      Value := '';
      if iSubItem = 0 then Value := DisplayName
        else
      if iSubItem < DirViewColumns then
      begin
        case TDirViewCol(iSubItem) of
          dvSize: {Size:     }
            if not IsDirectory then Value := FormatPanelBytes(Size, FormatSizeBytes);
          dvType: {FileType: }
            Value := TypeName;
          dvChanged: {Date}
            Value := FormatFileTime(FileTime);
          dvAttr: {Attrs:}
            Value := GetAttrString(Attr);
          dvExt:
            Value := FileExt;
        end {Case}
      end; {SubItem}
      StrPLCopy(pszText, Value, cchTextMax - 1);
    end;

    {Set display icon of current file:}
    if (iSubItem = 0) and ((DispInfo.Mask and LVIF_IMAGE) <> 0) then
    begin
      iImage := PFileRec(ListItem.Data).ImageIndex;
      Mask := Mask or LVIF_DI_SETITEM;
    end;
  end; {With PFileRec Do}
  {Mask := Mask Or LVIF_DI_SETITEM; {<== causes flickering display and icons not to be updated on renaming the item}
end;

function TDirView.ItemColor(Item: TListItem): TColor;
begin
  if PFileRec(Item.Data).Attr and FILE_ATTRIBUTE_COMPRESSED <> 0 then
  begin
    if SupportsDarkMode and DarkMode then Result := clSkyBlue
      else Result := clBlue;
  end
    else
  if DimmHiddenFiles and not Item.Selected and
     (PFileRec(Item.Data).Attr and FILE_ATTRIBUTE_HIDDEN <> 0) then
      Result := clGrayText
    else
  Result := clDefaultItemColor;
end;

procedure TDirView.StartFileDeleteThread;
var
  Files: TStringList;
begin
  Files := TStringList.Create;
  try
    CreateFileList(False, True, Files);
    TFileDeleteThread.Create(Files, MaxWaitTimeOut, SignalFileDelete);
  finally
    Files.Free;
  end;
end;

procedure TDirView.StartIconUpdateThread;
begin
  if DirOK then
  begin
    if not Assigned(FIconUpdateThread) then
    begin
      if Items.Count > 0 then
        FIconUpdateThread := TIconUpdateThread.Create(Self);
    end
      else
    begin
      Assert(not FIconUpdateThread.Terminated);
      FIconUpdateThread.Index := 0;
      if ViewStyle = vsReport then
        FIconUpdateThread.Resume;
    end;
  end;
end; {StartIconUpdateThread}

procedure TDirView.StopIconUpdateThread;
begin
  if Assigned(FIconUpdateThread) then
  begin
    FIconUpdateThread.Terminate;
    FIconUpdateThread.Priority := tpHigher;
    if FIconUpdateThread.Suspended then
      FIconUpdateThread.Resume;
    if not FIconUpdateThread.WaitFor(MSecsPerSec div 4) then
    begin
      // This prevents Destroy from waiting for (stalled) thread
      FIconUpdateThread.Suspend;
    end;
    FIconUpdateThread.Destroy;
    FIconUpdateThread := nil;
  end;
end; {StopIconUpdateThread}

procedure TDirView.StopWatchThread;
begin
  if Assigned(FDiscMonitor) then
  begin
    FDiscMonitor.Enabled := False;
  end;
end; {StopWatchThread}

procedure TDirView.StartWatchThread;
begin
  if (Length(Path) > 0) and WatchForChanges and DirOK then
  begin
    if not Assigned(FDiscMonitor) then
    begin
      FDiscMonitor := TDiscMonitor.Create(Self);
      with FDiscMonitor do
      begin
        ChangeDelay := msThreadChangeDelay;
        SubTree := False;
        Filters := [moDirName, moFileName, moSize, moAttributes, moLastWrite];
        SetDirectory(PathName);
        OnChange := ChangeDetected;
        OnInvalid := ChangeInvalid;
        Open;
      end;
    end
      else
    begin
      FDiscMonitor.SetDirectory(PathName);
      FDiscMonitor.Enabled := True;
    end;
  end;
end; {StartWatchThread}

procedure TDirView.TimerOnTimer(Sender: TObject);
begin
  if not Loading then
  begin
    // fix by MP: disable timer and reload directory before call to event
    FChangeTimer.Enabled := False;
    FChangeTimer.Interval := 0;
    Reload2;
  end;
end; {TimerOnTimer}

procedure TDirView.ChangeDetected(Sender: TObject; const Directory: string;
  var SubdirsChanged: Boolean);
begin
  // avoid prolonging the actual update with each change, as if continous change
  // is occuring in current directory, the panel will never be updated
  if not FChangeTimer.Enabled then
  begin
    FDirty := True;
    FChangeTimer.Interval := FChangeInterval;
    FChangeTimer.Enabled := True;
  end;
end; {ChangeDetected}

procedure TDirView.ChangeInvalid(Sender: TObject; const Directory: string;
  const ErrorStr: string);
begin
  FDiscMonitor.Close;
end; {ChangeInvalid}

function TDirView.WatchThreadActive: Boolean;
begin
  Result := WatchForChanges and Assigned(FDiscMonitor) and
    FDiscMonitor.Active and FDiscMonitor.Enabled;
end; {WatchThreadActive}

procedure TDirView.SetChangeInterval(Value: Cardinal);
begin
  if Value > 0 then
  begin
    FChangeInterval := Value;
    FChangeTimer.Interval := Value;
  end;
end; {SetChangeInterval}

procedure TDirView.SetDirColProperties(Value: TDirViewColProperties);
begin
  if Value <> ColProperties then
    ColProperties := Value;
end;

function TDirView.GetDirColProperties: TDirViewColProperties;
begin
  Result := TDirViewColProperties(ColProperties);
end;

procedure TDirView.SetWatchForChanges(Value: Boolean);
begin
  if WatchForChanges <> Value then
  begin
    FWatchForChanges := Value;
    if not (csDesigning in ComponentState) then
    begin
      if Value then StartWatchThread
        else StopWatchThread;
    end;
  end;
end; {SetWatchForChanges}

procedure TDirView.DisplayPropertiesMenu;
var
  FileList: TStringList;
  Index: Integer;
  PIDLRel: PItemIDList;
  PIDLPath: PItemIDList;
begin
  if not Assigned(ItemFocused) then
      ShellExecuteContextCommand(ParentForm.Handle, shcProperties, PathName)
    else
  if (not IsRecycleBin) and (MarkedCount > 1) and ItemFocused.Selected then
  begin
    FileList := TStringList.Create;
    try
      CreateFileList(False, True, FileList);
      for Index := 0 to Pred(FileList.Count) do
        FileList[Index] := ExtractFileName(FileList[Index]);
      ShellExecuteContextCommand(ParentForm.Handle, shcProperties,
        PathName, FileList);
    finally
      FileList.Free;
    end;
  end
    else
  if Assigned(ItemFocused.Data) then
  begin
    if IsRecycleBin then
    begin
      if Assigned(PFileRec(ItemFocused.Data)^.PIDL) then
      begin
        PIDL_GetRelative(PFileRec(ItemFocused.Data)^.PIDL, PIDLPath, PIDLRel);
        ShellExecuteContextCommand(ParentForm.Handle, shcProperties, iRecycleFolder, 1, PIDLRel);
        FreePIDL(PIDLRel);
        FreePIDL(PIDLPath);
      end;
    end
      else
    ShellExecuteContextCommand(ParentForm.Handle, shcProperties,
      ItemFullFileName(ItemFocused));
  end;
end;

procedure TDirView.ExecuteFile(Item: TListItem);
var
  DefDir: string;
  FileName: string;
begin
  if (UpperCase(PFileRec(Item.Data)^.FileExt) = 'LNK') or
     PFileRec(Item.Data)^.IsDirectory then
  begin
    if PFileRec(Item.Data)^.IsDirectory then
    begin
      FileName := ItemFullFileName(Item);
      if not DirectoryExistsFix(FileName) then
      begin
        Reload2;
        if Assigned(FDriveView) and Assigned(FDriveView.Selected) then
          with FDriveView do
            ValidateDirectory(Selected);
        Exit;
      end;
    end
      else
    FileName := ResolveFileShortCut(ItemFullFileName(Item), True);

    if DirectoryExistsFix(FileName) then
    begin
      Path := FileName;
      Exit;
    end
      else
    if not FileExistsFix(ApiPath(FileName)) then
    begin
      Exit;
    end;
  end;

  GetDir(0, DefDir);
  ChDir(PathName);

  try
    ShellExecuteContextCommand(ParentForm.Handle, shcDefault,
      ItemFullFileName(Item));
  finally
    ChDir(DefDir);
  end;
end;

procedure TDirView.ExecuteDrive(Drive: string);
var
  APath: string;
  DriveRoot: string;
begin
  if Assigned(FLastPath) and FLastPath.ContainsKey(Drive) then
  begin
    APath := FLastPath[Drive];
    if not DirectoryExists(ApiPath(APath)) then
    begin
      if DriveInfo.IsRealDrive(Drive) then
        APath := Format('%s:', [Drive])
      else
        APath := Drive;
    end;
  end
    else
  begin
    if DriveInfo.IsRealDrive(Drive) then
    begin
      GetDir(Integer(Drive[1]) - Integer('A') + 1, APath);
      DriveRoot := DriveInfo.GetDriveRoot(Drive);
      // When the drive is not valid, the GetDir returns the current drive working directory, detect that,
      // and let it fail later when trying to open root of the invalid drive.
      if not StartsText(DriveRoot, APath) then
        APath := DriveRoot;
      APath := ExcludeTrailingPathDelimiter(APath);
    end
      else
    begin
      APath := Drive;
    end;
  end;

  if Path <> APath then
    Path := APath;
end;

procedure TDirView.ExecuteHomeDirectory;
begin
  Path := HomeDirectory;
end;

procedure TDirView.ExecuteParentDirectory;
begin
  if Valid then
  begin
    if Assigned(DriveView) and Assigned(DriveView.Selected) then
    begin
      DriveView.Selected := DriveView.Selected.Parent
    end
      else
    begin
      Path := ExtractFilePath(Path);
    end;
  end;
end;

procedure TDirView.ExecuteRootDirectory;
begin
  if Valid then
  begin
    FNotRelative := True;
    try
      Path := ExtractFileDrive(Path);
    finally
      FNotRelative := False;
    end;
  end;
end;

procedure TDirView.Delete(Item: TListItem);
begin
  if Assigned(Item) and Assigned(Item.Data) and not (csRecreating in ControlState) then
    with PFileRec(Item.Data)^ do
    begin
      SetLength(FileName, 0);
      SetLength(TypeName, 0);
      SetLength(DisplayName, 0);
      if Assigned(PIDL) then FreePIDL(PIDL);
      Dispose(PFileRec(Item.Data));
      Item.Data := nil;
    end;
  inherited Delete(Item);
end; {Delete}

procedure TDirView.InternalEdit(const HItem: TLVItem);
var
  Item: TListItem;
  Info: string;
  NewCaption: string;
  IsDirectory: Boolean;
begin
  Item := GetItemFromHItem(HItem);
  IsDirectory := DirectoryExists(ItemFullFileName(Item));
  NewCaption := HItem.pszText;

  StopWatchThread;
  if IsDirectory and Assigned(FDriveView) then
    TDriveView(FDriveView).StopWatchThread;

  with FFileOperator do
  begin
    Flags := [foAllowUndo, foNoConfirmation];
    Operation := foRename;
    OperandFrom.Clear;
    OperandTo.Clear;
    OperandFrom.Add(ItemFullFileName(Item));
    OperandTo.Add(FPath + '\' + HItem.pszText);
  end;

  try
    if FFileOperator.Execute then
    begin
      if IsDirectory and Assigned(FDriveView) then
        with FDriveView do
          if Assigned(Selected) then
            ValidateDirectory(Selected);

      with GetFileRec(Item.Index)^ do
      begin
        Empty := True;
        IconEmpty := True;
        FileName := NewCaption;
        DisplayName := FileName;
        FileExt := UpperCase(ExtractFileExt(HItem.pszText));
        FileExt := Copy(FileExt, 2, Length(FileExt) - 1);
        TypeName := EmptyStr;
        if Assigned(PIDL) then
          FreePIDL(PIDL);
      end;
      GetDisplayData(Item, True);
      ResetItemImage(Item.Index);
      UpdateItems(Item.Index, Item.Index);
      if Assigned(OnEdited) then OnEdited(Self, Item, NewCaption);
      if Item <> nil then Item.Caption := NewCaption;
      SortItems;
      if Assigned(ItemFocused) then ItemFocused.MakeVisible(False);
    end
      else
    begin
      Item.Caption := GetFileRec(Item.Index)^.FileName;
      Item.Update;

      if FileOrDirExists(IncludeTrailingPathDelimiter(FPath) + HItem.pszText) then
        Info := SErrorRenameFileExists + HItem.pszText
      else
        Info := SErrorRenameFile + HItem.pszText;

      MessageBeep(MB_ICONHAND);
      if MessageDlg(FormatLastOSError(Info), mtError, [mbOK, mbAbort], 0) = mrOK then
        RetryRename(HItem.pszText);
    end;
  finally
    Sleep(0);
    LoadEnabled := True;
    if FWatchForChanges and (not WatchThreadActive) then
      StartWatchThread;
    if Assigned(FDriveView) then
      TDriveView(FDriveView).StartWatchThread;
  end;
end;

function TDirView.ItemFileName(Item: TListItem): string;
begin
  if Assigned(Item) and Assigned(Item.Data) then
    Result := ExtractFileName(PFileRec(Item.Data)^.FileName)
  else
    Result := '';
end;

function TDirView.ItemFileSize(Item: TListItem): Int64;
begin
  Result := 0;
  if Assigned(Item) and Assigned(Item.Data) then
    with PFileRec(Item.Data)^ do
      if Size >= 0 then Result := Size;
end;

function TDirView.ItemFileTime(Item: TListItem;
  var Precision: TDateTimePrecision): TDateTime;
begin
  Result := FileTimeToDateTime(PFileRec(Item.Data)^.FileTime);
  Precision := tpMillisecond;
end;

function TDirView.ItemImageIndex(Item: TListItem;
  Cache: Boolean): Integer;
begin
  if Assigned(Item) and Assigned(Item.Data) then
  begin
    if PFileRec(Item.Data)^.IconEmpty then
    begin
      if Cache then Result := -1
        else Result := UnknownFileIcon;
    end
      else
    begin
      if (not Cache) or MatchesFileExt(PFileRec(Item.Data)^.FileExt, SpecialExtensions) then
        Result := PFileRec(Item.Data)^.ImageIndex
      else
        Result := -1
    end;
  end
    else Result := -1;
end;

procedure TDirView.Notification(AComponent: TComponent; Operation: TOperation);
begin
  inherited Notification(AComponent, Operation);
  if (Operation = opRemove) and (AComponent = FDriveView) then
    FDriveView := nil;
end; {Notification}

procedure TDirView.ReloadDirectory;
begin
  Reload(True);
end;

procedure TDirView.ResetItemImage(Index: Integer);
var
  LVI: TLVItem;
begin
  with PFileRec(Items[Index].Data)^, LVI do
  begin
    {Update imageindex:}
    Mask := LVIF_STATE or LVIF_DI_SETITEM or LVIF_IMAGE;
    iItem := Index;
    iSubItem := 0;
    if ListView_GetItem(Handle, LVI) then
    begin
      iImage := I_IMAGECALLBACK;
      Mask := Mask and (not LVIF_DI_SETITEM);
      ListView_SetItem(Handle, LVI);
    end;
  end; {With}
end; {ResetItemImage}

{ Drag&Drop handling }

procedure TDirView.SignalFileDelete(Sender: TObject; Files: TStringList);
{Called by TFileDeleteThread, when a file was deleted by the Drag&Drop target window:}
var
  Index: Integer;
begin
  if Files.Count > 0 then
    for Index := 0 to Files.Count - 1 do
      ValidateFile(Files[Index]);
end;

procedure TDirView.DDMenuPopup(Sender: TObject; AMenu: HMenu; DataObj: IDataObject;
  AMinCustCmd: Integer; grfKeyState: Longint; pt: TPoint);
begin
  if Assigned(FDriveView) then
  begin
    // When a change is detected while menu is popped up
    // it loses focus (or something similar)
    // preventing it from handling subsequent click.
    // This typically happens when right-dragging from remote to local panel,
    // what causes temp directory being created+deleted.
    // This is HACK, we should implement some uniform watch disabling/enabling
    TDriveView(FDriveView).SuspendChangeTimer;
  end;

  inherited;
end;

procedure TDirView.DDMenuDone(Sender: TObject; AMenu: HMenu);
begin
  if not WatchThreadActive then
  begin
    FChangeTimer.Interval := Min(FChangeInterval * 2, 3000);
    FChangeTimer.Enabled  := True;
  end;

  if Assigned(FDriveView) then
  begin
    TDriveView(FDriveView).ResumeChangeTimer;
  end;

  inherited;
end;

procedure TDirView.DDDropHandlerSucceeded(Sender: TObject; grfKeyState: Longint;
  Point: TPoint; dwEffect: Longint);
begin
  // Not sure why is this here. There's no "disable" counterparty.
  if not WatchThreadActive then
  begin
    FChangeTimer.Interval := FChangeInterval;
    FChangeTimer.Enabled  := True;
  end;

  inherited;
end;

procedure TDirView.AddToDragFileList(FileList: TFileList; Item: TListItem);
begin
  Assert(Assigned(Item));
  if IsRecycleBin then
  begin
    if Assigned(Item.Data) then
    begin
      if UpperCase(ExtractFileExt(PFileRec(Item.Data)^.DisplayName)) =
        ('.' + PFileRec(Item.Data)^.FileExt) then
          FileList.AddItemEx(PFileRec(Item.Data)^.PIDL,
            ItemFullFileName(Item), PFileRec(Item.Data)^.DisplayName)
      else
        FileList.AddItemEx(PFileRec(Item.Data)^.PIDL,
          ItemFullFileName(Item), PFileRec(Item.Data)^.DisplayName +
            ExtractFileExt(PFileRec(Item.Data)^.FileName));
    end;
  end
    else inherited;
end;

procedure TDirView.DDDragDetect(grfKeyState: Longint; DetectStart, Point: TPoint;
  DragStatus: TDragDetectStatus);
var
  WasWatchThreadActive: Boolean;
begin
  if (DragStatus = ddsDrag) and (MarkedCount > 0) then
  begin
    WasWatchThreadActive := WatchThreadActive;
    inherited;

    if (LastDDResult = drMove) and (not WasWatchThreadActive) then
      StartFileDeleteThread;
  end;
end; {DDDragDetect}

procedure TDirView.DDChooseEffect(grfKeyState: Integer; var dwEffect: Integer; PreferredEffect: Integer);
begin
  if DragDropFilesEx.OwnerIsSource and
     (dwEffect = DROPEFFECT_COPY) and (not Assigned(DropTarget)) then
  begin
    dwEffect := DROPEFFECT_NONE
  end
    else
  if (grfKeyState and (MK_CONTROL or MK_SHIFT) = 0) and (PreferredEffect = 0) then
  begin
    if FDragDrive <> '' then
    begin
      if ExeDrag and DriveInfo.IsFixedDrive(DriveInfo.GetDriveKey(Path)) and DriveInfo.IsFixedDrive(FDragDrive) then
      begin
        dwEffect := DROPEFFECT_LINK;
      end
        else
      begin
        if DragOnDriveIsMove and
           (not DDOwnerIsSource or Assigned(DropTarget)) and
           ((SameText(FDragDrive, DriveInfo.GetDriveKey(Path)) and (dwEffect = DROPEFFECT_COPY) and
           (DragDropFilesEx.AvailableDropEffects and DROPEFFECT_MOVE <> 0))
             or IsRecycleBin) then
        begin
          dwEffect := DROPEFFECT_MOVE;
        end;
      end;
    end;
  end;

  inherited;
end;

procedure TDirView.PerformDragDropFileOperation(TargetPath: string;
  Effect: Integer; RenameOnCollision: Boolean; Paste: Boolean);
var
  Index: Integer;
  SourcePath: string;
  OldCursor: TCursor;
  OldWatchForChanges: Boolean;
  IsRecycleBin: Boolean;
  SourceIsDirectory: Boolean;
  Node: TTreeNode;
begin
  if DragDropFilesEx.FileList.Count > 0 then
  begin
    if not DirectoryExists(TargetPath) then
    begin
      Reload(True);
      DDError(DDPathNotFoundError);
    end
      else
    begin
      IsRecycleBin := Self.IsRecycleBin or
        ((DropTarget <> nil) and ItemIsRecycleBin(DropTarget));
      if not (DragDropFilesEx.FileNamesAreMapped and IsRecycleBin) then
      begin
        OldCursor := Screen.Cursor;
        OldWatchForChanges := WatchForChanges;
        SourceIsDirectory := True;
        SourcePath := EmptyStr;

        try
          Screen.Cursor := crHourGlass;
          WatchForChanges := False;

          if Effect in [DROPEFFECT_COPY, DROPEFFECT_MOVE] then
          begin
            StopWatchThread;

            if Assigned(DriveView) then
              TDriveView(DriveView).StopWatchThread;

            if (DropSourceControl <> Self) and
               (DropSourceControl is TDirView) then
                TDirView(DropSourceControl).StopWatchThread;

            if DropFiles(
                 DragDropFilesEx, Effect, FFileOperator, TargetPath, RenameOnCollision, IsRecycleBin,
                 ConfirmDelete, ConfirmOverwrite, Paste,
                 Self, OnDDFileOperation, SourcePath, SourceIsDirectory) then
            begin
              ReLoad2;
              if Assigned(OnDDFileOperationExecuted) then
                OnDDFileOperationExecuted(Self, Effect, SourcePath, TargetPath);
            end;
          end
            else
          if Effect = DROPEFFECT_LINK then
          (* Create Link requested: *)
          begin
            StopWatchThread;
            for Index := 0 to DragDropFilesEx.FileList.Count - 1 do
            begin
              if not DropLink(PFDDListItem(DragDropFilesEx.FileList[Index]), TargetPath) then
              begin
                DDError(DDCreateShortCutError);
              end;
            end;
            ReLoad2;
          end;

          if Assigned(DropSourceControl) and
             (DropSourceControl is TDirView) and
             (DropSourceControl <> Self) and
             (Effect = DROPEFFECT_MOVE) then
          begin
            TDirView(DropSourceControl).ValidateSelectedFiles;
          end;

          if Assigned(FDriveView) and SourceIsDirectory then
          begin
            with TDriveView(FDriveView) do
            begin
              try
                ValidateDirectory(FindNodeToPath(TargetPath));
              except
              end;

              if (Effect = DROPEFFECT_MOVE) or IsRecycleBin then
              try
                Node := TryFindNodeToPath(SourcePath);
                // If the path is not even in the tree, do not bother.
                // This is particularly for dragging from remote folder, when the source path in %TEMP% and
                // calling ValidateDirectory would load whole TEMP (and typically also "C:\Users")
                if Assigned(Node) then
                begin
                  if Assigned(Node.Parent) then
                    Node := Node.Parent;
                  ValidateDirectory(Node);
                end;
              except
              end;
            end;
          end;
        finally
          FFileOperator.OperandFrom.Clear;
          FFileOperator.OperandTo.Clear;
          if Assigned(FDriveView) then
            TDriveView(FDriveView).StartWatchThread;
          Sleep(0);
          WatchForChanges := OldWatchForChanges;
          if (DropSourceControl <> Self) and (DropSourceControl is TDirView) then
            TDirView(DropSourceControl).StartWatchThread;
          Screen.Cursor := OldCursor;
        end;
      end;
    end;
  end;
end; {PerformDragDropFileOperation}

procedure TDirView.DDError(ErrorNo: TDDError);
begin
  if Assigned(OnDDError) then OnDDError(Self, ErrorNo)
    else
  raise EDragDrop.Create(Format(SDragDropError, [Ord(ErrorNo)]));
end; {DDError}

function TDirView.GetCanUndoCopyMove: Boolean;
begin
  Result := Assigned(FFileOperator) and FFileOperator.CanUndo;
end; {CanUndoCopyMove}

function TDirView.UndoCopyMove : Boolean;
var
  LastTarget: string;
  LastSource: string;
begin
  Result := False;
  if FFileOperator.CanUndo then
  begin
    Lasttarget := FFileOperator.LastOperandTo[0];
    LastSource := FFileOperator.LastOperandFrom[0];
    if Assigned(FDriveView) then
      TDriveView(FDriveView).StopAllWatchThreads;

    Result := FFileOperator.UndoExecute;

    if not WatchthreadActive then
      Reload2;

    if Assigned(FDriveView) then
      with TDriveView(FDriveView) do
      begin
        ValidateDirectory(FindNodeToPath(ExtractFilePath(LastTarget)));
        ValidateDirectory(FindNodeToPath(ExtractFilePath(LastSource)));
        StartAllWatchThreads;
      end;
  end;
end; {UndoCopyMove}

procedure TDirView.EmptyClipboard;
var
  Item: TListItem;
begin
  if Windows.OpenClipBoard(0) then
  begin
    Windows.EmptyClipBoard;
    Windows.CloseClipBoard;
    if LastClipBoardOperation <> cboNone then
    begin
      Item := GetNextItem(nil, sdAll, [isCut]);
      while Assigned(Item) do
      begin
        Item.Cut := False;
        Item := GetNextItem(Item, sdAll, [isCut]);
      end;
    end;
    LastClipBoardOperation := cboNone;
    if Assigned(FDriveView) then
      TDriveView(FDriveView).LastPathCut := '';
  end;
end; {EmptyClipBoard}

function TDirView.DoCopyToClipboard(Focused: Boolean; Cut: Boolean; Operation: TClipBoardOperation): Boolean;
var
  Item: TListItem;
  SaveCursor: TCursor;
begin
  SaveCursor := Screen.Cursor;
  Screen.Cursor := crHourGlass;
  try
    Result := False;
    EmptyClipBoard;
    DragDropFilesEx.FileList.Clear;
    if OperateOnFocusedFile(Focused) or (SelCount > 0) then
    begin
      if OperateOnFocusedFile(Focused) then
      begin
        DragDropFilesEx.FileList.AddItem(nil, ItemFullFileName(ItemFocused));
      end
        else
      begin
        Item := GetNextItem(nil, sdAll, [isSelected]);
        while Assigned(Item) do
        begin
          DragDropFilesEx.FileList.AddItem(nil, ItemFullFileName(Item));
          Item.Cut := Cut;
          Item := GetNextItem(Item, sdAll, [isSelected]);
        end;
      end;

      Result := DragDropFilesEx.CopyToClipBoard;
      LastClipBoardOperation := Operation;
    end;
  finally
    Screen.Cursor := SaveCursor;
  end;
end; {DoCopyToClipBoard}

function TDirView.CopyToClipBoard(Focused: Boolean): Boolean;
begin
  Result := DoCopyToClipboard(Focused, False, cboCopy);
end;

function TDirView.CutToClipBoard(Focused: Boolean): Boolean;
begin
  Result := DoCopyToClipboard(Focused, True, cboCut);
end;

function TDirView.PasteFromClipBoard(TargetPath: string): Boolean;
begin
  DragDropFilesEx.FileList.Clear;
  Result := False;
  if CanPasteFromClipBoard and {MP}DragDropFilesEx.GetFromClipBoard{/MP}
    then
  begin
    if TargetPath = '' then
      TargetPath := PathName;
    case LastClipBoardOperation of
      cboNone:
        begin
          PerformDragDropFileOperation(TargetPath, DROPEFFECT_COPY, False, True);
          if Assigned(OnDDExecuted) then OnDDExecuted(Self, DROPEFFECT_COPY);
        end;
      cboCopy:
        begin
          PerformDragDropFileOperation(TargetPath, DROPEFFECT_COPY,
            ExcludeTrailingPathDelimiter(ExtractFilePath(TFDDListItem(DragDropFilesEx.FileList[0]^).Name)) = Path, True);
          if Assigned(OnDDExecuted) then OnDDExecuted(Self, DROPEFFECT_COPY);
        end;
      cboCut:
        begin
          PerformDragDropFileOperation(TargetPath, DROPEFFECT_MOVE, False, True);
          if Assigned(OnDDExecuted) then OnDDExecuted(Self, DROPEFFECT_MOVE);
          EmptyClipBoard;
        end;
    end;
    Result := True;
  end;
end; {PasteFromClipBoard}

function TDirView.DragCompleteFileList: Boolean;
begin
  Result := inherited DragCompleteFileList and
    (FDriveType <> DRIVE_REMOVABLE);
end;

function TDirView.DuplicateSelectedFiles: Boolean;
begin
  Result := False;
  if SelCount > 0 then
  begin
    Result := CopyToClipBoard(False);
    if Result then
      try
        SelectNewFiles := True;
        Selected := nil;
        Result := PasteFromClipBoard();
      finally
        SelectNewFiles := False;
        if Assigned(Selected) then
        begin
          ItemFocused := Selected;
          Selected.MakeVisible(False);
          if SelCount = 1 then
            Selected.EditCaption;
        end;
      end;

  end;
  EmptyClipBoard;
end; {DuplicateFiles}

function TDirView.NewColProperties: TCustomListViewColProperties;
begin
  Result := TDirViewColProperties.Create(Self);
end;

function TDirView.SortAscendingByDefault(Index: Integer): Boolean;
begin
  Result := not (TDirViewCol(Index) in [dvSize, dvChanged]);
end;

procedure TDirView.SetItemImageIndex(Item: TListItem; Index: Integer);
begin
  Assert(Assigned(Item));
  if Assigned(Item.Data) then
    with PFileRec(Item.Data)^ do
    begin
      ImageIndex := Index;
      IconEmpty := (ImageIndex < 0);
    end;
end;

{=================================================================}

initialization
  LastClipBoardOperation := cboNone;
  DaylightHack := (not IsWin7);
end.
