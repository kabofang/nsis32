#include "file7z.h"
#include "file_utils.h"
#include "growbuf.h"
#include "strlist.h"
#include "build.h"
#include "plugin_parse.h"

#define CHECK_FILE7Z_VALID(valid) if(!valid) return false;

tstring tar_name;
tstring install7z_name;

File7z::File7z() {
  //MessageBox(NULL, L"", L"", MB_OK);
  std::srand(std::time(0));
  int random_num = std::rand();
  tstring random_str = std::to_wstring(random_num % 100000);
  tar_name = tstring(L"install") + random_str + L".tar";
  install7z_name = tstring(L"install") + random_str + L".7z";
  is_valid_ = GetOrCreateTempDirectory(xnsis_path_, random_str);
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

wchar_t* File7z::GetInstall7zName() {
  return const_cast<wchar_t*>(install7z_name.c_str());
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
  if (!SyncCall7zSync(cmd)) {
    return 0;
  }
  ++count_;
  DWORD file_size2 = GetFileSizeWrapper(tar_path_);
  auto diff = file_size2 - file_size1 ? file_size2 - file_size1 : 1;
  return file_size2 - file_size1;
}

int File7z::AddSrcFile(const tstring& path, const tstring& oname) {
  CHECK_FILE7Z_VALID(is_valid_);
  int file_size = GetCommonFileSize(path.c_str());
  if (file_size < 0) {
    return 0;
  }
  if (WonameCopy(path.c_str(), (xnsis_path_ + _T("\\") + oname).c_str()) != 0) {
    return 0;
  }
  file_size = file_size ? file_size : 1;
  return file_size;
}

bool File7z::GenerateInstall7z(CEXEBuild* build,int& build_compress) {
  CHECK_FILE7Z_VALID(is_valid_);
  if (count_ == 0) {
    return true;
  }
  count_ = 0;

  tstring tar_other_cmd = tstring(_T("a -sdel ")) + L" \"" + tar_path_ + _T("\" \"") + xnsis_path_ + _T("\\*\"") + _T(" -x!") + tar_name;
  if (!SyncCall7zSync(tar_other_cmd)) {
    return false;
  }

  tstring exctr_cmd = tstring(_T("x \"")) + tar_path_ + _T("\" -o\"") + xnsis_path_ + _T("\" -aos");
  if (!SyncCall7zSync(exctr_cmd)) {
    return false;
  }
  DeleteFile(tar_path_.c_str());

  std::wstring sz7zPath = GetCurrentModuleDir() + L"plugin_compress.ini";
  CompressPluginEntry entrys[MAX_ENTRIES];
  int count = parse_plugin_compress_file(sz7zPath.c_str(), (CompressPluginEntry*)entrys);
  tstring exclude_param;
  for (int i = 0; i < count; ++i) {
    tstring real_file_path = xnsis_path_ + _T("\\") + entrys[i].filename;
    tstring plugin_path = xnsis_path_ + _T("\\") + entrys[i].filename + L".nsisbin";
    tstring exctr_cmd = tstring(_T("x \"")) + real_file_path + _T("\" -o\"") + plugin_path + _T("\"");
    if (!SyncCall7zSync(exctr_cmd)) {
      return false;
    }
    DeleteFile(real_file_path.c_str());
    exclude_param = exclude_param + L" -x!" + entrys[i].filename;
  }
  tstring compress_cmd = tstring(_T("a ")) + param_7z_cmd_.c_str() + exclude_param + L" \"" + install7z_path_ + _T("\" \"") + xnsis_path_ + _T("\\*\"");
  if (!SyncCall7zSync(compress_cmd)) {
    return false;
  }
  auto exec_script = [build](const tstring& cmd) {
      StringList hist;
      GrowBuf linedata;
      build->ps_addtoline(cmd.c_str(), linedata, hist);
      linedata.add(_T(""), sizeof(_T("")));
      return build->doParse((TCHAR*)linedata.get());
      };
  static const int cmd_count = 5;
  tstring cmd_list[cmd_count];
  cmd_list[0] = _T("SetCompress off");
  cmd_list[1] = _T("SetOutPath \"$INSTDIR\"");
  cmd_list[2] = tstring(_T("File /n7z ")) + install7z_path_;
  if (count != 0) {
    cmd_list[3] = tstring(_T("File /n7z ")) + sz7zPath;
  }
  //off\0auto\0force\0
  switch (build_compress) {
    case 0: {
    } break;
    case 1: {
      cmd_list[4] = _T("SetCompress auto");
    } break;
    case 2: {
      cmd_list[4] = _T("SetCompress force");
    } break;
  }

  for (int i = 0; i < cmd_count; ++i) {
    if (cmd_list[i].empty()) {
      continue;
    }
    if (exec_script(cmd_list[i].c_str()) != PS_OK) {
      return false;
    }
  }
  ClearDirectory(xnsis_path_);
  return true;
}
