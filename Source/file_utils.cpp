#include "file_utils.h"
#include <windows.h>
#include <Shlobj.h>
#include <WinError.h>
#include "build.h"

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

BOOL GetOrCreateTempDirectory(tstring &path, const tstring &random_str) {
  path.resize(MAX_PATH * 2);
  DWORD dwRet = GetTempPath(path.size(), LPTSTR(path.c_str()));
  if (dwRet == 0) {
    return FALSE;
  }
  size_t len = _tcslen(path.c_str());
  if (len > 0 && (path[len - 1] == '\\' || path[len - 1] == '/')) {
    path[--len] = '\0';
  }
  path = tstring(path.c_str()) + _T("\\xnsis_temp") + random_str;

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

bool SyncCall7zSync(const tstring& szCommand, CEXEBuild* build) {
  build->INFO_MSG(_T("XNSIS: 7z cmd, %") NPRIs _T("\n"), szCommand.c_str());
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
    build->ERROR_MSG(_T("XNSIS: CreateProcess failed, %d\n"), GetLastError());
    return false;
  }

  ::WaitForSingleObject(pi.hProcess, INFINITE);
  ::GetExitCodeProcess(pi.hProcess, &dwExitCode);
  ::CloseHandle(pi.hProcess);
  ::CloseHandle(pi.hThread);
  if (dwExitCode != 0) {
    build->ERROR_MSG(_T("XNSIS: 7z cmd failed\n"));
  }

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

long long GetCommonFileSize(const wchar_t* path) {
  WIN32_FIND_DATAW find_data;
  HANDLE hFind = FindFirstFileW(path, &find_data);

  if (hFind == INVALID_HANDLE_VALUE) {
    // 转换Windows错误码为errno
    switch (GetLastError()) {
    case ERROR_FILE_NOT_FOUND:
    case ERROR_PATH_NOT_FOUND:
      errno = ENOENT;
      break;
    case ERROR_ACCESS_DENIED:
      errno = EACCES;
      break;
    default:
      errno = EIO;
    }
    return -1;
  }

  FindClose(hFind);

  // 检查是否为目录
  if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
    errno = EISDIR;
    return -1;
  }

  // 组合64位文件尺寸
  ULARGE_INTEGER size;
  size.HighPart = find_data.nFileSizeHigh;
  size.LowPart = find_data.nFileSizeLow;
  return (long long)size.QuadPart;
}

void ModifyPathSpec(TCHAR(&szDst)[MAX_PATH], BOOL bAddSpec)
{
  int nLen = lstrlen(szDst);
  TCHAR  ch = szDst[nLen - 1];
  if ((ch == _T('\\')) || (ch == _T('/')))
  {
    if (!bAddSpec)
    {
      szDst[nLen - 1] = _T('\0');
    }
  }
  else
  {
    if (bAddSpec)
    {
      szDst[nLen] = _T('\\');
      szDst[nLen + 1] = _T('\0');
    }
  }
}

inline BOOL CreateDirectoryNested(LPCTSTR lpszDir)
{
  if (::PathIsDirectory(lpszDir)) return TRUE;

  TCHAR   szPreDir[MAX_PATH];
  _tcscpy_s(szPreDir, lpszDir);
  //确保路径末尾没有反斜杠
  ModifyPathSpec(szPreDir, FALSE);

  //获取上级目录
  BOOL  bGetPreDir = ::PathRemoveFileSpec(szPreDir);
  if (!bGetPreDir) return FALSE;

  //如果上级目录不存在,则递归创建上级目录
  if (!::PathIsDirectory(szPreDir))
  {
    CreateDirectoryNested(szPreDir);
  }

  return ::CreateDirectory(lpszDir, NULL);
}

// 主功能函数
int WonameCopy(const wchar_t* src_path, const wchar_t* dst_path) {
  // 处理长路径
  const wchar_t* real_src = src_path;
  const wchar_t* real_dst = dst_path;

  // 验证源文件
  DWORD src_attr = GetFileAttributesW(real_src);
  if (src_attr == INVALID_FILE_ATTRIBUTES || (src_attr & FILE_ATTRIBUTE_DIRECTORY)) {
    errno = ENOENT;
    return -1;
  }

  // 分离目标路径
  wchar_t drive[_MAX_DRIVE] = { 0 };
  wchar_t dir[_MAX_DIR] = { 0 };
  wchar_t fname[_MAX_FNAME] = { 0 };
  wchar_t ext[_MAX_EXT] = { 0 };
  _wsplitpath_s(real_dst, drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, ext, _MAX_EXT);

  // 构建目标目录
  wchar_t target_dir[MAX_PATH] = { 0 };
  _wmakepath_s(target_dir, MAX_PATH, drive, dir, L"", L"");

  // 创建目录结构
  if (!CreateDirectoryNested(target_dir)) {
    errno = EIO;
    return -1;
  }

  // 执行文件复制
  if (!CopyFileW(real_src, real_dst, FALSE)) {
    DWORD err = GetLastError();
    if (err == ERROR_FILE_NOT_FOUND) {
      errno = ENOENT;
    }
    else {
      errno = EIO;
    }
    return -1;
  }

  return 0;
}

BOOL WaitForDeleteFile(const LPCTSTR lpFileName, DWORD dwMilliseconds)
{
  // 参数校验
  if (lpFileName == NULL || lpFileName[0] == _T('\0')) {
    SetLastError(ERROR_INVALID_PARAMETER);
    return FALSE;
  }

  const DWORD dwStartTick = GetTickCount();
  DWORD dwRetryInterval = 100; // 重试间隔(ms)
  BOOL bSuccess = FALSE;

  do {
    // 尝试删除文件
    if (DeleteFile(lpFileName)) {
      bSuccess = TRUE;
      break;
    }

    // 检查是否因为文件不存在而失败
    if (GetLastError() == ERROR_FILE_NOT_FOUND) {
      bSuccess = TRUE;
      break;
    }

    // 检查是否超时
    if (GetTickCount() - dwStartTick > dwMilliseconds) {
      break;
    }

    // 动态调整重试间隔（指数退避算法）
    if (dwRetryInterval < 1000) {
      dwRetryInterval *= 2;
    }

    Sleep(dwRetryInterval);

  } while (TRUE);

  // 最终确认文件是否已删除
  if (bSuccess) {
    WIN32_FIND_DATA fd;
    HANDLE hFind = FindFirstFile(lpFileName, &fd);
    if (hFind != INVALID_HANDLE_VALUE) {
      FindClose(hFind);
      bSuccess = FALSE; // 文件仍然存在
      SetLastError(ERROR_ACCESS_DENIED);
    }
  }

  return bSuccess;
}