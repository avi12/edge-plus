// atomic_file.h — a file under %LOCALAPPDATA% the panel reads, written whole to
// a temporary file and moved over the old one, so the panel never reads half of
// it. Best effort: a file that cannot be written is left as it was.
#pragma once
#include <windows.h>
#include <string>
#include "core/config.h"
#include "core/handles.h"

namespace edgeplus::atomic_file {

inline constexpr char kTemporarySuffix[] = ".tmp";

inline void WriteUnderLocalAppData(const char* relativePath, const void* bytes,
                                   DWORD size) {
  const std::string path = config::LocalAppDataPath(relativePath);
  if (path.empty()) {
    return;
  }
  const std::string temporaryPath = path + kTemporarySuffix;
  {
    const handles::UniqueHandle file{
        CreateFileA(temporaryPath.c_str(), GENERIC_WRITE, 0, nullptr,
                    CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if (!file) {
      return;
    }
    DWORD written = 0;
    const bool isWritten =
        WriteFile(file.Get(), bytes, size, &written, nullptr) != FALSE;
    if (!isWritten) {
      return;
    }
  }
  MoveFileExA(temporaryPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING);
}

}  // namespace edgeplus::atomic_file
