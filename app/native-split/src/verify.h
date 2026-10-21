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
[[nodiscard]] inline bool BuildMatches() {
  wchar_t path[MAX_PATH];
  HMODULE module = GetModuleHandleW(edge155::kModule);
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
      build, revision, edge155::kVersionMajor, edge155::kVersionMinor,
      edge155::kVersionBuild, edge155::kVersionRevision);
  return major == edge155::kVersionMajor && minor == edge155::kVersionMinor &&
         build == edge155::kVersionBuild &&
         revision == edge155::kVersionRevision;
}

// A hooked function should begin with a plausible x64 prologue byte and sit inside
// the module image — a cheap guard against a stale RVA landing on data/padding.
[[nodiscard]] inline bool LooksLikeCode(uintptr_t rva) {
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

// True when the bytes at `rva` are exactly `expected`.
[[nodiscard]] inline bool BytesAre(uintptr_t rva, const uint8_t* expected,
                                   size_t length) {
  const auto* actual = reinterpret_cast<const uint8_t*>(edge::At(rva));
  for (size_t i = 0; i < length; i++) {
    if (actual[i] != expected[i]) {
      return false;
    }
  }
  return true;
}

// The three bytes hooks.h overwrites to lift Edge's 2-pane CHECKs. A stale
// address would corrupt unrelated code, so each must hold the instruction it was
// reversed from: the `2` in `cmp r8d, 2`, and the two-byte opcode of a rel32
// `jne` (0F 85) / `jae` (0F 83). The patches stay in the process after this DLL
// unloads (hooks.h), so a DLL loaded again (hot-reload) finds them already made:
// the lifted cap, or the NOPs, count as holding the instruction too.
[[nodiscard]] inline bool PatchSitesHoldTheirInstructions() {
  constexpr uint8_t kIndexCapImmediate[] = {0x02};
  constexpr uint8_t kLiftedCapImmediate[] = {edge::kLiftedIndexCap};
  constexpr uint8_t kJneRel32[] = {0x0f, 0x85};
  constexpr uint8_t kJaeRel32[] = {0x0f, 0x83};
  constexpr uint8_t kNopedJump[] = {edge::kNopOpcode, edge::kNopOpcode};
  const bool isGuardIntact =
      BytesAre(edge155::rva::SetWebContentsAtIndex_guard_imm,
               kIndexCapImmediate, sizeof(kIndexCapImmediate)) ||
      BytesAre(edge155::rva::SetWebContentsAtIndex_guard_imm,
               kLiftedCapImmediate, sizeof(kLiftedCapImmediate));
  const bool isLabelCheckIntact =
      BytesAre(edge155::rva::SplitA11yLabel_paneidx_jne, kJneRel32,
               sizeof(kJneRel32)) ||
      BytesAre(edge155::rva::SplitA11yLabel_paneidx_jne, kNopedJump,
               sizeof(kNopedJump));
  const bool isActiveIndexCheckIntact =
      BytesAre(edge155::rva::SetActiveIndex_index_jae, kJaeRel32,
               sizeof(kJaeRel32)) ||
      BytesAre(edge155::rva::SetActiveIndex_index_jae, kNopedJump,
               sizeof(kNopedJump));
  return isGuardIntact && isLabelCheckIntact && isActiveIndexCheckIntact;
}

// The critical hook sites (a subset of what hooks.h installs). If any fails, we
// do not install.
[[nodiscard]] inline bool HookSitesLookSane() {
  const uintptr_t sites[] = {
      edge155::rva::MultiContentsView_SetWebContentsAtIndex,
      edge155::rva::MultiContentsView_CalculateProposedLayout,
      edge155::rva::SplitWindowController_DoAction,
      edge155::rva::SplitTabMenuModel_ctor,
      edge155::rva::SplitTabMenuModel_ExecuteCommand,
      edge155::rva::ContentSettingRPHBubbleModel_CommitChanges,
      edge155::rva::EdgeComputePreferredColorScheme,
      edge155::rva::WebContentsImpl_GetColorProvider,
      edge155::rva::ThemeService_GetColorProviderKey,
      edge155::rva::RegisterProtocolHandlerPermissionRequest_PermissionDecided,
      // The vertical-tab colour hooks, read on every paint.
      edge155::rva::BrowserThemePack_GetColor,
      edge155::rva::ColorProvider_GetColor,
      edge155::rva::EdgeTab_UpdateForegroundColors,
  };
  for (const uintptr_t rva : sites) {
    if (!LooksLikeCode(rva)) {
      Log("[verify] hook site rva=0x%llx does not look like code",
          static_cast<unsigned long long>(rva));
      return false;
    }
  }
  if (!PatchSitesHoldTheirInstructions()) {
    Log("[verify] a patch site does not hold the instruction it was reversed "
        "from");
    return false;
  }
  // The link-handler bubble's Done slot is written while that feature is on;
  // it must hold Edge's own empty virtual, or it is not the vtable we read.
  if (!edge::RphDoneSlotIsStock()) {
    Log("[verify] handler bubble Done slot is not Edge's empty virtual");
    return false;
  }
  return true;
}

// The one gate Init calls before touching anything.
[[nodiscard]] inline bool Ok() {
  return BuildMatches() && HookSitesLookSane();
}

}  // namespace fixedge::verify
