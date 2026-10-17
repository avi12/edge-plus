// glic_library.h — Chrome's glic client library, from an installed Chrome. One
// concern: find Chrome's install, read its resources.pak (public program files,
// never a user profile) and pull out the script Chrome hands the Gemini page as
// its "glic API source", so the host (glic_host_script.h) can offer it to the
// page. Nothing of Google's is copied into fix-edge or onto disk: the text is
// read from the user's own Chrome, once, and kept in memory.
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "edge.h"
#include "pak.h"

namespace fixedge::glic_library {

// Where Chrome lives, newest version folder wins; the pak is beside chrome.dll.
inline constexpr wchar_t kInstallRoots[][MAX_PATH] = {
    L"C:\\Program Files\\Google\\Chrome\\Application",
    L"C:\\Program Files (x86)\\Google\\Chrome\\Application",
};
inline constexpr wchar_t kPakName[] = L"resources.pak";

// What the library is recognised by, and how big it can be (about 500 KB in
// Chrome 155; the window only spares decompressing every other resource).
inline constexpr char kLibraryMarker[] = "internalAutoGlicBoot";
inline constexpr size_t kMinLibraryBytes = size_t{100} * 1024;
inline constexpr size_t kMaxLibraryBytes = size_t{4} * 1024 * 1024;

inline std::vector<uint8_t> ReadWholeFile(const std::wstring& path) {
  std::vector<uint8_t> bytes;
  HANDLE file =
      CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return bytes;
  }
  LARGE_INTEGER size{};
  if (GetFileSizeEx(file, &size) && size.QuadPart > 0) {
    bytes.resize(static_cast<size_t>(size.QuadPart));
    DWORD read = 0;
    const bool isRead =
        ReadFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &read,
                 nullptr) != 0 &&
        read == bytes.size();
    if (!isRead) {
      bytes.clear();
    }
  }
  CloseHandle(file);
  return bytes;
}

// The pak of the newest Chrome version folder under `root`, or an empty path.
inline std::wstring NewestPak(const std::wstring& root) {
  WIN32_FIND_DATAW found{};
  HANDLE search = FindFirstFileW((root + L"\\*").c_str(), &found);
  if (search == INVALID_HANDLE_VALUE) {
    return std::wstring();
  }
  std::wstring newest;
  do {
    const bool isVersionFolder =
        (found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
        found.cFileName[0] >= L'0' && found.cFileName[0] <= L'9';
    if (!isVersionFolder) {
      continue;
    }
    const std::wstring pak = root + L"\\" + found.cFileName + L"\\" + kPakName;
    const bool isNewer =
        GetFileAttributesW(pak.c_str()) != INVALID_FILE_ATTRIBUTES &&
        lstrcmpW(found.cFileName, newest.empty() ? L"" : newest.c_str()) > 0;
    if (isNewer) {
      newest = found.cFileName;
    }
  } while (FindNextFileW(search, &found) != 0);
  FindClose(search);
  return newest.empty() ? std::wstring()
                        : root + L"\\" + newest + L"\\" + kPakName;
}

// Edge's own decoders for what a pak holds.
inline bool DecompressWithEdge(pak::Encoding encoding, const uint8_t* data,
                               size_t size, size_t decodedSize,
                               std::string* out) {
  return encoding == pak::Encoding::kBrotli
             ? edge::BrotliDecompress(data, size, decodedSize, out)
             : edge::GzipDecompress(data, size, decodedSize, out);
}

// The glic client library text, or empty when no Chrome is installed or its
// resources do not hold one. Read once; later calls return the same.
inline const std::string& Load() {
  static bool isLoaded = false;
  static std::string library;
  if (isLoaded) {
    return library;
  }
  isLoaded = true;
  for (const auto& root : kInstallRoots) {
    const std::wstring pakPath = NewestPak(root);
    if (pakPath.empty()) {
      continue;
    }
    const std::vector<uint8_t> file = ReadWholeFile(pakPath);
    library = pak::FindContaining(file, kLibraryMarker, kMinLibraryBytes,
                                  kMaxLibraryBytes, DecompressWithEdge);
    Log("[glic] %ls: library %zu bytes", pakPath.c_str(), library.size());
    if (!library.empty()) {
      break;
    }
  }
  return library;
}

}  // namespace fixedge::glic_library
