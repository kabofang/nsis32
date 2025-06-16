#pragma once

#include <tchar.h>
#include <set>
#include "tstring.h"
#define MAX_PATH 260

class CEXEBuild;
class File7z {
public:
  File7z();
  int AddSrcFile(const tstring& path, int recurse, const std::set<tstring>&);
  int AddSrcFile(const tstring& path, const tstring& oname);

  bool GenerateInstall7z(CEXEBuild*, int& build_compress);
  wchar_t* GetInstall7zName();
 
  bool install_name_seted_ = false;
private:
  tstring xnsis_path_;
  tstring tar_path_;
  tstring install7z_path_;
  tstring param_7z_cmd_;
  int count_{};
  bool is_valid_{};
};