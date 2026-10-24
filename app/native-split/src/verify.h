// verify.h — fail-safe self-checks run before any hook is installed: offsets are
// build-specific, and applied to another Edge build they would corrupt memory.
#pragma once
#include <windows.h>
#include <cstdint>
#include <span>
#include <vector>
#include "edge.h"

#pragma comment(lib, "version.lib")

namespace fixedge::verify {

// VerQueryValueW's sub-block for the VS_FIXEDFILEINFO root.
inline constexpr wchar_t kFixedFileInfoBlock[] = L"\\";

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

// The x64 bytes the checks below look for.
enum class Opcode : uint8_t {
  kRexPrefix = 0x40,
  kRexPrefixB = 0x41,
  kRexPrefixW = 0x48,
  kRexPrefixWR = 0x4c,
  kPushRbx = 0x53,
  kPushRbp = 0x55,
  kPushRsi = 0x56,
  kPushRdi = 0x57,
  kMovLoad = 0x8b,
  kXor = 0x33,
  kJmpRel32 = 0xe9,
  kCallGroup = 0xff,
  kTwoByteEscape = 0x0f,
  kJaeRel32Second = 0x83,
  kJneRel32Second = 0x85,
};

[[nodiscard]] constexpr uint8_t ByteOf(Opcode opcode) noexcept {
  return static_cast<uint8_t>(opcode);
}

// The `2` in `cmp r8d, 2`.
inline constexpr uint8_t kStockIndexCap = 2;

// A plausible x64 prologue byte: a cheap guard against a stale RVA landing on
// data or padding.
[[nodiscard]] inline bool LooksLikeCode(uintptr_t rva) {
  const uint8_t firstByte = *reinterpret_cast<uint8_t*>(edge::At(rva));
  switch (static_cast<Opcode>(firstByte)) {
    case Opcode::kRexPrefix:
    case Opcode::kRexPrefixB:
    case Opcode::kRexPrefixW:
    case Opcode::kRexPrefixWR:
    case Opcode::kPushRbx:
    case Opcode::kPushRbp:
    case Opcode::kPushRsi:
    case Opcode::kPushRdi:
    case Opcode::kMovLoad:
    case Opcode::kXor:
    case Opcode::kJmpRel32:
    case Opcode::kCallGroup:
      return true;
    default:
      return false;
  }
}

[[nodiscard]] inline bool BytesAre(uintptr_t rva,
                                   std::span<const uint8_t> expected) {
  const auto* actual = reinterpret_cast<const uint8_t*>(edge::At(rva));
  for (size_t i = 0; i < expected.size(); i++) {
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
  constexpr uint8_t kIndexCapImmediate[] = {kStockIndexCap};
  constexpr uint8_t kLiftedCapImmediate[] = {edge::kLiftedIndexCap};
  constexpr uint8_t kJneRel32[] = {ByteOf(Opcode::kTwoByteEscape),
                                   ByteOf(Opcode::kJneRel32Second)};
  constexpr uint8_t kJaeRel32[] = {ByteOf(Opcode::kTwoByteEscape),
                                   ByteOf(Opcode::kJaeRel32Second)};
  constexpr uint8_t kNopedJump[] = {edge::kNopOpcode, edge::kNopOpcode};
  const bool isGuardIntact =
      BytesAre(edge155::rva::SetWebContentsAtIndex_guard_imm,
               kIndexCapImmediate) ||
      BytesAre(edge155::rva::SetWebContentsAtIndex_guard_imm,
               kLiftedCapImmediate);
  const bool isLabelCheckIntact =
      BytesAre(edge155::rva::SplitA11yLabel_paneidx_jne, kJneRel32) ||
      BytesAre(edge155::rva::SplitA11yLabel_paneidx_jne, kNopedJump);
  const bool isActiveIndexCheckIntact =
      BytesAre(edge155::rva::SetActiveIndex_index_jae, kJaeRel32) ||
      BytesAre(edge155::rva::SetActiveIndex_index_jae, kNopedJump);
  return isGuardIntact && isLabelCheckIntact && isActiveIndexCheckIntact;
}

// A subset of what hooks.h installs.
[[nodiscard]] inline bool HookSitesLookSane() {
  const uintptr_t sites[] = {
      edge155::rva::MultiContentsView_SetWebContentsAtIndex,
      edge155::rva::MultiContentsView_CalculateProposedLayout,
      edge155::rva::MultiContentsView_CloseSplitView,
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
      edge155::rva::Tab_IsActive,
      edge155::rva::EdgeTabStyleViews_IsLeftSplitTab,
      edge155::rva::EdgeTabStyleViews_IsRightSplitTab,
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

[[nodiscard]] inline bool Ok() {
  return BuildMatches() && HookSitesLookSane();
}

}  // namespace fixedge::verify
