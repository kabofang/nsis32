#include "file7z.h"
#include "file_utils.h"
#include "growbuf.h"
#include "strlist.h"
#include "build.h"

#define CHECK_FILE7Z_VALID(valid) if(!valid) return false;

const tstring tar_name = _T("install.tar");
const tstring install7z_name = _T("install.7z");

File7z::File7z() {
  // MessageBox(NULL, L"", L"", MB_OK);
  is_valid_ = GetOrCreateTempDirectory(xnsis_path_);
  if (is_valid_) {
    ClearDirectory(xnsis_path_);
    tar_path_ = xnsis_path_ + _T("\\") + tar_name;
    install7z_path_ = xnsis_path_ + _T("\\") + install7z_name;
  }
  std::wstring sz7zPath = GetCurrentModuleDir() + L"config7z.ini";
  param_7z_cmd_.resize(4096);
  if (!ReadFirstLineW(sz7zPath.c_str(), const_cast<wchar_t*>(param_7z_cmd_.c_str()), param_7z_cmd_.size())) {
    param_7z_cmd_ = L"-t7z -m0=lzma:fb=273 -mx=9 -md=256M -ms=4G -mmt=2";
  }
}

int File7z::AddSrcFile(const tstring& path, int recurse, const std::set<tstring>& excluded) {
  CHECK_FILE7Z_VALID(is_valid_);
  DWORD file_size1 = GetFileSizeWrapper(tar_path_);
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
  DWORD file_size2 = GetFileSizeWrapper(tar_path_);
  return file_size2 - file_size1;
}

bool File7z::GenerateInstall7z(CEXEBuild* build) {
  CHECK_FILE7Z_VALID(is_valid_);
  if (count_ == 0) {
    return true;
  }
  tstring exctr_cmd = tstring(_T("x \"")) + tar_path_ + _T("\" -o\"") + xnsis_path_ + _T("\"");
  SyncCall7zSync(exctr_cmd);
  DeleteFile(tar_path_.c_str());

  tstring compress_cmd = tstring(_T("a ")) + param_7z_cmd_.c_str() + L" \"" + install7z_path_ + _T("\" \"") + xnsis_path_ + _T("\\*\"");
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