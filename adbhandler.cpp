#include "StdAfx.h"
#include "adbhandler.h"

using namespace std;

/* -------------------------------
   ---- Some helper functions ----
   ------------------------------- */

#define EPOCH_DIFF 0x019DB1DED53E8000LL /* 116444736000000000 nsecs */
#define RATE_DIFF 10000000 /* 100 nsecs */
/* Convert a UNIX time_t into a Windows filetime_t */
__int64 unixTimeToFileTime(unsigned int utime) {
        __int64 tconv = ((__int64)utime * RATE_DIFF) + EPOCH_DIFF;
        return tconv;
}
/* Convert a Windows filetime_t into a UNIX time_t */
unsigned int fileTimeToUnixTime(__int64 ftime) {
        unsigned int tconv = (ftime - EPOCH_DIFF) / RATE_DIFF;
        return (time_t)tconv;
}
string trim( string const& str, const char* sepSet)
{
	std::string::size_type const first = str.find_first_not_of(sepSet);
	return ( first==std::string::npos ) ? std::string() : str.substr(first, str.find_last_not_of(sepSet)-first+2);
}
/* Get adbfsplugin  directory, and replace dll with adb.exe */

LPSTR __adb__filename = NULL;
LPSTR GetAdbFileName() {
	if (__adb__filename) return __adb__filename;
	
	__adb__filename = new char[1024];
	GetModuleFileName( GetModuleHandle("adbfsplugin.wfx"), __adb__filename, 1024 );
	FILE* f = fopen("d:\\log.txt","w+");fprintf(f,"%ls",__adb__filename);fclose(f);
	LPSTR filename = PathFindFileName(__adb__filename);
	strcpy_s(filename,16,"adb.exe");
	return __adb__filename;
}

/* ---------------------------
   ---- Adb Communicator -----
   --------------------------- */

AdbCommunicator* AdbCommunicator::_global_adb = 0;

AdbCommunicator::AdbCommunicator() {
	PROCESS_INFORMATION piProcInfo;
	STARTUPINFO siStartInfo;
	ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
	ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
	siStartInfo.cb = sizeof(STARTUPINFO);

	BOOL retval = CreateProcess(GetAdbFileName(),"adb.exe start-server",NULL,NULL,TRUE,CREATE_NO_WINDOW|CREATE_UNICODE_ENVIRONMENT,NULL,NULL,&siStartInfo,&piProcInfo);
	if (!retval) {
		throw wstring(L"<0000 - Could not start ADB server>");
	}
	s = INVALID_SOCKET;
	_needsu = true;
}

void AdbCommunicator::Close() {
	closesocket(s);
	s = INVALID_SOCKET;
}

void AdbCommunicator::SendStringToServer(char* str) {
	if (send(s, str, strlen(str), 0) == SOCKET_ERROR) {
		Close();
		throw wstring(L"<0009 - could not switch to usb mode>");
	}

	// get result
	char recbuf[5];
	recbuf[4]='\0';
	int bytesRead = recv(s, recbuf, 4, MSG_WAITALL);
	if ((bytesRead==SOCKET_ERROR) || (bytesRead!=4)) {
		Close();
		throw wstring(L"<000A - no ack data from adb server>");
	}
	if (strcmpi("FAIL",recbuf)==0) {
		// cleanup
		recv(s,recbuf,4,MSG_WAITALL);
		int datalen;
		sscanf_s(recbuf,"%x",&datalen);
		char* data = new char[datalen+1];
		recv(s,data,datalen,MSG_WAITALL);
		Close();
		throw wstring(L"<000B - FAIL response from adb server>");
	} else if (strcmpi("OKAY",recbuf)!=0) {
		Close();
		throw wstring(L"<000C - Bad response from adb server>");
	}
}

void AdbCommunicator::ReConnect() {
	struct addrinfo *result = NULL, *ptr = NULL, hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
		
	if (getaddrinfo("127.0.0.1", "5037", &hints, &result)!=0) {
		throw wstring(L"<0007 - localhost not found>");
	}
	ptr = result;
	s = socket(ptr->ai_family, ptr->ai_socktype, ptr->ai_protocol);
	if (s==INVALID_SOCKET) {
		throw wstring(L"<0006 - socket initialization failed>");
	}

	if (connect(s, ptr->ai_addr, ptr->ai_addrlen) == SOCKET_ERROR) {
		Close();
		throw wstring(L"<0008 - could not connect to local adb server>");
	}

	// switch to usb mode
	// TODO: multiple devices support
	SendStringToServer("0012host:transport-usb");
	// start shell
	SendStringToServer("0006shell:");

	if (_needsu) {
		// TODO: very hacky
		Sleep(500); // let the shell start
		CleanBuffer(false); // remove everything in buffer
		char buf[4] = "su\n";
		send(s,buf,3,0);
		Sleep(50); // small timeout for the echo
		CleanBuffer(false); // remove echo
		CleanBuffer(true); // wait for root
		// TODO: should check whether root failed or not
	}
}

void AdbCommunicator::CleanBuffer(bool timeout) {
	TIMEVAL timeval;
	timeval.tv_sec = 0;
	timeval.tv_usec = 0;
	fd_set set;
	FD_ZERO(&set);
	FD_SET(s, &set);
	char buf[1024];
	
	// cleans input buffer
	while (select(0, &set, NULL, NULL, (timeout)?NULL:&timeval)!=0) {
		recv(s,buf,1024,0);
		if (timeout) return;
	}
}

void AdbCommunicator::PushCommandW(wstring command) {
	if (s==INVALID_SOCKET) {
		ReConnect();
		Sleep(500); // wait for the shell to start
	}

	CleanBuffer(false);

	// add some garbage data to determine where sending starts and where it stops
	command = L"echo \"===adbfspluginÈ·\" ;" + command + L" ; echo \"===adbfspluginÈ·Ì\"";

	// convert utf-16 command to utf-8
	int sizeneeded = WideCharToMultiByte(CP_UTF8,0,command.c_str(),-1,NULL,0,NULL,NULL);
	char* comm = new char[sizeneeded+3];
	WideCharToMultiByte(CP_UTF8,0,command.c_str(),-1,comm,sizeneeded+3,NULL,NULL);

	sizeneeded = strlen(comm);
	comm[sizeneeded]='\n';
	comm[sizeneeded+1]='\0';
	if (send(s,comm,sizeneeded+1,0) == SOCKET_ERROR) {
		Close();
		throw wstring(L"<000D - Command send failed>");
	}

	// throw out initial garbage
	wstring* line = ReadLineW();
	while ((line!=NULL) && (*line != L"===adbfspluginÈ·")) {
		delete line;
		line = ReadLineW();
	}
};

wstring* AdbCommunicator::ReadLineW() {
	string input = "";
	DWORD bytesRead;
	char a;
	int state=0; // check for prompt state
	bytesRead = recv(s,&a,1,0);
	while ((bytesRead!=SOCKET_ERROR) && (bytesRead!=0) && (a!='\n') && (input != "===adbfsplugin√©√°√≠")) {
		input += a;
		bytesRead = recv(s,&a,1,0);
	}
	if (bytesRead==SOCKET_ERROR) {
		Close();
		int d = WSAGetLastError();
		throw wstring(L"Socket Error");
	}
	if (input.empty() || input=="===adbfsplugin√©√°√≠") {
		return NULL;
	}
	input = trim(input," \t\r\n");
	int wide = MultiByteToWideChar(CP_UTF8, 0, input.c_str(), input.length(), NULL, 0);
	LPWSTR output = new wchar_t[wide];
	MultiByteToWideChar(CP_UTF8, 0, input.c_str(), input.length(), output, wide);
	return new wstring(output,wide-1);
}

/* ---------------------------
   ---- FileData Helpers -----
   --------------------------- */

void FillStat(wstring directory, list<FileData*>* fd) {
	try {
		wstring command = L"busybox stat -c \"%a -%F- %g %u %s %X %Y %Z %N\" ";
		for (auto i = fd->begin(); i != fd->end(); i++) {
			command.append(L" \"");
			command.append(directory);
			command.append((*i)->name);
			command.append(L"\"");
		}
		AdbCommunicator::instance()->PushCommandW(command);
		wstring* line = AdbCommunicator::instance()->ReadLineW();
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
			line = AdbCommunicator::instance()->ReadLineW();
			i++;
		}
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
		AdbCommunicator::instance()->PushCommandW((wstring(L"busybox ls --color=never -1 \"") + filename + L"\"").c_str());
		wstring* line = AdbCommunicator::instance()->ReadLineW();
		while (line!=NULL) {
			result->push_back(new FileData(*line));
			delete line;
			line = AdbCommunicator::instance()->ReadLineW();
		}
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

