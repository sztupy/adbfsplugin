#include "StdAfx.h"
#include "adbhandler.h"

using namespace std;

PipeHandler::PipeHandler()
{
	SECURITY_ATTRIBUTES saAttr;
	ZeroMemory(&inf, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&saAttr, sizeof(SECURITY_ATTRIBUTES));
	saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
	saAttr.bInheritHandle = TRUE;
	saAttr.lpSecurityDescriptor = NULL;
	stdout_rd = NULL;
	stdout_wr = NULL;
	stdin_rd = NULL;
	stdin_wr = NULL;
	stderr_rd = NULL;
	stderr_wr = NULL;
	BOOL retval = CreatePipe(&stdout_rd, &stdout_wr, &saAttr, 0);
	retval = retval && SetHandleInformation(stdout_rd,HANDLE_FLAG_INHERIT,0);
	retval = retval && CreatePipe(&stdin_rd, &stdin_wr, &saAttr, 0);
	retval = retval && SetHandleInformation(stdin_wr,HANDLE_FLAG_INHERIT,0);
	retval = retval && CreatePipe(&stderr_rd, &stderr_wr, &saAttr, 0);
	retval = retval && SetHandleInformation(stderr_rd,HANDLE_FLAG_INHERIT,0);
	if (! retval) {
		throw wstring(L"<0001 - Handle generation failed>");
	}
}

PipeHandler::~PipeHandler() {
/*	if (stdout_rd) CloseHandle(&stdout_rd);
	if (stdout_wr) CloseHandle(&stdout_wr);
	if (stdin_wr) CloseHandle(&stdin_wr);
	if (stdin_rd) CloseHandle(&stdin_rd);
	if (stderr_rd) CloseHandle(&stderr_rd);
	if (stderr_wr) CloseHandle(&stderr_wr);

	if (inf.hProcess) CloseHandle(inf.hProcess);
	if (inf.hThread) CloseHandle(inf.hThread);*/
}

string trim( string const& str, const char* sepSet)
{
	std::string::size_type const first = str.find_first_not_of(sepSet);
	return ( first==std::string::npos ) ? std::string() : str.substr(first, str.find_last_not_of(sepSet)-first+2);
}

wstring* PipeHandler::ReadWLine() {
	string input = "";
	DWORD bytesRead;
	char a;
	BOOL bResult = ReadFile(stdout_rd, &a, 1, &bytesRead, NULL);
	while (bResult && (bytesRead!=0) && (a!='\n')) {
		input += a;
		bResult = ReadFile(stdout_rd, &a, 1, &bytesRead, NULL);
	}
	if (input.empty()) {
		return NULL;
	}
	input = trim(input," \t\r\n");
	int wide = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), input.length(), NULL, 0);
	LPWSTR output = new wchar_t[wide];
	MultiByteToWideChar(CP_UTF8, 0, input.c_str(), input.length(), output, wide);
	return new wstring(output,wide-1);
}

bool PipeHandler::WriteWLine(wstring w) {
	return false;
}


/* Get adbfsplugin  directory, and replace dll with adb.exe */

LPWSTR __adb__filename = NULL;
LPWSTR GetAdbFileName() {
	if (__adb__filename) return __adb__filename;
	
	__adb__filename = new WCHAR[1024];
	GetModuleFileNameW( GetModuleHandle("adbfsplugin.wfx"), __adb__filename, 1024 );
	FILE* f = fopen("d:\\log.txt","w+");fprintf(f,"%ls",__adb__filename);fclose(f);
	LPWSTR filename = PathFindFileNameW(__adb__filename);
	wcscpy_s(filename,16,L"adb.exe");
	return __adb__filename;
}

/* Run Command */

PipeHandler* RunCommand(LPCWSTR command) {
	PROCESS_INFORMATION piProcInfo;
	STARTUPINFOW siStartInfo;
	
	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFOW));

	PipeHandler* ph = new PipeHandler();

	siStartInfo.cb = sizeof(STARTUPINFOW);
	siStartInfo.hStdError = ph->stderr_wr;
	siStartInfo.hStdOutput = ph->stdout_wr;
	siStartInfo.hStdInput = ph->stdin_rd;

	siStartInfo.dwFlags |= STARTF_USESTDHANDLES;

	WCHAR* comm = new WCHAR[wcslen(command)+1];
	wcscpy(comm,command);
	BOOL retval = CreateProcessW(GetAdbFileName(),comm,NULL,NULL,TRUE,CREATE_NO_WINDOW|CREATE_UNICODE_ENVIRONMENT,NULL,NULL,&siStartInfo,&piProcInfo);
	delete comm;
	if ( ! retval ) {
		delete ph;
		throw wstring(L"<0002 - ADB could not be started>");
	}
	retval = retval && CloseHandle(ph->stderr_wr);
	retval = retval && CloseHandle(ph->stdout_wr);
	retval = retval && CloseHandle(ph->stdin_rd);
	ph->stderr_wr = NULL;
	ph->stdout_wr = NULL;
	ph->stdin_rd = NULL;
	if ( ! retval ) {
		delete ph;
		throw wstring(L"<0003 - Pipe generation failed>");
	}

	ph->inf = piProcInfo;

	return ph;
};

void FillStat(wstring directory, list<FileData*>* fd) {
	try {
		wstring command = L"adb.exe shell busybox stat -c \"%a -%F- %g %u %s %X %Y %Z %N\" ";
		for (auto i = fd->begin(); i != fd->end(); i++) {
			command.append(L" \"");
			command.append(directory);
			command.append((*i)->name);
			command.append(L"\"");
		}
		PipeHandler* ph = RunCommand(command.c_str());
		wstring* line = ph->ReadWLine();
		auto i = fd->begin();
		while ((line!=NULL) && (i!=fd->end())) {
			(*i)->cache_name = directory+(*i)->name;
			int mode = 0;;
			WCHAR type[100];
			WCHAR name[1024];
			ZeroMemory(type,sizeof(type));
			ZeroMemory(name,sizeof(name));
			DWORD gid = 0;
			DWORD uid = 0;
			unsigned __int64 size = 0;
			DWORD time1 = 0,time2 = 0,time3 = 0;
			int res = swscanf_s(line->c_str(),L"%o -%[a-zA-Z ]- %u %u %I64i %u %u %u %[^\n]",&mode,type,countof(type),&gid,&uid,&size,&time1,&time2,&time3,name,countof(name));
			if (res<9) {
				(*i)->alt_name = L"<0005 - stat failed>";
			} else {
				(*i)->accessTime = time1;
				(*i)->modificationTime = time2;
				(*i)->changeTime = time3;
				(*i)->uid = uid;
				(*i)->gid = gid;
				(*i)->mode = mode;
				(*i)->size = size;
				(*i)->type = OTHER;
				if (wcscmp(type,L"directory")==0) {
					(*i)->type = DIRECTORY;
				} else if (wcscmp(type, L"symbolic link")==0) {
					(*i)->type = LINK;
				} else if (wcscmp(type, L"regular file")==0) {
					(*i)->type = REGFILE;
				}
				(*i)->alt_name = name;
			}
			line = ph->ReadWLine();
			i++;
		}
		delete ph;
	} catch (wstring e) {
	}
}

void GetStat(WIN32_FIND_DATAW* fs, FileData* fd) {
	memset(fs,0,sizeof(WIN32_FIND_DATAW));
	wcslcpy(fs->cFileName,fd->name.c_str(),countof(fs->cFileName)-1);
	fs->dwFileAttributes = 0x80000000;
	if (fd->type == DIRECTORY) {
		fs->dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;
	}
	fs->dwReserved0 = fd->mode;
	fs->nFileSizeHigh = fd->size >> 32;
	fs->nFileSizeLow = fd->size & 0x0FFFFFFFF;
}

list<FileData*>* DirList(wstring filename) {
	auto* result = new list<FileData*>();
	try {
		PipeHandler* ph = RunCommand((wstring(L"adb.exe shell busybox ls --color=never -1 ") + filename).c_str());
		wstring* line = ph->ReadWLine();
		while (line!=NULL) {
			result->push_back(new FileData(*line));
			delete line;
			line = ph->ReadWLine();
		}
		delete ph;
	} catch (wstring e) {
		result->push_back(new FileData(e));
	}

	// Get stat in batches of 10 files
	auto* l = new list<FileData*>();
	for (auto i = result->begin(); i!= result->end(); i++) {
		l->push_back(*i);
		if (l->size()>10) {
			FillStat(filename,l);
			l->clear();
		}
	}
	if (!l->empty()) {
		FillStat(filename,l);
	}
	delete l;
	return result;
}

