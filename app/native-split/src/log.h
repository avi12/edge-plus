// Logging — one concern: a line to the debugger and to the panel-readable file
// %LOCALAPPDATA%\FixEdge\split-native.log. Header-only; the single TU includes it.
#pragma once
#include <windows.h>
#include <cstdarg>
#include <cstdio>

namespace fixedge {

inline constexpr size_t kLogLineCapacity = 512;
inline constexpr wchar_t kLogFileUnderLocalAppData[] =
    L"\\FixEdge\\split-native.log";

inline void Log(const char* format, ...) {
  char line[kLogLineCapacity];
  va_list arguments;
  va_start(arguments, format);
  // Two bytes short of the buffer: room for the '\n' and terminator added below.
  int length = vsnprintf(line, sizeof(line) - 2, format, arguments);
  va_end(arguments);
  const bool isFormatError = length < 0;
  if (isFormatError) {
    return;
  }
  line[length] = '\n';
  length++;
  line[length] = '\0';
  OutputDebugStringA(line);

  wchar_t path[MAX_PATH];
  const bool hasLocalAppData =
      GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH) != 0;
  if (!hasLocalAppData) {
    return;
  }
  wcscat_s(path, kLogFileUnderLocalAppData);
  HANDLE file =
      CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                  nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  const bool isFileOpen = file != INVALID_HANDLE_VALUE;
  if (!isFileOpen) {
    return;
  }
  DWORD written = 0;
  WriteFile(file, line, static_cast<DWORD>(length), &written, nullptr);
  CloseHandle(file);
}

}  // namespace fixedge
