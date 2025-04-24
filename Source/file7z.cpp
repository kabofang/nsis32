#include "file7z.h"
#include <windows.h>
#include <Shlobj.h>
#include "growbuf.h"
#include "strlist.h"
#include "build.h"

#define CHECK_FILE7Z_VALID(valid) if(!valid) return false;

const tstring tar_name = _T("install.tar");
const tstring install7z_name = _T("install.7z");

void ClearDirectory(const std::wstring & path) {
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
  // MessageBox(NULL, L"", L"", MB_OK);
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

File7z::File7z() {
  is_valid_ = GetOrCreateTempDirectory(xnsis_path_);
  if (is_valid_) {
    ClearDirectory(xnsis_path_);
    tar_path_ = xnsis_path_ + _T("\\") + tar_name;
    install7z_path_ = xnsis_path_ + _T("\\") + install7z_name;
  }
}

bool File7z::AddSrcFile(const tstring& path, int recurse, const std::set<tstring>& excluded) {
  CHECK_FILE7Z_VALID(is_valid_);
  tstring cmd = tstring(_T("a"));
  if (recurse) {
    cmd = cmd + _T(" -r");
  }
  for (auto& it : excluded) {
    cmd = cmd + _T(" -x!") + it.c_str();
  }
  cmd = cmd + _T(" \"") + tar_path_ + _T("\"");
  cmd = cmd + _T(" \"") + path + _T("\"");
  SyncCall7zSync(cmd);
  ++count_;
  return true;
}

bool File7z::GenerateInstall7z(CEXEBuild* build) {
  CHECK_FILE7Z_VALID(is_valid_);
  if (count_ == 0) {
    return true;
  }
  tstring exctr_cmd = tstring(_T("x \"")) + tar_path_ + _T("\" -o\"") + xnsis_path_ + _T("\"");
  SyncCall7zSync(exctr_cmd);
  DeleteFile(tar_path_.c_str());

  tstring compress_cmd = tstring(_T("a -t7z -m0=lzma:fb=256 -mx=9 -md=256M -ms=4G -mmt=2 \"")) + install7z_path_ + _T("\" \"") + xnsis_path_ + _T("\\*\"");
  SyncCall7zSync(compress_cmd);
  auto exec_script = [build](const tstring& cmd) {
      StringList hist;
      GrowBuf linedata;
      build->ps_addtoline(cmd.c_str(), linedata, hist);
      linedata.add(_T(""), sizeof(_T("")));
      return build->doParse((TCHAR*)linedata.get());
      };
  static const int cmd_count = 6;
  tstring cmd_list[cmd_count];
  cmd_list[0] = _T("Section \"Add7zInstall\"");
  cmd_list[1] = _T("SetOutPath \"$INSTDIR\"");
  cmd_list[2] = _T("SetCompress off");
  cmd_list[3] = tstring(_T("File /n7z ")) + install7z_path_;
  cmd_list[4] = _T("SetCompress auto");
  cmd_list[5] = _T("SectionEnd");
  for (int i = 0; i < cmd_count; ++i) {
    exec_script(cmd_list[i].c_str());
  }
  ClearDirectory(xnsis_path_);
  return true;
}