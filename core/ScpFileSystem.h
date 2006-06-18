//---------------------------------------------------------------------------
#ifndef ScpFileSystemH
#define ScpFileSystemH

#include <FileSystems.h>
//---------------------------------------------------------------------------
class TCommandSet;
//---------------------------------------------------------------------------
class TSCPFileSystem : public TCustomFileSystem
{
public:
  __fastcall TSCPFileSystem(TTerminal * ATerminal);
  virtual __fastcall ~TSCPFileSystem();

  virtual AnsiString __fastcall AbsolutePath(AnsiString Path);
  virtual void __fastcall KeepAlive();
  virtual void __fastcall AnyCommand(const AnsiString Command,
    TLogAddLineEvent OutputEvent);
  virtual void __fastcall ChangeDirectory(const AnsiString Directory);
  virtual void __fastcall CachedChangeDirectory(const AnsiString Directory);
  virtual void __fastcall ChangeFileProperties(const AnsiString FileName,
    const TRemoteFile * File, const TRemoteProperties * Properties);
  virtual bool __fastcall LoadFilesProperties(TStrings * FileList);
  virtual void __fastcall CopyToLocal(TStrings * FilesToCopy,
    const AnsiString TargetDir, const TCopyParamType * CopyParam,
    int Params, TFileOperationProgressType * OperationProgress,
    bool & DisconnectWhenComplete);
  virtual void __fastcall CopyToRemote(TStrings * FilesToCopy,
    const AnsiString TargetDir, const TCopyParamType * CopyParam,
    int Params, TFileOperationProgressType * OperationProgress,
    bool & DisconnectWhenComplete);
  virtual void __fastcall CreateDirectory(const AnsiString DirName,
    const TRemoteProperties * Properties);
  virtual void __fastcall CreateLink(const AnsiString FileName, const AnsiString PointTo, bool Symbolic);
  virtual void __fastcall DeleteFile(const AnsiString FileName,
    const TRemoteFile * File, bool Recursive);
  virtual void __fastcall CustomCommandOnFile(const AnsiString FileName,
    const TRemoteFile * File, AnsiString Command, int Params, TLogAddLineEvent OutputEvent);
  virtual void __fastcall DoStartup();
  virtual void __fastcall HomeDirectory();
  virtual bool __fastcall IsCapable(int Capability) const;
  virtual void __fastcall AdditionalInfo(TStrings * AdditionalInfo, bool Initial);
  virtual void __fastcall LookupUsersGroups();
  virtual void __fastcall ReadCurrentDirectory();
  virtual void __fastcall ReadDirectory(TRemoteFileList * FileList);
  virtual void __fastcall ReadFile(const AnsiString FileName,
    TRemoteFile *& File);
  virtual void __fastcall ReadSymlink(TRemoteFile * SymlinkFile,
    TRemoteFile *& File);
  virtual void __fastcall RenameFile(const AnsiString FileName,
    const AnsiString NewName);
  virtual void __fastcall CopyFile(const AnsiString FileName,
    const AnsiString NewName);
  virtual AnsiString __fastcall FileUrl(const AnsiString FileName);
  virtual TStrings * __fastcall GetFixedPaths();
  virtual void __fastcall SpaceAvailable(const AnsiString Path,
    TSpaceAvailable & ASpaceAvailable);
  virtual bool __fastcall TemporaryTransferFile(const AnsiString & FileName);

protected:
  __property TStrings * Output = { read = FOutput };
  __property int ReturnCode = { read = FReturnCode };

  virtual AnsiString __fastcall GetCurrentDirectory();
  virtual void __fastcall SetCurrentDirectory(AnsiString value);
  virtual AnsiString __fastcall GetProtocolName() const;

private:
  TCommandSet * FCommandSet;
  AnsiString FCurrentDirectory;
  TStrings * FOutput;
  int FReturnCode;
  AnsiString FCachedDirectoryChange;
  bool FProcessingCommand;
  int FLsFullTime;
  TLogAddLineEvent FOnCaptureOutput;

  void __fastcall AliasGroupList();
  void __fastcall ClearAliases();
  void __fastcall CustomReadFile(const AnsiString FileName,
    TRemoteFile *& File, TRemoteFile * ALinkedByFile);
  static AnsiString __fastcall DelimitStr(AnsiString Str);
  void __fastcall DetectReturnVar();
  bool __fastcall IsLastLine(AnsiString & Line);
  static bool __fastcall IsTotalListingLine(const AnsiString Line);
  void __fastcall EnsureLocation();
  void __fastcall ExecCommand(const AnsiString Cmd, int Params = -1);
  void __fastcall ExecCommand(TFSCommand Cmd, const TVarRec * args = NULL,
    int size = 0, int Params = -1);
  void __fastcall ReadCommandOutput(int Params);
  void __fastcall SCPResponse(bool * GotLastLine = NULL);
  void __fastcall SCPDirectorySource(const AnsiString DirectoryName,
    const TCopyParamType * CopyParam, int Params,
    TFileOperationProgressType * OperationProgress, int Level);
  void __fastcall SCPError(const AnsiString Message, bool Fatal);
  void __fastcall SCPSendError(const AnsiString Message, bool Fatal);
  void __fastcall SCPSink(const AnsiString TargetDir,
    const AnsiString FileName, const AnsiString SourceDir,
    const TCopyParamType * CopyParam, bool & Success,
    TFileOperationProgressType * OperationProgress, int Params, int Level);
  void __fastcall SCPSource(const AnsiString FileName,
    const TCopyParamType * CopyParam, int Params,
    TFileOperationProgressType * OperationProgress, int Level);
  void __fastcall SendCommand(const AnsiString Cmd);
  void __fastcall SkipFirstLine();
  void __fastcall SkipStartupMessage();
  void __fastcall UnsetNationalVars();
  TRemoteFile * __fastcall CreateRemoteFile(const AnsiString & ListingStr, 
  	TRemoteFile * LinkedByFile = NULL);
  void __fastcall CaptureOutput(TObject * Sender, TLogLineType Type,
    const AnsiString AddedLine);

  static bool __fastcall RemoveLastLine(AnsiString & Line,
    int & ReturnCode, AnsiString LastLine = "");
};
//---------------------------------------------------------------------------
#endif // ScpFileSystemH
