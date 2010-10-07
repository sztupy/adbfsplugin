#include "StdAfx.h"

#define BUF_SIZE 8192
// Singleton
class AdbCommunicator {
public:
	static AdbCommunicator* instance() { if (!_global_adb) _global_adb = new AdbCommunicator(); return _global_adb; };
	static void disconnect() { if (_global_adb) delete _global_adb; _global_adb = NULL; }
	std::wstring* ReadLineW();
	std::string* ReadLine();
	int PutData(const char* data, int length);
	void CleanBuffer(bool timeout);
	void PushCommandW(std::wstring command);
	void SetSU(bool needsu);
private:
	AdbCommunicator();
	~AdbCommunicator();
	void ReConnect();
	void Close();
	void SendStringToServer(char* str);
	int ReadBuf(void);

	SOCKET s;
	bool _needsu;
	static AdbCommunicator* _global_adb;

	char actbuf[BUF_SIZE];
	char* actbufpospoint;
	int actbufsize;
	int actbufpos;
};

enum FileTypeEnum {
	REGFILE, DIRECTORY, LINK, OTHER
};

class FileData {
public:
	FileData(std::wstring _name) { type=REGFILE; mode=0; size=0;accessTime=0;modificationTime=0;changeTime=0;uid=0;gid=0; name = _name; alt_name = _name; cache_name = _name; };
	FileData() { type=REGFILE; mode=0; size=0;accessTime=0;modificationTime=0;changeTime=0;uid=0;gid=0; name = L""; alt_name = L""; cache_name = L""; };
	~FileData() { };
	FileTypeEnum type;
	unsigned int mode;
	__int64 size;
	unsigned int accessTime, modificationTime, changeTime;
	unsigned int uid, gid;
	std::wstring alt_name;
	std::wstring name;
	std::wstring cache_name;
};

int decode64(const char* input, char* output);
int encode64(const char* input, char* output);

std::list<FileData*>* DirList(std::wstring filename);
void GetStat(WIN32_FIND_DATAW* fs, FileData* fd);
bool RunCommand(std::wstring comm);
std::wstring QuoteString(std::wstring str);