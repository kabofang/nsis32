#include "plugin_parse.h"
#include <windows.h>
#include <stdlib.h>

int parse_config(const wchar_t* input, struct CompressPluginEntry* entries) {
  int entry_count = 0;
  int line_type = 0;
  int pos = 0;
  int line_start = 0;

  while (input[pos] != L'\0' && entry_count < MAX_ENTRIES) {
    if (input[pos] == L'\n' || input[pos] == L'\0') {
      int line_end = pos;
      if (line_end > line_start && input[line_end - 1] == L'\r') {
        line_end--;
      }

      for (int i = line_start, j = 0; i < line_end; i++, j++) {
        if (j >= MAX_LINE_LEN - 1) break;

        if (line_type == 0) {
          entries[entry_count].filename[j] = input[i];
        }
        else {
          entries[entry_count].args[j] = input[i];
        }
      }

      if (line_type == 0) {
        entries[entry_count].filename[line_end - line_start] = L'\0';
      }
      else {
        entries[entry_count].args[line_end - line_start] = L'\0';
        entry_count++;
      }

      line_type = !line_type;
      line_start = pos + 1;
    }
    pos++;
  }
  return entry_count;
}

int parse_plugin_compress_file(const wchar_t* filename, struct CompressPluginEntry* entries) {
  HANDLE hFile = INVALID_HANDLE_VALUE;
  DWORD dwBytesRead = 0;
  BYTE* buffer = NULL;
  wchar_t* wbuffer = NULL;
  int ret = -1;

  hFile = CreateFileW(
    filename,
    GENERIC_READ,
    FILE_SHARE_READ,
    NULL,
    OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL,
    NULL
  );

  if (hFile == INVALID_HANDLE_VALUE) {
    return -1;
  }

  LARGE_INTEGER fileSize;
  if (!GetFileSizeEx(hFile, &fileSize)) {
    CloseHandle(hFile);
    return -2;
  }

  const DWORD maxSize = 4 * 1024 * 1024;
  if (fileSize.QuadPart > maxSize) {
    CloseHandle(hFile);
    return -3;
  }

  buffer = (BYTE*)malloc(fileSize.LowPart + 2);
  if (!buffer) {
    CloseHandle(hFile);
    return -4;
  }

  if (!ReadFile(hFile, buffer, fileSize.LowPart, &dwBytesRead, NULL)) {
    free(buffer);
    CloseHandle(hFile);
    return -5;
  }

  buffer[dwBytesRead] = 0;
  buffer[dwBytesRead + 1] = 0;

  int wchars = MultiByteToWideChar(
    CP_UTF8,
    0,
    (LPCCH)buffer,
    -1,
    NULL,
    0
  );

  wbuffer = (wchar_t*)malloc(wchars * sizeof(wchar_t));
  if (!wbuffer) {
    free(buffer);
    CloseHandle(hFile);
    return -6;
  }

  MultiByteToWideChar(
    CP_UTF8,
    0,
    (LPCCH)buffer,
    -1,
    wbuffer,
    wchars
  );

  ret = parse_config(wbuffer, entries);

  free(buffer);
  free(wbuffer);
  CloseHandle(hFile);

  return ret;
}