// Logging — one concern: a line to the debugger and to the panel-readable file
// %LOCALAPPDATA%\FixEdge\split-native.log. Header-only; the single TU includes it.
#pragma once
#include <windows.h>
#include <cstdarg>
#include <cstdio>

namespace fixedge {

inline void Log(const char* fmt, ...) {
  char line[512];
  va_list ap;
  va_start(ap, fmt);
  int n = vsnprintf(line, sizeof(line) - 2, fmt, ap);
  va_end(ap);
  if (n < 0) {
    return;
  }
  line[n++] = '\n';
  line[n] = 0;
  OutputDebugStringA(line);

  wchar_t path[MAX_PATH];
  if (!GetEnvironmentVariableW(L"LOCALAPPDATA", path, MAX_PATH)) {
    return;
  }
  wcscat_s(path, L"\\FixEdge\\split-native.log");
  HANDLE h =
      CreateFileW(path, FILE_APPEND_DATA, FILE_SHARE_READ | FILE_SHARE_WRITE,
                  nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (h == INVALID_HANDLE_VALUE) {
    return;
  }
  DWORD wrote;
  WriteFile(h, line, static_cast<DWORD>(n), &wrote, nullptr);
  CloseHandle(h);
}

}  // namespace fixedge
