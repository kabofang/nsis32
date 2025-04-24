#include "file_utils.h"
#include <windows.h>
#include <Shlobj.h>

#define CHECK_FILE7Z_VALID(valid) if(!valid) return false;

const tstring tar_name = _T("install.tar");
const tstring install7z_name = _T("install.7z");

void ClearDirectory(const tstring & path) {
  if (path.empty()) {
    return;
  }
  WIN32_FIND_DATA findData;
  HANDLE hFind = FindFirstFile((path + L"\\*").c_str(), &findData);

  if (hFind != INVALID_HANDLE_VALUE) {
    do {
      if (wcscmp(findData.cFileName, L".") == 0 ||
        wcscmp(findData.cFileName, L"..") == 0) continue;

      std::wstring fullPath = path + L"\\" + findData.cFileName;

      if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
        ClearDirectory(fullPath);
        RemoveDirectory(fullPath.c_str());
      }
      else {
        DeleteFile(fullPath.c_str());
      }
    } while (FindNextFile(hFind, &findData));
    FindClose(hFind);
  }
}

BOOL GetOrCreateTempDirectory(tstring &path) {
  path.resize(MAX_PATH * 2);
  DWORD dwRet = GetTempPath(path.size(), LPTSTR(path.c_str()));
  if (dwRet == 0) {
    return FALSE;
  }
  size_t len = _tcslen(path.c_str());
  if (len > 0 && (path[len - 1] == '\\' || path[len - 1] == '/')) {
    path[--len] = '\0';
  }
  path = tstring(path.c_str()) + _T("\\xnsis_temp");

  LPTSTR pszPath = LPTSTR(path.c_str());
  DWORD dwAttrib = GetFileAttributes(pszPath);
  if (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY)) {
    ClearDirectory(pszPath);
    return true;
  }
  int err = SHCreateDirectoryEx(NULL, pszPath, NULL);
  if (err == ERROR_SUCCESS) {
    return TRUE;
  }
  SetLastError(err);
  return FALSE;
}

tstring GetCurrentModuleDir() {
  wchar_t szPath[MAX_PATH] = { 0 };
  GetModuleFileNameW(NULL, szPath, MAX_PATH);

  tstring path(szPath);
  size_t lastSlash = path.find_last_of(L"\\/");
  if (lastSlash != tstring::npos) {
    return path.substr(0, lastSlash + 1);
  }
  return L".\\";
}

bool SyncCall7zSync(const tstring& szCommand) {
  STARTUPINFOW si = { sizeof(STARTUPINFOW) };
  PROCESS_INFORMATION pi = { 0 };
  BOOL bSuccess = FALSE;
  DWORD dwExitCode = 0;

  std::wstring sz7zPath = GetCurrentModuleDir() + L"7z.exe";
  tstring cmdLine = sz7zPath + L" " + szCommand;

  bSuccess = ::CreateProcessW(
    NULL,
    const_cast<LPWSTR>(cmdLine.c_str()),
    NULL,
    NULL,
    FALSE,
    0,
    NULL,
    NULL,
    &si,
    &pi
  );

  if (!bSuccess) {
    printf("CreateProcess failed. Error: %d", GetLastError());
    return false;
  }

  ::WaitForSingleObject(pi.hProcess, INFINITE);
  ::GetExitCodeProcess(pi.hProcess, &dwExitCode);
  ::CloseHandle(pi.hProcess);
  ::CloseHandle(pi.hThread);

  return (dwExitCode == 0);
}

DWORD GetFileSizeWrapper(const std::wstring& filePath) {
  HANDLE hFile = CreateFile(
    filePath.c_str(),
    GENERIC_READ,
    FILE_SHARE_READ,
    NULL,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    NULL
  );
  DWORD fileSize{};

  if (hFile == INVALID_HANDLE_VALUE) {
    return fileSize;
  }

  fileSize = GetFileSize(hFile, NULL);
  if (fileSize == INVALID_FILE_SIZE) {
    CloseHandle(hFile);
    fileSize = 0;
    return fileSize;
  }
  CloseHandle(hFile);
  return fileSize;
}

bool ReadFirstLineW(const wchar_t* file_path, wchar_t* buffer, size_t buffer_size) {
  HANDLE hFile = INVALID_HANDLE_VALUE;
  bool success = false;
  const DWORD MAX_READ_SIZE = 4096;
  char raw_buffer[MAX_READ_SIZE] = { 0 };

  hFile = CreateFileW(
    file_path,
    GENERIC_READ,
    FILE_SHARE_READ,
    NULL,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    NULL
  );
  if (hFile == INVALID_HANDLE_VALUE) {
    return false;
  }

  DWORD bytes_read = 0;
  if (!ReadFile(hFile, raw_buffer, MAX_READ_SIZE - 1, &bytes_read, NULL)) {
    CloseHandle(hFile);
    return false;
  }

  DWORD line_end = 0;
  while (line_end < bytes_read) {
    if (raw_buffer[line_end] == '\r' || raw_buffer[line_end] == '\n') {
      break;
    }
    line_end++;
  }

  if (line_end > 0) {
    int converted = MultiByteToWideChar(
      CP_ACP,
      0,
      raw_buffer,
      line_end,
      buffer,
      (int)buffer_size - 1
    );
    if (converted > 0) {
      buffer[converted] = L'\0';
      success = true;
    }
  }

  CloseHandle(hFile);
  return success;
}