#pragma once

#include <tchar.h>
#include <set>
#include <windows.h>
#include <Shlobj.h>
#include "tstring.h"
#define MAX_PATH 260
class CEXEBuild;

void ClearDirectory(const tstring& path);
BOOL GetOrCreateTempDirectory(tstring& path, const tstring& random_str);
tstring GetCurrentModuleDir();
bool SyncCall7zSync(const tstring& szCommand, CEXEBuild* build);
DWORD GetFileSizeWrapper(const std::wstring& filePath);
bool ReadFirstLineW(const wchar_t* file_path, wchar_t* buffer, size_t buffer_size);
long long GetCommonFileSize(const wchar_t* path);
int WonameCopy(const wchar_t* src_path, const wchar_t* dst_path);
BOOL WaitForDeleteFile(const LPCTSTR lpFileName, DWORD dwMilliseconds);