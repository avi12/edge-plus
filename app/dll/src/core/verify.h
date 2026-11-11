// verify.h — fail-safe self-checks run before any hook is installed: offsets are
// build-specific, and applied to another Edge build they would corrupt memory.
#pragma once
#include <windows.h>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include "edge/availability.h"
#include "edge/build_relation.h"
#include "edge/edge.h"

#pragma comment(lib, "version.lib")

namespace edgeplus::verify {

// VerQueryValueW's sub-block for the VS_FIXEDFILEINFO root.
inline constexpr wchar_t kFixedFileInfoBlock[] = L"\\";

// The build offsets.h is pinned to.
inline constexpr edge_build::Version kPinnedBuild{
    edge155::kVersionMajor, edge155::kVersionMinor, edge155::kVersionBuild,
    edge155::kVersionRevision};

// The file version of the msedge.dll this process loaded; nullopt when it cannot
// be read.
[[nodiscard]] inline std::optional<edge_build::Version> RunningBuild() {
  wchar_t path[MAX_PATH];
  HMODULE module = GetModuleHandleW(edge155::kModule);
  const bool hasModulePath =
      module != nullptr && GetModuleFileNameW(module, path, MAX_PATH) != 0;
  if (!hasModulePath) {
    return std::nullopt;
  }

  DWORD ignored = 0;
  const DWORD versionInfoSize = GetFileVersionInfoSizeW(path, &ignored);
  if (!versionInfoSize) {
    return std::nullopt;
  }
  std::vector<uint8_t> versionInfo(versionInfoSize);
  if (!GetFileVersionInfoW(path, 0, versionInfoSize, versionInfo.data())) {
    return std::nullopt;
  }

  VS_FIXEDFILEINFO* info = nullptr;
  UINT infoLength = 0;
  const bool hasFixedInfo =
      VerQueryValueW(versionInfo.data(), kFixedFileInfoBlock,
                     reinterpret_cast<void**>(&info), &infoLength) &&
      info != nullptr;
  if (!hasFixedInfo) {
    return std::nullopt;
  }
  const edge_build::Version running{
      HIWORD(info->dwFileVersionMS), LOWORD(info->dwFileVersionMS),
      HIWORD(info->dwFileVersionLS), LOWORD(info->dwFileVersionLS)};
  Log("[verify] msedge.dll %d.%d.%d.%d (pinned %d.%d.%d.%d)", running.major,
      running.minor, running.build, running.revision, kPinnedBuild.major,
      kPinnedBuild.minor, kPinnedBuild.build, kPinnedBuild.revision);
  return running;
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
  const void* const address = edge::At(rva);
  if (!address) {
    return false;
  }
  const uint8_t firstByte = *static_cast<const uint8_t*>(address);
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
  if (!actual) {
    return false;
  }
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
      edge155::rva::RawPtrBackupRef_AcquireInternal,
      edge155::rva::MultiContentsView_dtor,
      edge155::rva::EdgeTabDragContextImpl_CalculateBoundsForDraggedViews,
      edge155::rva::EdgeTabStyleViews_IsRightSplitTab,
      // The Cyberpunk skin's tab frames (tab_frame.h).
      edge155::rva::EdgeTabStyleViews_PaintBackgroundStroke,
      edge155::rva::TabStrip_GetStrokeThickness,
      // Its title font (tab_font.h).
      edge155::rva::EdgeTabStyleViews_PaintTab,
      // Its omnibox frame (omnibox_frame.h).
      edge155::rva::LocationBarView_EdgeCreateRoundRectBackground,
  };
  for (const uintptr_t rva : sites) {
    // A site with no place in this build belongs to a feature that stood down
    // (resolve.h); the pinned build places them all.
    const bool isPlaced = edge::At(rva) != nullptr;
    if (!isPlaced) {
      continue;
    }
    if (!LooksLikeCode(rva)) {
      Log("[verify] hook site rva=0x%llx does not look like code",
          static_cast<unsigned long long>(rva));
      return false;
    }
  }
  const bool isSplitAvailable =
      availability::IsAvailable(edge155::signatures::Feature::kSplit);
  if (isSplitAvailable && !PatchSitesHoldTheirInstructions()) {
    Log("[verify] a patch site does not hold the instruction it was reversed "
        "from");
    return false;
  }
  // The link-handler bubble's Done slot is written while that feature is on;
  // it must hold Edge's own empty virtual, or it is not the vtable we read.
  const bool areHandlersAvailable = availability::IsAvailable(
      edge155::signatures::Feature::kProtocolHandlers);
  if (areHandlersAvailable && !edge::RphDoneSlotIsStock()) {
    Log("[verify] handler bubble Done slot is not Edge's empty virtual");
    return false;
  }
  return true;
}

}  // namespace edgeplus::verify
