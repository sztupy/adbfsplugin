#include "StdAfx.h"

enum FileTypeEnum {
	REGFILE, DIRECTORY, LINK, OTHER
};

class FileData {
public:
	FileData(std::wstring _name) { type=REGFILE; mode=0; size=0;accessTime=0;modificationTime=0;changeTime=0;uid=0;gid=0; name = _name; alt_name = _name; cache_name = _name; };
	//FileData(FileData& other) { type = other.type; mode = other.mode; size = other.size; accessTime = other.accessTime; modificationTime = other.modificationTime; changeTime = other.changeTime; uid = other.uid; gid = other.gid; name = other.name; alt_name = other.alt_name; }; 
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

std::list<FileData*>* DirList(std::wstring filename);
void GetStat(WIN32_FIND_DATAW* fs, FileData* fd);