#include <windows.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <softpub.h>
#include <vector>

// This is the only elevated component. It accepts no certificate path or
// thumbprint: it can trust only the public certificate signing its own binary.
int WINAPI wWinMain(HINSTANCE,HINSTANCE,LPWSTR arguments,int){
 if(arguments&&*arguments)return ERROR_INVALID_PARAMETER;
 wchar_t path[32768]{};DWORD n=GetModuleFileNameW(nullptr,path,DWORD(std::size(path)));
 if(!n||n>=std::size(path))return ERROR_BAD_PATHNAME;
 WINTRUST_FILE_INFO file{sizeof(file)};file.pcwszFilePath=path;
 WINTRUST_DATA trust{sizeof(trust)};trust.dwUIChoice=WTD_UI_NONE;trust.fdwRevocationChecks=WTD_REVOKE_NONE;
 trust.dwUnionChoice=WTD_CHOICE_FILE;trust.pFile=&file;trust.dwStateAction=WTD_STATEACTION_VERIFY;
 trust.dwProvFlags=WTD_CACHE_ONLY_URL_RETRIEVAL;
 GUID action=WINTRUST_ACTION_GENERIC_VERIFY_V2;
 LONG verified=WinVerifyTrust(nullptr,&action,&trust);
 trust.dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust(nullptr,&action,&trust);
 if(verified!=ERROR_SUCCESS&&verified!=CERT_E_UNTRUSTEDROOT)return int(TRUST_E_BAD_DIGEST);
 HCERTSTORE source=nullptr;HCRYPTMSG message=nullptr;
 if(!CryptQueryObject(CERT_QUERY_OBJECT_FILE,path,CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,CERT_QUERY_FORMAT_FLAG_BINARY,0,nullptr,nullptr,nullptr,&source,&message,nullptr))return int(TRUST_E_BAD_DIGEST);
 DWORD size=0;PCCERT_CONTEXT cert=nullptr;
 if(CryptMsgGetParam(message,CMSG_SIGNER_INFO_PARAM,0,nullptr,&size)){
  std::vector<BYTE> bytes(size);
  if(CryptMsgGetParam(message,CMSG_SIGNER_INFO_PARAM,0,bytes.data(),&size)){
   auto signer=reinterpret_cast<CMSG_SIGNER_INFO*>(bytes.data());CERT_INFO info{};info.Issuer=signer->Issuer;info.SerialNumber=signer->SerialNumber;
   cert=CertFindCertificateInStore(source,X509_ASN_ENCODING|PKCS_7_ASN_ENCODING,0,CERT_FIND_SUBJECT_CERT,&info,nullptr);
  }
 }
 int result=int(TRUST_E_BAD_DIGEST);
 if(cert){
  HCERTSTORE target=CertOpenStore(CERT_STORE_PROV_SYSTEM_W,0,0,CERT_SYSTEM_STORE_LOCAL_MACHINE,L"TrustedPeople");
  if(target){result=CertAddCertificateContextToStore(target,cert,CERT_STORE_ADD_USE_EXISTING,nullptr)?ERROR_SUCCESS:int(GetLastError());CertCloseStore(target,0);}
  else result=int(GetLastError());
  CertFreeCertificateContext(cert);
 }
 CryptMsgClose(message);CertCloseStore(source,0);return result;
}
