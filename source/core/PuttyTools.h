//---------------------------------------------------------------------------
#ifndef PuttyToolsH
#define PuttyToolsH
//---------------------------------------------------------------------------
enum TKeyType
{
  ktUnopenable, ktUnknown,
  ktSSH1, ktSSH2,
  ktOpenSSHAuto, ktOpenSSHPEM, ktOpenSSHNew, ktSSHCom,
  ktSSH1Public, ktSSH2PublicRFC4716, ktSSH2PublicOpenSSH
};
TKeyType KeyType(UnicodeString FileName);
bool IsKeyEncrypted(TKeyType KeyType, const UnicodeString & FileName, UnicodeString & Comment);
struct TPrivateKey;
TPrivateKey * LoadKey(TKeyType KeyType, const UnicodeString & FileName, const UnicodeString & Passphrase);
UnicodeString TestKey(TKeyType KeyType, const UnicodeString & FileName);
void ChangeKeyComment(TPrivateKey * PrivateKey, const UnicodeString & Comment);
void SaveKey(TKeyType KeyType, const UnicodeString & FileName,
  const UnicodeString & Passphrase, TPrivateKey * PrivateKey);
void FreeKey(TPrivateKey * PrivateKey);
UnicodeString GetPublicKeyLine(const UnicodeString & FileName, UnicodeString & Comment);
extern const UnicodeString PuttyKeyExt;
//---------------------------------------------------------------------------
bool __fastcall HasGSSAPI(UnicodeString CustomPath);
//---------------------------------------------------------------------------
void __fastcall AES256EncodeWithMAC(char * Data, size_t Len, const char * Password,
  size_t PasswordLen, const char * Salt);
//---------------------------------------------------------------------------
void __fastcall NormalizeFingerprint(UnicodeString & Fingerprint, UnicodeString & KeyName);
UnicodeString __fastcall KeyTypeFromFingerprint(UnicodeString Fingerprint);
//---------------------------------------------------------------------------
UnicodeString __fastcall GetPuTTYVersion();
//---------------------------------------------------------------------------
UnicodeString __fastcall Sha256(const char * Data, size_t Size);
//---------------------------------------------------------------------------
void __fastcall DllHijackingProtection();
//---------------------------------------------------------------------------
UnicodeString __fastcall ParseOpenSshPubLine(const UnicodeString & Line, const struct ssh_keyalg *& Algorithm);
//---------------------------------------------------------------------------
UnicodeString __fastcall GetKeyTypeHuman(const UnicodeString & KeyType);
//---------------------------------------------------------------------------
bool IsOpenSSH(const UnicodeString & SshImplementation);
//---------------------------------------------------------------------------
TStrings * SshCipherList();
TStrings * SshKexList();
TStrings * SshHostKeyList();
TStrings * SshMacList();
//---------------------------------------------------------------------------
class TSessionData;
void SaveAsPutty(const UnicodeString & Name, TSessionData * Data);
class THierarchicalStorage;
void WritePuttySettings(THierarchicalStorage * Storage, const UnicodeString & Settings);
void SavePuttyDefaults(const UnicodeString & Name);
//---------------------------------------------------------------------------
bool RandomSeedExists();
//---------------------------------------------------------------------------
#endif
