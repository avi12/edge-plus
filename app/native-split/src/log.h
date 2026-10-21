// Logging — one concern: a line to the debugger and to the panel-readable file
// %LOCALAPPDATA%\FixEdge\split-native.log. Header-only; the single TU includes it.
#pragma once
#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include "handles.h"

namespace fixedge {

inline constexpr size_t kLogLineCapacity = 512;
// The formatted message's share of the line: the '\n' and terminator Log
// appends need the last two bytes.
inline constexpr int kLogMessageCapacity =
    static_cast<int>(kLogLineCapacity) - 2;
inline constexpr wchar_t kLogFileUnderLocalAppData[] =
    L"\\FixEdge\\split-native.log";

inline void Log(const char* format, ...) {
  char line[kLogLineCapacity];
  va_list arguments{};
  va_start(arguments, format);
  int length = vsnprintf(line, kLogMessageCapacity, format, arguments);
  va_end(arguments);
  const bool isFormatError = length < 0;
  if (isFormatError) {
    return;
  }
  // vsnprintf returns the length the whole message WOULD have; a longer one was
  // cut to fit, so the '\n' goes after what was actually written.
  const bool isTruncated = length >= kLogMessageCapacity;
  if (isTruncated) {
    length = kLogMessageCapacity - 1;
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
  const handles::UniqueHandle file{
      CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                  nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)};
  if (!file) {
    return;
  }
  DWORD written = 0;
  WriteFile(file.Get(), line, static_cast<DWORD>(length), &written, nullptr);
}

}  // namespace fixedge
