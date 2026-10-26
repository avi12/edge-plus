// log.h — a line to the debugger and to the panel-readable log file.
#pragma once
#include <windows.h>
#include <cstdarg>
#include <cstdio>
#include "core/handles.h"

namespace edgeplus {

inline constexpr size_t kLogLineCapacity = 512;
// The '\n' and terminator Log appends need the last two bytes.
inline constexpr int kLogMessageCapacity =
    static_cast<int>(kLogLineCapacity) - 2;
inline constexpr wchar_t kLogFileUnderLocalAppData[] =
    L"\\EdgePlus\\split-native.log";

inline constexpr wchar_t kLocalAppDataVariable[] = L"LOCALAPPDATA";

inline void Log(const char* format, ...) noexcept {
  char line[kLogLineCapacity];
  va_list arguments{};
  va_start(arguments, format);
  int length = vsnprintf(line, kLogMessageCapacity, format, arguments);
  va_end(arguments);
  const bool isFormatError = length < 0;
  if (isFormatError) {
    return;
  }
  // vsnprintf returns the length the whole message WOULD have.
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
      GetEnvironmentVariableW(kLocalAppDataVariable, path, MAX_PATH) != 0;
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

}  // namespace edgeplus
