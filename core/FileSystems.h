//---------------------------------------------------------------------------
#ifndef FileSystemsH
#define FileSystemsH

#include <Classes.hpp>
//---------------------------------------------------------------------------
class TTerminal;
class TRights;
class TRemoteFile;
class TRemoteFileList;
struct TCopyParamType;
struct TSpaceAvailable;
class TFileOperationProgressType;
class TRemoteProperties;
//---------------------------------------------------------------------------
enum TFSCommand { fsNull = 0, fsVarValue, fsLastLine, fsFirstLine,
  fsCurrentDirectory, fsChangeDirectory, fsListDirectory, fsListCurrentDirectory,
  fsListFile, fsLookupUsersGroups, fsCopyToRemote, fsCopyToLocal, fsDeleteFile,
  fsRenameFile, fsCreateDirectory, fsChangeMode, fsChangeGroup, fsChangeOwner,
  fsHomeDirectory, fsUnset, fsUnalias, fsAliasGroupList, fsCreateLink, fsCopyFile, 
  fsAnyCommand, fsReadSymlink, fsChangeProperties, fsMoveFile };
//---------------------------------------------------------------------------
// Duplicated in LogMemo.h for design-time-only purposes
enum TLogLineType {llOutput, llInput, llStdError, llMessage, llException};
typedef void __fastcall (__closure *TLogAddLineEvent)(System::TObject* Sender,
  TLogLineType Type, const AnsiString AddedLine);
//---------------------------------------------------------------------------
class TCustomFileSystem : public TObject
{
public:
  virtual AnsiString __fastcall AbsolutePath(AnsiString Path) = 0;
  virtual void __fastcall KeepAlive() = 0;
  virtual void __fastcall AnyCommand(const AnsiString Command,
    TLogAddLineEvent OutputEvent) = 0;
  virtual void __fastcall ChangeDirectory(const AnsiString Directory) = 0;
  virtual void __fastcall CachedChangeDirectory(const AnsiString Directory) = 0;
  virtual void __fastcall ChangeFileProperties(const AnsiString FileName,
    const TRemoteFile * File, const TRemoteProperties * Properties) = 0;
  virtual bool __fastcall LoadFilesProperties(TStrings * FileList) = 0;
  virtual void __fastcall CopyToLocal(TStrings * FilesToCopy,
    const AnsiString TargetDir, const TCopyParamType * CopyParam,
    int Params, TFileOperationProgressType * OperationProgress,
    bool & DisconnectWhenComplete) = 0;
  virtual void __fastcall CopyToRemote(TStrings * FilesToCopy,
    const AnsiString TargetDir, const TCopyParamType * CopyParam,
    int Params, TFileOperationProgressType * OperationProgress,
    bool & DisconnectWhenComplete) = 0;
  virtual void __fastcall CreateDirectory(const AnsiString DirName,
    const TRemoteProperties * Properties) = 0;
  virtual void __fastcall CreateLink(const AnsiString FileName, const AnsiString PointTo, bool Symbolic) = 0;
  virtual void __fastcall DeleteFile(const AnsiString FileName,
    const TRemoteFile * File, bool Recursive) = 0;
  virtual void __fastcall CustomCommandOnFile(const AnsiString FileName,
    const TRemoteFile * File, AnsiString Command, int Params, TLogAddLineEvent OutputEvent) = 0;
  virtual void __fastcall DoStartup() = 0;
  virtual void __fastcall HomeDirectory() = 0;
  virtual bool __fastcall IsCapable(int Capability) const = 0;
  virtual void __fastcall AdditionalInfo(TStrings * AdditionalInfo, bool Initial) = 0;
  virtual void __fastcall LookupUsersGroups() = 0;
  virtual void __fastcall ReadCurrentDirectory() = 0;
  virtual void __fastcall ReadDirectory(TRemoteFileList * FileList) = 0;
  virtual void __fastcall ReadFile(const AnsiString FileName,
    TRemoteFile *& File) = 0;
  virtual void __fastcall ReadSymlink(TRemoteFile * SymLinkFile,
    TRemoteFile *& File) = 0;
  virtual void __fastcall RenameFile(const AnsiString FileName,
    const AnsiString NewName) = 0;
  virtual void __fastcall CopyFile(const AnsiString FileName,
    const AnsiString NewName) = 0;
  virtual AnsiString __fastcall FileUrl(const AnsiString FileName) = 0;
  virtual TStrings * __fastcall GetFixedPaths() = 0;
  virtual void __fastcall SpaceAvailable(const AnsiString Path,
    TSpaceAvailable & ASpaceAvailable) = 0;
  virtual bool __fastcall TemporaryTransferFile(const AnsiString & FileName) = 0;

  __property AnsiString CurrentDirectory = { read = GetCurrentDirectory, write = SetCurrentDirectory };
  __property AnsiString ProtocolName = { read = GetProtocolName };

protected:
  TTerminal * FTerminal;

  __fastcall TCustomFileSystem(TTerminal * ATerminal);
  virtual AnsiString __fastcall GetCurrentDirectory() = 0;
  virtual AnsiString __fastcall GetProtocolName() const = 0;
  virtual void __fastcall SetCurrentDirectory(AnsiString value) = 0;

  static void __fastcall FindCustomCommandPattern(
    const AnsiString & Command, int Index, int & Len, char & PatternCmd);
};
//---------------------------------------------------------------------------
#endif
