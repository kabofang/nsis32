#pragma once
#include <windows.h>
#define MAX_ENTRIES 10 
#define MAX_LINE_LEN 512

struct CompressPluginEntry {
  wchar_t filename[MAX_LINE_LEN];
  wchar_t args[MAX_LINE_LEN];
};

#ifdef __cplusplus
extern "C" {
#endif
int parse_plugin_compress_file(const wchar_t* filename, struct CompressPluginEntry* entries);
#ifdef __cplusplus
}
#endif