// verify.h — fail-safe self-checks run before any hook is installed. One concern:
// confirm the loaded msedge.dll is the exact build offsets.h was reversed against,
// and that the critical addresses look like code. Offsets are build-specific, so
// applying them to a different Edge build would corrupt memory — refuse instead.
#pragma once
#include <windows.h>
#include <cstdint>
#include <vector>
#include "edge.h"

#pragma comment(lib, "version.lib")

namespace fixedge::verify {

// VerQueryValueW's sub-block for the VS_FIXEDFILEINFO root.
inline constexpr wchar_t kFixedFileInfoBlock[] = L"\\";

// True only if the running msedge.dll is exactly the build offsets.h names.
inline bool BuildMatches() {
  wchar_t path[MAX_PATH];
  HMODULE module = GetModuleHandleW(edge::kModule);
  const bool hasModulePath =
      module != nullptr && GetModuleFileNameW(module, path, MAX_PATH) != 0;
  if (!hasModulePath) {
    return false;
  }

  DWORD ignored = 0;
  const DWORD versionInfoSize = GetFileVersionInfoSizeW(path, &ignored);
  if (!versionInfoSize) {
    return false;
  }
  std::vector<uint8_t> versionInfo(versionInfoSize);
  if (!GetFileVersionInfoW(path, 0, versionInfoSize, versionInfo.data())) {
    return false;
  }

  VS_FIXEDFILEINFO* info = nullptr;
  UINT infoLength = 0;
  const bool hasFixedInfo =
      VerQueryValueW(versionInfo.data(), kFixedFileInfoBlock,
                     reinterpret_cast<void**>(&info), &infoLength) &&
      info != nullptr;
  if (!hasFixedInfo) {
    return false;
  }
  const int major = HIWORD(info->dwFileVersionMS);
  const int minor = LOWORD(info->dwFileVersionMS);
  const int build = HIWORD(info->dwFileVersionLS);
  const int revision = LOWORD(info->dwFileVersionLS);
  Log("[verify] msedge.dll %d.%d.%d.%d (expected %d.%d.%d.%d)", major, minor,
      build, revision, edge::kVersionMajor, edge::kVersionMinor,
      edge::kVersionBuild, edge::kVersionRevision);
  return major == edge::kVersionMajor && minor == edge::kVersionMinor &&
         build == edge::kVersionBuild && revision == edge::kVersionRevision;
}

// A hooked function should begin with a plausible x64 prologue byte and sit inside
// the module image — a cheap guard against a stale RVA landing on data/padding.
inline bool LooksLikeCode(uintptr_t rva) {
  const uint8_t firstByte = *reinterpret_cast<uint8_t*>(edge::At(rva));
  switch (firstByte) {
    // REX prefixes
    case 0x40:
    case 0x41:
    case 0x48:
    case 0x4c:
    // push rbx / rbp / rsi / rdi
    case 0x53:
    case 0x55:
    case 0x56:
    case 0x57:
    // mov / xor / jmp / call-group
    case 0x8b:
    case 0x33:
    case 0xe9:
    case 0xff:
      return true;
    default:
      return false;
  }
}

// The critical hook sites (a subset of what hooks.h installs). If any fails, we
// do not install.
inline bool HookSitesLookSane() {
  const uintptr_t sites[] = {
      edge155::rva::MultiContentsView_SetWebContentsAtIndex,
      edge155::rva::MultiContentsView_CalculateProposedLayout,
      edge155::rva::SplitWindowController_DoAction,
      edge155::rva::SplitTabMenuModel_ctor,
      edge155::rva::SplitTabMenuModel_ExecuteCommand,
  };
  for (const uintptr_t rva : sites) {
    if (!LooksLikeCode(rva)) {
      Log("[verify] hook site rva=0x%llx does not look like code",
          static_cast<unsigned long long>(rva));
      return false;
    }
  }
  return true;
}

// The one gate Init calls before touching anything.
inline bool Ok() {
  return BuildMatches() && HookSitesLookSane();
}

}  // namespace fixedge::verify
