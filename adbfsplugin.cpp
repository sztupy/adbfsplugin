// fsplugin.cpp : Defines the entry point for the DLL application.
//

#include "stdafx.h"
#include "adbfsplugin.h"
#include "adbhandler.h"
#include "cunicode.h"

#define pluginrootlen 1

using namespace std;

HANDLE hinst;
char inifilename[MAX_PATH]="adbfsplugin.ini";  // Unused in this plugin, may be used to save data

char* strlcpy(char* p,char*p2,int maxlen)
{
	if ((int)strlen(p2)>=maxlen) {
		strncpy(p,p2,maxlen);
		p[maxlen]=0;
	} else
		strcpy(p,p2);
	return p;
}

BOOL APIENTRY DllMain( HANDLE hModule, 
                       DWORD  ul_reason_for_call, 
                       LPVOID lpReserved
					 )
{
   	if (ul_reason_for_call==DLL_PROCESS_ATTACH)
		hinst=hModule;
	return TRUE;
}

int PluginNumber;
tProgressProc ProgressProc=NULL;
tLogProc LogProc=NULL;
tRequestProc RequestProc=NULL;
tProgressProcW ProgressProcW=NULL;
tLogProcW LogProcW=NULL;
tRequestProcW RequestProcW=NULL;
map<wstring,FileData> cacheMap;

int __stdcall FsInit(int PluginNr,tProgressProc pProgressProc,tLogProc pLogProc,tRequestProc pRequestProc)
{
	ProgressProc=pProgressProc;
    LogProc=pLogProc;
    RequestProc=pRequestProc;
	PluginNumber=PluginNr;
	return WSAStartup(MAKEWORD(2,2), &wsaData);
}

int __stdcall FsInitW(int PluginNr,tProgressProcW pProgressProcW,tLogProcW pLogProcW,tRequestProcW pRequestProcW)
{
	ProgressProcW=pProgressProcW;
    LogProcW=pLogProcW;
    RequestProcW=pRequestProcW;
	PluginNumber=PluginNr;
	return WSAStartup(MAKEWORD(2,2), &wsaData);
}

typedef struct {
	WCHAR PathW[wdirtypemax];
	WCHAR LastFoundNameW[wdirtypemax];
	HANDLE searchhandle;
} tLastFindStuct,*pLastFindStuct;

typedef struct {
	list<FileData*>* result;
	wstring path;
	int origlength;
} FindDataHandle;

HANDLE __stdcall FsFindFirstW(WCHAR* Path,WIN32_FIND_DATAW *FindData)
{
	cacheMap.clear();
	wstring path = Path;
	for (auto iter = path.begin(); iter != path.end(); iter++) {
		if ((*iter)==L'\\') {
			*iter = L'/';
		}
	}
	if (path.back()!=L'/') {
		path.push_back(L'/');
	}
	list<FileData*>* result = DirList(path);
	memset(FindData,0,sizeof(WIN32_FIND_DATAW));
	if (result->empty()) {
		SetLastError(ERROR_NO_MORE_FILES);
		return INVALID_HANDLE_VALUE;
	} else {
		for (auto i = result->begin(); i!= result->end(); i++) {
			cacheMap[wstring((*i)->cache_name)] = **i;
		}
		FindDataHandle * r = new FindDataHandle;
		r->path = path;

		FileData* back = result->back();
		result->pop_back();
		GetStat(FindData,back);
		delete back;

		r->result = result;
		r->origlength = result->size();
		return r;
	}
}

HANDLE __stdcall FsFindFirst(char* Path,WIN32_FIND_DATA *FindData)
{
	WIN32_FIND_DATAW FindDataW;
	WCHAR PathW[wdirtypemax];
	HANDLE retval=FsFindFirstW(awfilenamecopy(PathW,Path),&FindDataW);
	if (retval!=INVALID_HANDLE_VALUE)
		copyfinddatawa(FindData,&FindDataW);
	return retval;
}

BOOL __stdcall FsFindNextW(HANDLE Hdl,WIN32_FIND_DATAW *FindData)
{
	FindDataHandle* r = (FindDataHandle*)(Hdl);
	list<FileData*>* result = r->result;
	if (result->empty()) {
		return false;
	} else {
		FileData* str = result->back();
		result->pop_back();
		GetStat(FindData,str);
		delete str;
		return true;
	}
}

BOOL __stdcall FsFindNext(HANDLE Hdl,WIN32_FIND_DATA *FindData)
{
	WIN32_FIND_DATAW FindDataW;
	copyfinddataaw(&FindDataW,FindData);
	BOOL retval=FsFindNextW(Hdl,&FindDataW);
	if (retval)
		copyfinddatawa(FindData,&FindDataW);
	return retval;
}

int __stdcall FsFindClose(HANDLE Hdl)
{
	if ((int)Hdl==1)
		return 0;
	FindDataHandle* r = (FindDataHandle*)(Hdl);
	list<FileData*>* result = r->result;
	while (!result->empty()) {
		delete result->back();
		result->pop_back();
	}
	delete result;
	delete r;
	return 0;
}

BOOL __stdcall FsMkDir(char* Path)
{
	WCHAR wbuf[wdirtypemax];
	return FsMkDirW(awfilenamecopy(wbuf,Path));
}

BOOL __stdcall FsMkDirW(WCHAR* Path)
{
	if (wcslen(Path)<pluginrootlen+2)
		return false;
	return CreateDirectoryT(Path+pluginrootlen,NULL);
}

int __stdcall FsExecuteFile(HWND MainWin,char* RemoteName,char* Verb)
{
    SHELLEXECUTEINFO shex;
	if (strlen(RemoteName)<pluginrootlen+2)
		return FS_EXEC_ERROR;
	if (stricmp(Verb,"open")==0) {
		return FS_EXEC_YOURSELF;
	} else if (stricmp(Verb,"properties")==0) {
        memset(&shex,0,sizeof(shex));
		shex.fMask=SEE_MASK_INVOKEIDLIST;
        shex.cbSize=sizeof(shex);
		shex.nShow=SW_SHOW;
		shex.hwnd=MainWin;
		shex.lpVerb=Verb;
		shex.lpFile=RemoteName+pluginrootlen;
		if (!ShellExecuteEx(&shex))
			return FS_EXEC_ERROR;
		else
			return FS_EXEC_OK;

	} else
		return FS_EXEC_ERROR;
}

int __stdcall FsExecuteFileW(HWND MainWin,WCHAR* RemoteName,WCHAR* Verb)
{
    SHELLEXECUTEINFOW shex;
	if (wcslen(RemoteName)<pluginrootlen+2)
		return FS_EXEC_ERROR;
	if (wcsicmp(Verb,L"open")==0) {
		return FS_EXEC_YOURSELF;
	} else if (wcsicmp(Verb,L"properties")==0) {
        memset(&shex,0,sizeof(shex));
		shex.fMask=SEE_MASK_INVOKEIDLIST;
        shex.cbSize=sizeof(shex);
		shex.nShow=SW_SHOW;
		shex.hwnd=MainWin;
		shex.lpVerb=Verb;
		shex.lpFile=RemoteName+pluginrootlen;
		if (!ShellExecuteExW(&shex))
			return FS_EXEC_ERROR;
		else
			return FS_EXEC_OK;

	} else
		return FS_EXEC_ERROR;
}

int __stdcall FsRenMovFile(char* OldName,char* NewName,BOOL Move,BOOL OverWrite,RemoteInfoStruct* ri)
{
	WCHAR OldNameW[wdirtypemax],NewNameW[wdirtypemax];
	return FsRenMovFileW(awfilenamecopy(OldNameW,OldName),awfilenamecopy(NewNameW,NewName),Move,OverWrite,ri);
}

int __stdcall FsRenMovFileW(WCHAR* OldName,WCHAR* NewName,BOOL Move,BOOL OverWrite,RemoteInfoStruct* ri)
{
	if (wcslen(OldName)<pluginrootlen+2 || wcslen(NewName)<pluginrootlen+2)
		return FS_FILE_NOTFOUND;

	int err=ProgressProcT(PluginNumber,OldName,NewName,0);
	if (err)
		return FS_FILE_USERABORT;

	if (Move) {
		if (OverWrite)
			DeleteFileT(NewName+pluginrootlen);
		if (MoveFileT(OldName+pluginrootlen,NewName+pluginrootlen))
			return FS_FILE_OK;
	} else {
		if (CopyFileT(OldName+pluginrootlen,NewName+pluginrootlen,!OverWrite))
			return FS_FILE_OK;
	}
	switch(GetLastError()) {
		case ERROR_FILE_NOT_FOUND:
		case ERROR_PATH_NOT_FOUND:
		case ERROR_ACCESS_DENIED:
			return FS_FILE_NOTFOUND;
		case ERROR_FILE_EXISTS:
			return FS_FILE_EXISTS;
		default:
			return FS_FILE_WRITEERROR;
	}
	err=ProgressProcT(PluginNumber,OldName,NewName,100);
	if (err)
		return FS_FILE_USERABORT;
}

int __stdcall FsGetFileW(WCHAR* RemoteName,WCHAR* LocalName,int CopyFlags,RemoteInfoStruct* ri)
{
    int err;
	BOOL ok,OverWrite,Resume,Move;

	OverWrite=CopyFlags & FS_COPYFLAGS_OVERWRITE;
	Resume=CopyFlags & FS_COPYFLAGS_RESUME;
	Move=CopyFlags & FS_COPYFLAGS_MOVE;

	if (Resume)
		return FS_FILE_NOTSUPPORTED;

	if (wcslen(RemoteName)<pluginrootlen+2)
		return FS_FILE_NOTFOUND;
	
	err=ProgressProcT(PluginNumber,RemoteName,LocalName,0);
	if (err)
		return FS_FILE_USERABORT;
	if (Move) {
		if (OverWrite)
			DeleteFileT(LocalName);
		ok=MoveFileT(RemoteName+pluginrootlen,LocalName);
	} else
		ok=CopyFileT(RemoteName+pluginrootlen,LocalName,!OverWrite);
	
	if (ok) {
		ProgressProcT(PluginNumber,RemoteName,LocalName,100);
		return FS_FILE_OK;
	} else {
		err=GetLastError();
		switch (err) {
		case 2:
		case 3:
		case 4:
		case 5:
			return FS_FILE_NOTFOUND;
		case ERROR_FILE_EXISTS:
			return FS_FILE_EXISTS;
		default:
			return FS_FILE_READERROR;
		}
	}
}

int __stdcall FsGetFile(char* RemoteName,char* LocalName,int CopyFlags,RemoteInfoStruct* ri)
{
	WCHAR RemoteNameW[wdirtypemax],LocalNameW[wdirtypemax];
	return FsGetFileW(awfilenamecopy(RemoteNameW,RemoteName),awfilenamecopy(LocalNameW,LocalName),CopyFlags,ri);
}

int __stdcall FsPutFileW(WCHAR* LocalName,WCHAR* RemoteName,int CopyFlags)
{
    int err;
	BOOL ok,OverWrite,Resume,Move;

	OverWrite=CopyFlags & FS_COPYFLAGS_OVERWRITE;
	Resume=CopyFlags & FS_COPYFLAGS_RESUME;
	Move=CopyFlags & FS_COPYFLAGS_MOVE;
	if (Resume)
		return FS_FILE_NOTSUPPORTED;

	if (wcslen(RemoteName)<pluginrootlen+2)
		return FS_FILE_NOTFOUND;
	
	err=ProgressProcT(PluginNumber,LocalName,RemoteName,0);
	if (err)
		return FS_FILE_USERABORT;
	if (Move) {
		if (OverWrite)
			DeleteFileT(RemoteName+pluginrootlen);
		ok=MoveFileT(LocalName,RemoteName+pluginrootlen);
	} else
		ok=CopyFileT(LocalName,RemoteName+pluginrootlen,!OverWrite);

	if (ok) {
		ProgressProcT(PluginNumber,RemoteName,LocalName,100);
		return FS_FILE_OK;
	} else {
		err=GetLastError();
		switch (err) {
		case 2:
		case 3:
		case 4:
		case 5:
			return FS_FILE_NOTFOUND;
		case ERROR_FILE_EXISTS:
			return FS_FILE_EXISTS;
		default:
			return FS_FILE_READERROR;
		}
	}
}

int __stdcall FsPutFile(char* LocalName,char* RemoteName,int CopyFlags)
{
	WCHAR LocalNameW[wdirtypemax],RemoteNameW[wdirtypemax];
	return FsPutFileW(awfilenamecopy(LocalNameW,LocalName),awfilenamecopy(RemoteNameW,RemoteName),CopyFlags);
}

BOOL __stdcall FsDeleteFileW(WCHAR* RemoteName)
{
	if (wcslen(RemoteName)<pluginrootlen+2)
		return false;

	return DeleteFileT(RemoteName+pluginrootlen);	
}

BOOL __stdcall FsDeleteFile(char* RemoteName)
{
	WCHAR RemoteNameW[wdirtypemax];
	return FsDeleteFileW(awfilenamecopy(RemoteNameW,RemoteName));
}

BOOL __stdcall FsRemoveDirW(WCHAR* RemoteName)
{
	if (wcslen(RemoteName)<pluginrootlen+2)
		return false;

	return RemoveDirectoryT(RemoteName+pluginrootlen);	
}

BOOL __stdcall FsRemoveDir(char* RemoteName)
{
	WCHAR RemoteNameW[wdirtypemax];
	return FsRemoveDirW(awfilenamecopy(RemoteNameW,RemoteName));
}

BOOL __stdcall FsSetAttrW(WCHAR* RemoteName,int NewAttr)
{
	if (wcslen(RemoteName)<pluginrootlen+2)
		return false;

	if (NewAttr==0)
		NewAttr=FILE_ATTRIBUTE_NORMAL;
	return SetFileAttributesT(RemoteName+pluginrootlen,NewAttr);	
}

BOOL __stdcall FsSetAttr(char* RemoteName,int NewAttr)
{
	WCHAR RemoteNameW[wdirtypemax];
	return FsSetAttrW(awfilenamecopy(RemoteNameW,RemoteName),NewAttr);
}

BOOL __stdcall FsSetTimeW(WCHAR* RemoteName,FILETIME *CreationTime,
      FILETIME *LastAccessTime,FILETIME *LastWriteTime)
{
	if (wcslen(RemoteName)<pluginrootlen+2)
		return false;

	HANDLE filehandle = CreateFileT(RemoteName+pluginrootlen,	
                      GENERIC_WRITE,          // Open for writing
                      0,                      // Do not share
                      NULL,                   // No security
                      OPEN_EXISTING,          // Existing file only
                      FILE_ATTRIBUTE_NORMAL,  // Normal file
                      NULL);

	if (filehandle==INVALID_HANDLE_VALUE)
		return FALSE;

	BOOL retval=SetFileTime(filehandle,CreationTime,LastAccessTime,LastWriteTime);
    CloseHandle(filehandle);
	return retval;
}

BOOL __stdcall FsSetTime(char* RemoteName,FILETIME *CreationTime,
      FILETIME *LastAccessTime,FILETIME *LastWriteTime)
{
	WCHAR RemoteNameW[wdirtypemax];
	return FsSetTimeW(awfilenamecopy(RemoteNameW,RemoteName),CreationTime,
		LastAccessTime,LastWriteTime);
}

void __stdcall FsStatusInfo(char* RemoteDir,int InfoStartEnd,int InfoOperation)
{
	// This function may be used to initialize variables and to flush buffers
	
/*	char text[wdirtypemax];

	if (InfoStartEnd==FS_STATUS_START)
		strcpy(text,"Start: ");
	else
		strcpy(text,"End: ");
	
	switch (InfoOperation) {
	case FS_STATUS_OP_LIST:
		strcat(text,"Get directory list");
		break;
	case FS_STATUS_OP_GET_SINGLE:
		strcat(text,"Get single file");
		break;
	case FS_STATUS_OP_GET_MULTI:
		strcat(text,"Get multiple files");
		break;
	case FS_STATUS_OP_PUT_SINGLE:
		strcat(text,"Put single file");
		break;
	case FS_STATUS_OP_PUT_MULTI:
		strcat(text,"Put multiple files");
		break;
	case FS_STATUS_OP_RENMOV_SINGLE:
		strcat(text,"Rename/Move/Remote copy single file");
		break;
	case FS_STATUS_OP_RENMOV_MULTI:
		strcat(text,"Rename/Move/Remote copy multiple files");
		break;
	case FS_STATUS_OP_DELETE:
		strcat(text,"Delete multiple files");
		break;
	case FS_STATUS_OP_ATTRIB:
		strcat(text,"Change attributes of multiple files");
		break;
	case FS_STATUS_OP_MKDIR:
		strcat(text,"Create directory");
		break;
	case FS_STATUS_OP_EXEC:
		strcat(text,"Execute file or command line");
		break;
	case FS_STATUS_OP_CALCSIZE:
		strcat(text,"Calculate space occupied by a directory");
		break;
	case FS_STATUS_OP_SEARCH:
		strcat(text,"Search for file names");
		break;
	case FS_STATUS_OP_SEARCH_TEXT:
		strcat(text,"Search for text in files");
		break;
	case FS_STATUS_OP_SYNC_SEARCH:
		strcat(text,"Search files for sync comparison");
		break;
	case FS_STATUS_OP_SYNC_GET:
		strcat(text,"download files during sync");
		break;
	case FS_STATUS_OP_SYNC_PUT:
		strcat(text,"Upload files during sync");
		break;
	case FS_STATUS_OP_SYNC_DELETE:
		strcat(text,"Delete files during sync");
		break;
	default:
		strcat(text,"Unknown operation");
	}
	if (InfoOperation != FS_STATUS_OP_LIST)   // avoid recursion due to re-reading!
		MessageBox(0,text,RemoteDir,0);
*/
}

void __stdcall FsGetDefRootName(char* DefRootName,int maxlen)
{
	strlcpy(DefRootName,"Android",maxlen);
}

int __stdcall FsExtractCustomIconW(WCHAR* RemoteName,int ExtractFlags,HICON* TheIcon)
{
	WCHAR* p;
	BOOL success,isdirectory;

	if (!RemoteName[0])
		return FS_ICON_USEDEFAULT;
	// Note: directories have a backslash at the end!
	p=RemoteName+wcslen(RemoteName)-1;
	isdirectory=p[0]=='\\';

	if (isdirectory) {
		// Sample: show drive icons for drive level
		if (wcslen(RemoteName)<=4 && wcscmp(RemoteName,L"\\..\\")!=0) {
			if (ExtractFlags & FS_ICONFLAG_SMALL)  // use LoadImage, because LoadIcon produces ugly results
				*TheIcon=(HICON)LoadImage((HINSTANCE)hinst,"ICON2",IMAGE_ICON,16,16,LR_SHARED);
			else
  				*TheIcon=LoadIcon((HINSTANCE)hinst,"ICON2");
			wcscpy(RemoteName,L"ICON2");   // Use it as identifier so the icon is only stored once
			return FS_ICON_EXTRACTED;
		}
	} else {	
		// Sample: extract custom icons for EXE files
		p=wcsrchr(RemoteName,'.');
		if (p && wcsicmp(p,L".exe")==0) {
			if (ExtractFlags & FS_ICONFLAG_BACKGROUND) {
				if (ExtractFlags & FS_ICONFLAG_SMALL)
					success=ExtractIconExT(RemoteName+pluginrootlen,0,NULL,TheIcon,1)==1;
				else
					success=ExtractIconExT(RemoteName+pluginrootlen,0,TheIcon,NULL,1)==1;
				if (success)
					return FS_ICON_EXTRACTED_DESTROY;  // must be destroyed with DestroyIcon!!!
			} else
				return FS_ICON_DELAYED;
		}
	}
	return FS_ICON_USEDEFAULT;
}

int __stdcall FsExtractCustomIcon(char* RemoteName,int ExtractFlags,HICON* TheIcon)
{
	WCHAR RemoteNameW[wdirtypemax],OldNameW[wdirtypemax];
	awfilenamecopy(RemoteNameW,RemoteName);
	wcscpy(OldNameW,RemoteNameW);
	int retval=FsExtractCustomIconW(RemoteNameW,ExtractFlags,TheIcon);
	if (wcscmp(OldNameW,RemoteNameW)!=0)
		wafilenamecopy(RemoteName,RemoteNameW);
	return retval;
}

int __stdcall FsGetPreviewBitmap(char* RemoteName,int width,int height,HBITMAP* ReturnedBitmap)
{
	if (strlen(RemoteName)<=4) {
		 if (strcmp(RemoteName,"\\..\\")==0)
			 return FS_BITMAP_NONE;
		 else {
			int w,h,bigx,bigy;
			int stretchx,stretchy;
			OSVERSIONINFO vx;
			BOOL is_nt;
			BITMAP bmpobj;
			HBITMAP bmp_image,bmp_thumbnail,oldbmp_image,oldbmp_thumbnail;
			HDC maindc,dc_thumbnail,dc_image;
			POINT pt;

			// check for operating system: Windows 9x does NOT support the HALFTONE stretchblt mode!
			vx.dwOSVersionInfoSize=sizeof(vx);
			GetVersionEx(&vx);
			is_nt=vx.dwPlatformId==VER_PLATFORM_WIN32_NT;

			
			bmp_image=LoadBitmap((HINSTANCE)hinst,"BITMAP1");
			if (bmp_image && GetObject(bmp_image,sizeof(bmpobj),&bmpobj)) {
				bigx=bmpobj.bmWidth;
				bigy=bmpobj.bmHeight;
				// do we need to stretch?
				if ((bigx>=width || bigy>=height) && (bigx>0 && bigy>0)) {
					stretchy=MulDiv(width,bigy,bigx);
					if (stretchy<=height) {
						w=width;
						h=stretchy;
						if (h<1) h=1;
					} else {
						stretchx=MulDiv(height,bigx,bigy);
						w=stretchx;
						if (w<1) w=1;
						h=height;
					}

					maindc=GetDC(GetDesktopWindow());
					dc_thumbnail=CreateCompatibleDC(maindc);
					dc_image=CreateCompatibleDC(maindc);
					bmp_thumbnail=CreateCompatibleBitmap(maindc,w,h);
					ReleaseDC(GetDesktopWindow(),maindc);
					oldbmp_image=(HBITMAP)SelectObject(dc_image,bmp_image);
					oldbmp_thumbnail=(HBITMAP)SelectObject(dc_thumbnail,bmp_thumbnail);
					if(is_nt) {
						SetStretchBltMode(dc_thumbnail,HALFTONE);
						SetBrushOrgEx(dc_thumbnail,0,0,&pt);

					} else {
						SetStretchBltMode(dc_thumbnail,COLORONCOLOR);
					}
					StretchBlt(dc_thumbnail,0,0,w,h,dc_image,0,0,bigx,bigy,SRCCOPY);
					SelectObject(dc_image,oldbmp_image);
					SelectObject(dc_thumbnail,oldbmp_thumbnail);
					DeleteDC(dc_image);
					DeleteDC(dc_thumbnail);
					DeleteObject(bmp_image);
					*ReturnedBitmap=bmp_thumbnail;
					return FS_BITMAP_EXTRACTED;
				} else {
					*ReturnedBitmap=bmp_image;
					return FS_BITMAP_EXTRACTED;
				}
			}
			return FS_BITMAP_NONE;
		}
	} else {
		memmove(RemoteName,RemoteName+pluginrootlen,strlen(RemoteName+pluginrootlen)+1);
		return FS_BITMAP_EXTRACT_YOURSELF | FS_BITMAP_CACHE;
	}
}

int __stdcall FsGetPreviewBitmapW(WCHAR* RemoteName,int width,int height,HBITMAP* ReturnedBitmap)
{
	if (wcslen(RemoteName)<=4) {
		if (wcscmp(RemoteName,L"\\..\\")==0)
			return FS_BITMAP_NONE;
		else {
			return FsGetPreviewBitmap("\\",width,height,ReturnedBitmap);
		}
	} else {
		memmove(RemoteName,RemoteName+pluginrootlen,2*wcslen(RemoteName+pluginrootlen)+2);
		return FS_BITMAP_EXTRACT_YOURSELF | FS_BITMAP_CACHE;
	}
}

void __stdcall FsSetDefaultParams(FsDefaultParamStruct* dps)
{
	strlcpy(inifilename,dps->DefaultIniName,MAX_PATH-1);
}

BOOL __stdcall FsLinksToLocalFiles()
{
	return true;
}

BOOL __stdcall FsGetLocalName(char* RemoteName,int maxlen)
{
	if (strlen(RemoteName)<pluginrootlen+2)
		return false;
	MoveMemory (RemoteName,RemoteName+pluginrootlen,strlen(RemoteName+pluginrootlen)+1);
	return true;
}

BOOL __stdcall FsGetLocalNameW(WCHAR* RemoteName,int maxlen)
{
	if (wcslen(RemoteName)<pluginrootlen+2)
		return false;
	MoveMemory(RemoteName,RemoteName+pluginrootlen,2*wcslen(RemoteName+pluginrootlen)+2);
	return true;
}

/**************************************************************************************/
/*********************** content plugin = custom columns part! ************************/
/**************************************************************************************/

#define fieldcount 5
char* fieldnames[fieldcount]={"mode","uid","gid","type","name"};
	//"size","creationdate","writedate","accessdate","size-delayed","size-ondemand"};

int fieldtypes[fieldcount]={ft_string,ft_numeric_32,ft_numeric_32,ft_string,ft_string};
		//ft_numeric_64,ft_datetime,ft_datetime,ft_datetime,ft_numeric_64,ft_numeric_64};

char* fieldunits_and_multiplechoicestrings[fieldcount]={"","","","",""};
		//"bytes|kbytes|Mbytes|Gbytes","","","","bytes|kbytes|Mbytes|Gbytes","bytes|kbytes|Mbytes|Gbytes"};

int fieldflags[fieldcount]={0,0,0,0,0};
    //contflags_substsize,contflags_edit,contflags_substdatetime,contflags_edit,contflags_substsize,contflags_substsize | contflags_edit};

int sortorders[fieldcount]={-1,-1,-1,-1,-1};
	//-1,-1,-1,-1,-1,-1};


int __stdcall FsContentGetSupportedField(int FieldIndex,char* FieldName,char* Units,int maxlen)
{
	if (FieldIndex<0 || FieldIndex>=fieldcount)
		return ft_nomorefields;
	strlcpy(FieldName,fieldnames[FieldIndex],maxlen-1);
	strlcpy(Units,fieldunits_and_multiplechoicestrings[FieldIndex],maxlen-1);
	return fieldtypes[FieldIndex];
}

int __stdcall FsContentGetValueT(BOOL unicode,WCHAR* FileName,int FieldIndex,int UnitIndex,void* FieldValue,int maxlen,int flags)
{
	WIN32_FIND_DATAW fd;
	HANDLE fh;
	__int64 filesize;

	wstring path = FileName;
	for (auto iter = path.begin(); iter != path.end(); iter++) {
		if ((*iter)==L'\\') {
			*iter = L'/';
		}
	}

	if (cacheMap.find(path) != cacheMap.end()) {
		FileData* fd = &cacheMap[path];
		switch (FieldIndex) {
		case 0: {
				char* text = (char*)FieldValue;
				strcpy(text,"--- --- ---");
				if (fd->mode & 0400) { text[0]='r'; }
				if (fd->mode & 0200) { text[1]='w'; }
				if (fd->mode & 0100) { text[2]='x'; }
				if (fd->mode & 040) { text[4]='r'; }
				if (fd->mode & 020) { text[5]='w'; }
				if (fd->mode & 010) { text[6]='x'; }
				if (fd->mode & 04) { text[8]='r'; }
				if (fd->mode & 02) { text[9]='w'; }
				if (fd->mode & 01) { text[10]='x'; }
			}
			break;
		case 1:
			*(int*)FieldValue = fd->uid;
			break;
		case 2:
			*(int*)FieldValue = fd->gid;
			break;
		case 3: {
				char* text = (char*)FieldValue;
				if (fd->type==REGFILE) {
					strcpy(text,"file");
				} else if (fd->type==DIRECTORY) {
					strcpy(text,"dir");
				} else if (fd->type==LINK) {
					strcpy(text,"link");
				} else {
					strcpy(text,"other");
				}
				break;
			}
		case 4: {
			walcopy((char*)FieldValue,fd->alt_name.c_str(),maxlen);
			break;
			}
		default:
			return ft_nosuchfield;
		}
	} else {
		return ft_fileerror;
	}
	return fieldtypes[FieldIndex];  // very important!
}

int __stdcall FsContentGetValueW(WCHAR* FileName,int FieldIndex,int UnitIndex,void* FieldValue,int maxlen,int flags)
{
	return FsContentGetValueT(true,FileName,FieldIndex,UnitIndex,FieldValue,maxlen,flags);
}

int __stdcall FsContentGetValue(char* FileName,int FieldIndex,int UnitIndex,void* FieldValue,int maxlen,int flags)
{
	WCHAR FileNameW[wdirtypemax];
	return FsContentGetValueT(false,awfilenamecopy(FileNameW,FileName),FieldIndex,UnitIndex,FieldValue,maxlen,flags);
}

int __stdcall FsContentGetSupportedFieldFlags(int FieldIndex)
{
	if (FieldIndex==-1)
		return contflags_substmask | contflags_edit;
	else if (FieldIndex<0 || FieldIndex>=fieldcount)
		return 0;
	else
		return fieldflags[FieldIndex];
}

int __stdcall FsContentGetDefaultSortOrder(int FieldIndex)
{
	if (FieldIndex<0 || FieldIndex>=fieldcount)
		return 1;
	else 
		return sortorders[FieldIndex];
}

BOOL __stdcall FsContentGetDefaultView(char* ViewContents,char* ViewHeaders,char* ViewWidths,char* ViewOptions,int maxlen)
{
	strlcpy(ViewContents,"[=tc.size]\\n[=<fs>.mode]\\n[=<fs>.uid]\\n[=<fs>.gid]\\n[=<fs>.type]\\n[=<fs>.name]",maxlen);  // separated by backslash and n, not new lines!
	strlcpy(ViewHeaders,"size\\nmode\\nuid\\ngid\\ntype\\nname",maxlen);  // titles in ENGLISH also separated by backslash and n, not new lines!
	strlcpy(ViewWidths,"148,23,-35,40,-18,-18,16,148",maxlen);
	strlcpy(ViewOptions,"-1|0",maxlen);  // auto-adjust-width, or -1 for no adjust | horizonal scrollbar flag
	return true;
}

int __stdcall FsContentSetValueW(WCHAR* FileName,int FieldIndex,int UnitIndex,int FieldType,void* FieldValue,int flags)
{
	int retval=ft_nomorefields;
	FILETIME oldcreationtime,newcreationtime;
	FILETIME *p1,*p2,*FieldTime;
	SYSTEMTIME st1,st2;
	HANDLE f;

	if (FileName==NULL)     // indicates end of operation -> may be used to flush data
		return ft_nosuchfield;

	if (FieldIndex<0 || FieldIndex>=fieldcount)
		return ft_nosuchfield;
	else if ((fieldflags[FieldIndex] & 1)==0)
		return ft_nosuchfield;
	else {
		switch (FieldIndex) {
		case 1:  // "creationdate"
		case 3:  // "accessdate"
			FieldTime=(FILETIME*)FieldValue;
			p1=NULL;p2=NULL;
			if (FieldIndex==1)
				p1=&oldcreationtime;
			else
				p2=&oldcreationtime;

			f= CreateFileT(FileName+pluginrootlen,	
                      GENERIC_READ|GENERIC_WRITE, // Open for reading+writing
                      0,                      // Do not share
                      NULL,                   // No security
                      OPEN_EXISTING,          // Existing file only
                      FILE_ATTRIBUTE_NORMAL,  // Normal file
                      NULL);
			if (flags & setflags_only_date) {
				GetFileTime(f,p1,p2,NULL);
				FileTimeToLocalFileTime(&oldcreationtime,&newcreationtime);
				FileTimeToSystemTime(&newcreationtime,&st2);
				FileTimeToLocalFileTime(FieldTime,&newcreationtime);
				FileTimeToSystemTime(&newcreationtime,&st1);
				st1.wHour=st2.wHour;
				st1.wMinute=st2.wMinute;
				st1.wSecond=st2.wSecond;
				st1.wMilliseconds=st2.wMilliseconds;
				SystemTimeToFileTime(&st1,&newcreationtime);
				LocalFileTimeToFileTime(&newcreationtime,&oldcreationtime);
			} else
				oldcreationtime=*FieldTime;
			if (!SetFileTime(f,p1,p2,NULL))
				retval=ft_fileerror;
			CloseHandle(f);
			break;
		}
	}
	return retval;
}

int __stdcall FsContentSetValue(char* FileName,int FieldIndex,int UnitIndex,int FieldType,void* FieldValue,int flags)
{
	WCHAR FileNameW[wdirtypemax];
	return FsContentSetValueW(awfilenamecopy(FileNameW,FileName),FieldIndex,UnitIndex,FieldType,FieldValue,flags);
}

void __stdcall FsContentPluginUnloading(void)
{
	// If you do something in a background thread, you may
	// wait in this function until the thread has finished
	// its work to prevent Total Commander from closing!
	// MessageBox(0,"fsplugin unloading!","Test",0);
}
