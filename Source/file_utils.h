#pragma once

#include <tchar.h>
#include <set>
#include <windows.h>
#include <Shlobj.h>
#include "tstring.h"
#define MAX_PATH 260

void ClearDirectory(const tstring& path);
BOOL GetOrCreateTempDirectory(tstring& path);
tstring GetCurrentModuleDir();
bool SyncCall7zSync(const tstring& szCommand);
DWORD GetFileSizeWrapper(const std::wstring& filePath);
bool ReadFirstLineW(const wchar_t* file_path, wchar_t* buffer, size_t buffer_size);