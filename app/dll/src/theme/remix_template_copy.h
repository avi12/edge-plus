// remix_template_copy.h — keeps a copy of Edge's remix storage while the
// Appearance editor runs, for reading the Appearance template it applies.
// The template (the `applyColorWheelThemeV2` source) is fetched from the cloud
// and stored as a bundle row of the profile's blueprint_artifacts.db only while
// the editor uses it; the row is gone (and wiped) once the editor closes, and
// the renderer parks its copy of the source compressed. So the first theme call
// of each DLL load copies every profile's database, byte for byte and with
// every share mode allowed (Edge keeps it open), into the Edge+ data folder as
// remix-template-<profile>.db. Read-only on Edge's side. UI thread.
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "core/config.h"
#include "core/handles.h"
#include "core/log.h"

namespace edgeplus::remix_template_copy {

inline constexpr char kUserDataRelativePath[] =
    "\\Microsoft\\Edge\\User Data\\";
inline constexpr char kDatabaseRelativePath[] =
    "\\Blueprints\\blueprint_artifacts.db";
inline constexpr char kCopyPrefix[] = "\\EdgePlus\\remix-template-";
inline constexpr char kCopyExtension[] = ".db";
inline constexpr DWORD kMaxDatabaseBytes = 64u << 20;

inline constinit bool g_isCopied = false;

inline bool CopyShared(const std::string& from, const std::string& to) {
  const handles::UniqueHandle source{
      CreateFileA(from.c_str(), GENERIC_READ,
                  FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                  nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
  if (!source) {
    return false;
  }
  const DWORD size = GetFileSize(source.Get(), nullptr);
  const bool isUsableSize =
      size != INVALID_FILE_SIZE && size <= kMaxDatabaseBytes;
  if (!isUsableSize) {
    return false;
  }
  std::vector<char> bytes(size);
  DWORD read = 0;
  const bool isRead =
      ReadFile(source.Get(), bytes.data(), size, &read, nullptr) != 0 &&
      read == size;
  if (!isRead) {
    return false;
  }
  const handles::UniqueHandle target{
      CreateFileA(to.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                  FILE_ATTRIBUTE_NORMAL, nullptr)};
  if (!target) {
    return false;
  }
  DWORD written = 0;
  return WriteFile(target.Get(), bytes.data(), size, &written, nullptr) != 0 &&
         written == size;
}

// Once per DLL load, on the first Appearance theme call.
inline void KeepOnce() {
  if (g_isCopied) {
    return;
  }
  g_isCopied = true;
  const std::string userData = config::LocalAppDataPath(kUserDataRelativePath);
  if (userData.empty()) {
    return;
  }
  WIN32_FIND_DATAA found{};
  HANDLE search = FindFirstFileA((userData + "*").c_str(), &found);
  if (search == INVALID_HANDLE_VALUE) {
    return;
  }
  do {
    const bool isProfileCandidate =
        (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
        found.cFileName[0] != '.';
    if (!isProfileCandidate) {
      continue;
    }
    const std::string profile = found.cFileName;
    const std::string from = userData + profile + kDatabaseRelativePath;
    const std::string to =
        config::LocalAppDataPath(kCopyPrefix) + profile + kCopyExtension;
    if (CopyShared(from, to)) {
      Log("[remix-theme] kept the remix storage of %s for its template",
          profile.c_str());
    }
  } while (FindNextFileA(search, &found));
  FindClose(search);
}

}  // namespace edgeplus::remix_template_copy
