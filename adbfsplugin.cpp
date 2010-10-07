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
		strncpy_s(p,maxlen+1,p2,maxlen);
		p[maxlen]=0;
	} else
		strcpy_s(p,maxlen+1,p2);
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
	list<FileData*>* result;
	wstring path;
	int origlength;
} FindDataHandle;

wstring PathConverter(WCHAR* Path) {
	wstring path = Path;
	for (auto iter = path.begin(); iter != path.end(); iter++) {
		if ((*iter)==L'\\') {
			*iter = L'/';
		}
	}
	return path;
}

HANDLE __stdcall FsFindFirstW(WCHAR* Path,WIN32_FIND_DATAW *FindData)
{
	cacheMap.clear();
	wstring path = PathConverter(Path);
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
	return RunCommand(L"busybox mkdir "+ QuoteString(PathConverter(Path)));
}

int __stdcall FsExecuteFile(HWND MainWin,char* RemoteName,char* Verb)
{
	return FS_EXEC_ERROR;
}

int __stdcall FsExecuteFileW(HWND MainWin,WCHAR* RemoteName,WCHAR* Verb)
{
	return FS_EXEC_ERROR;
}

int __stdcall FsRenMovFile(char* OldName,char* NewName,BOOL Move,BOOL OverWrite,RemoteInfoStruct* ri)
{
	WCHAR OldNameW[wdirtypemax],NewNameW[wdirtypemax];
	return FsRenMovFileW(awfilenamecopy(OldNameW,OldName),awfilenamecopy(NewNameW,NewName),Move,OverWrite,ri);
}

int __stdcall FsRenMovFileW(WCHAR* OldName,WCHAR* NewName,BOOL Move,BOOL OverWrite,RemoteInfoStruct* ri)
{
	if (Move) {
		return RunCommand(L"busybox mv -f " + QuoteString(PathConverter(OldName)) + L" " + QuoteString(PathConverter(NewName))) ? FS_FILE_OK : ERROR_ACCESS_DENIED;
	} else {
		return RunCommand(L"busybox cp -f " + QuoteString(PathConverter(OldName)) + L" " + QuoteString(PathConverter(NewName))) ? FS_FILE_OK : ERROR_ACCESS_DENIED;
	}
}

int __stdcall FsGetFileW(WCHAR* RemoteName,WCHAR* LocalName,int CopyFlags,RemoteInfoStruct* ri)
{
	bool exists = (GetFileAttributesW(LocalName) != INVALID_FILE_ATTRIBUTES);
	if ((exists && CopyFlags==0) || (exists && CopyFlags==FS_COPYFLAGS_MOVE)) {
		return FS_FILE_EXISTS;
	}
	FILE* f;
	_wfopen_s(&f, LocalName,L"wb+");
	if (f==NULL) return FS_FILE_WRITEERROR;
	try {
		AdbCommunicator::instance()->PushCommandW(L"busybox uuencode -m "+QuoteString(PathConverter(RemoteName))+L" x");
		string* line = AdbCommunicator::instance()->ReadLine();
		__int64 savedsize = 0;
		__int64 fullsize = ((__int64)ri->SizeHigh << 32) | ri->SizeLow;
		ProgressProcW(PluginNumber,RemoteName,LocalName,0);
		int outsize = 0;
		char out[BUF_SIZE*4];
		while (line!=NULL) {
			if (line->find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=")==wstring::npos) {
				int inputsize = line->size();
				const char* pos = line->c_str();
				while (*pos) {
					outsize += decode64(pos,out+outsize);
					pos = pos+4;
				}
			}
			if (outsize>BUF_SIZE*4-128) {
				if (fwrite(out,1,outsize,f)!=outsize) {
					return FS_FILE_WRITEERROR;
				}
				savedsize+=outsize;
				outsize=0;
				if (ProgressProcW(PluginNumber,RemoteName,LocalName,(int)((double)savedsize/fullsize*100))) {
					AdbCommunicator::disconnect();
					return FS_FILE_USERABORT;
				}
			}
			delete line;
			line = AdbCommunicator::instance()->ReadLine();
		}
		if (outsize!=0) {
			if (fwrite(out,1,outsize,f)!=outsize) {
				return FS_FILE_WRITEERROR;
			}
			outsize=0;
		}
		ProgressProcW(PluginNumber,RemoteName,LocalName,100);
		fclose(f);
		return FS_FILE_OK;
	} catch (wstring e) {
		fclose(f);
		return FS_FILE_READERROR;
	}
}

int __stdcall FsGetFile(char* RemoteName,char* LocalName,int CopyFlags,RemoteInfoStruct* ri)
{
	WCHAR RemoteNameW[wdirtypemax],LocalNameW[wdirtypemax];
	return FsGetFileW(awfilenamecopy(RemoteNameW,RemoteName),awfilenamecopy(LocalNameW,LocalName),CopyFlags,ri);
}

int __stdcall FsPutFileW(WCHAR* LocalName,WCHAR* RemoteName,int CopyFlags)
{
	FILE *f;
	_wfopen_s(&f, LocalName, L"rb");
	if (f==NULL) {
		return FS_FILE_READERROR;
	}
	try {
		AdbCommunicator::instance()->PushCommandW(L"busybox uudecode -o " + QuoteString(PathConverter(RemoteName)));
		ProgressProcW(PluginNumber,LocalName,RemoteName,0);

		AdbCommunicator::instance()->PutData("begin-base64 644 x\n",19);

		DWORD sizelo,sizehigh;
		sizelo = GetCompressedFileSizeW(LocalName,&sizehigh);
		__int64 savedsize = 0;
		__int64 fullsize = ((__int64)sizehigh << 32) | sizelo;

		char buf[45];
		int read=fread(buf,1,45,f);
		char out[61];
		out[60]='\n';		
		while (read==45) {
			int outwr=0;
			int inr=0;
			while (inr!=45) {
				encode64(buf+inr,out+outwr);
				outwr+=4;
				inr+=3;
			}
			AdbCommunicator::instance()->PutData(out,61);
			AdbCommunicator::instance()->CleanBuffer(false);
			read=fread(buf,1,45,f);
			savedsize += inr;
			if (ProgressProcW(PluginNumber,LocalName,RemoteName,(int)((double)savedsize/fullsize*100))) {
				AdbCommunicator::disconnect();
				return FS_FILE_USERABORT;
			}
		}
		if (read>0) {
			int outwr=0;
			int inr=0;
			while(read>2) {
				encode64(buf+inr,out+outwr);
				inr+=3;
				outwr+=4;
				read-=3;
			}
			if (read==2) {
				buf[inr+2]=0;
				encode64(buf+inr,out+outwr);
				outwr+=4;
				out[outwr-1]='=';
				out[outwr]='\n';
			} else if (read==1) {
				buf[inr+1]=0;
				buf[inr+2]=0;
				encode64(buf+inr,out+outwr);
				outwr+=4;
				out[outwr-2]='=';
				out[outwr-1]='=';
				out[outwr]='\n';
			}
			AdbCommunicator::instance()->PutData(out,outwr+1);
		}
		AdbCommunicator::instance()->PutData("====\x04\n",6);
		Sleep(100);
		/*string *line = AdbCommunicator::instance()->ReadLine();
		while (line!=NULL) {
			delete line;
			line = AdbCommunicator::instance()->ReadLine();
		}*/
		
		ProgressProcW(PluginNumber,LocalName,RemoteName,100);
		fclose(f);
	} catch (wstring e) {
		fclose(f);
		return FS_FILE_WRITEERROR;
	}
    return FS_FILE_OK;
}

int __stdcall FsPutFile(char* LocalName,char* RemoteName,int CopyFlags)
{
	WCHAR LocalNameW[wdirtypemax],RemoteNameW[wdirtypemax];
	return FsPutFileW(awfilenamecopy(LocalNameW,LocalName),awfilenamecopy(RemoteNameW,RemoteName),CopyFlags);
}

BOOL __stdcall FsDeleteFileW(WCHAR* RemoteName)
{
	return RunCommand(L"busybox rm " + QuoteString(PathConverter(RemoteName)));
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

	return RunCommand(L"rm -r " + QuoteString(PathConverter(RemoteName)));
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
	return false;
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
}

void __stdcall FsGetDefRootName(char* DefRootName,int maxlen)
{
	strlcpy(DefRootName,"Android",maxlen);
}

int __stdcall FsExtractCustomIconW(WCHAR* RemoteName,int ExtractFlags,HICON* TheIcon)
{
	return FS_ICON_USEDEFAULT;
}

int __stdcall FsExtractCustomIcon(char* RemoteName,int ExtractFlags,HICON* TheIcon)
{
	WCHAR RemoteNameW[wdirtypemax],OldNameW[wdirtypemax];
	awfilenamecopy(RemoteNameW,RemoteName);
	wcscpy_s(OldNameW,wdirtypemax,RemoteNameW);
	int retval=FsExtractCustomIconW(RemoteNameW,ExtractFlags,TheIcon);
	if (wcscmp(OldNameW,RemoteNameW)!=0)
		wafilenamecopy(RemoteName,RemoteNameW);
	return retval;
}

int __stdcall FsGetPreviewBitmap(char* RemoteName,int width,int height,HBITMAP* ReturnedBitmap)
{
	return FS_BITMAP_NONE;
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

/**************************************************************************************/
/*********************** content plugin = custom columns part! ************************/
/**************************************************************************************/

#define fieldcount 5
char* fieldnames[fieldcount]={"mode","uid","gid","type","name"};

int fieldtypes[fieldcount]={ft_string,ft_numeric_32,ft_numeric_32,ft_string,ft_string};

char* fieldunits_and_multiplechoicestrings[fieldcount]={"","","","",""};

int fieldflags[fieldcount]={0,0,0,0,0};

int sortorders[fieldcount]={-1,-1,-1,-1,-1};


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
	wstring path = PathConverter(FileName);

	if (cacheMap.find(path) != cacheMap.end()) {
		FileData* fd = &cacheMap[path];
		switch (FieldIndex) {
		case 0: {
				char* text = (char*)FieldValue;
				strcpy_s(text,maxlen,"--- --- ---");
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
					strcpy_s(text,maxlen,"file");
				} else if (fd->type==DIRECTORY) {
					strcpy_s(text,maxlen,"dir");
				} else if (fd->type==LINK) {
					strcpy_s(text,maxlen,"link");
				} else {
					strcpy_s(text,maxlen,"other");
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
	return ft_fileerror;
}

int __stdcall FsContentSetValue(char* FileName,int FieldIndex,int UnitIndex,int FieldType,void* FieldValue,int flags)
{
	WCHAR FileNameW[wdirtypemax];
	return FsContentSetValueW(awfilenamecopy(FileNameW,FileName),FieldIndex,UnitIndex,FieldType,FieldValue,flags);
}

void __stdcall FsContentPluginUnloading(void)
{
	AdbCommunicator::disconnect();
}

BOOL __stdcall FsDisconnect(char* DisconnectRoot) {
	AdbCommunicator::disconnect();
	return TRUE;
}

BOOL __stdcall FsDisconnectW(WCHAR* DisconnectRoot) {
	AdbCommunicator::disconnect();
	return TRUE;
}