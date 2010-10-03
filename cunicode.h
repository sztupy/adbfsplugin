
BOOL usys();

#define wdirtypemax 1024
#define longnameprefixmax 6

#ifndef countof
#define countof(str) (sizeof(str)/sizeof(str[0]))
#endif // countof

WCHAR* wcslcpy(WCHAR *str1,const WCHAR *str2,int imaxlen);
WCHAR* wcslcat(wchar_t *str1,const WCHAR *str2,int imaxlen);
char* walcopy(char* outname,WCHAR* inname,int maxlen);
WCHAR* awlcopy(WCHAR* outname,char* inname,int maxlen);

#define wafilenamecopy(outname,inname) walcopy(outname,inname,countof(outname)-1)
#define awfilenamecopy(outname,inname) awlcopy(outname,inname,countof(outname)-1)

void copyfinddatawa(WIN32_FIND_DATA *lpFindFileDataA,WIN32_FIND_DATAW *lpFindFileDataW);
void copyfinddataaw(WIN32_FIND_DATAW *lpFindFileDataW,WIN32_FIND_DATA *lpFindFileDataA);

int ProgressProcT(int PluginNr,WCHAR* SourceName,WCHAR* TargetName,int PercentDone);
void LogProcT(int PluginNr,int MsgType,WCHAR* LogString);
BOOL RequestProcT(int PluginNr,int RequestType,WCHAR* CustomTitle,
              WCHAR* CustomText,WCHAR* ReturnedText,int maxlen);

BOOL CopyFileT(WCHAR* lpExistingFileName,WCHAR* lpNewFileName,BOOL bFailIfExists);
BOOL CreateDirectoryT(WCHAR* lpPathName,LPSECURITY_ATTRIBUTES lpSecurityAttributes);
BOOL RemoveDirectoryT(WCHAR* lpPathName);
BOOL DeleteFileT(WCHAR* lpFileName);
BOOL MoveFileT(WCHAR* lpExistingFileName,WCHAR* lpNewFileName);
BOOL SetFileAttributesT(WCHAR* lpFileName,DWORD dwFileAttributes);
HANDLE CreateFileT(WCHAR* lpFileName,DWORD dwDesiredAccess,DWORD dwShareMode,
  LPSECURITY_ATTRIBUTES lpSecurityAttributes,DWORD dwCreationDisposition,
  DWORD dwFlagsAndAttributes,HANDLE hTemplateFile);
UINT ExtractIconExT(WCHAR* lpszFile,int nIconIndex,HICON *phiconLarge,HICON *phiconSmall,UINT nIcons);
HANDLE FindFirstFileT(WCHAR* lpFileName,LPWIN32_FIND_DATAW lpFindFileData);
BOOL FindNextFileT(HANDLE hFindFile,LPWIN32_FIND_DATAW lpFindFileData);