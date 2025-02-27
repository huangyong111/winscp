//---------------------------------------------------------------------------
#ifndef SetupH
#define SetupH
//---------------------------------------------------------------------------
#include <Interface.h>
#include <WinConfiguration.h>
#include <WinInterface.h>
//---------------------------------------------------------------------------
void __fastcall SetupInitialize();
void __fastcall AddSearchPath(const UnicodeString Path);
void __fastcall RemoveSearchPath(const UnicodeString Path);
class THttp;
THttp * __fastcall CreateHttp();
void __fastcall GetUpdatesMessage(UnicodeString & Message, bool & New, TQueryType & Type, bool Force);
bool __fastcall CheckForUpdates(bool CachedResults);
bool __fastcall QueryUpdates(TUpdatesConfiguration & Updates);
void FormatUpdatesMessage(UnicodeString & UpdatesMessage, const UnicodeString & AMessage, const TUpdatesConfiguration & Updates);
void __fastcall EnableAutomaticUpdates();
void __fastcall RegisterForDefaultProtocols();
void __fastcall UnregisterForProtocols();
void __fastcall LaunchAdvancedAssociationUI();
void __fastcall TemporaryDirectoryCleanup();
void __fastcall StartUpdateThread(TThreadMethod OnUpdatesChecked);
void __fastcall StopUpdateThread();
UnicodeString __fastcall CampaignUrl(UnicodeString URL);
void __fastcall UpdateJumpList(TStrings * SessionNames, TStrings * WorkspaceNames);
bool __fastcall AnyOtherInstanceOfSelf();
bool __fastcall IsInstalled();
UnicodeString __fastcall ProgramUrl(UnicodeString URL);
void __fastcall AutoShowNewTip();
bool __fastcall AnyTips();
void __fastcall ShowTips();
UnicodeString __fastcall FirstUnshownTip();
void __fastcall TipsUpdateStaticUsage();
int __fastcall GetNetVersion();
UnicodeString GetNetVersionStr();
UnicodeString GetNetCoreVersionStr();
UnicodeString GetPowerShellVersionStr();
UnicodeString GetPowerShellCoreVersionStr();
int ComRegistration(TConsole * Console);
//---------------------------------------------------------------------------
#endif
