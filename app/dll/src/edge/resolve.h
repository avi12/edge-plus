// resolve.h — finds offsets.h's entries again in an msedge.dll build the DLL was not
// reversed against. The only place that reads the running image's code and its
// exception table; the patterns are signature.h's, the verdict resolution.h's, the
// table signatures.g.h (tools/re/make_signatures.py).
//
// It runs once, at load, when the build is not the pinned one: scan the code
// section for every entry's signatures in one pass, decide each entry, then
//   - fill translation.h so edge::At lands on the found addresses, and
//   - clear the bit of every feature that has an entry it could not place
//     (availability.h), so that feature stays off and the others keep working.
#pragma once
#include <windows.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>
#include "core/handles.h"
#include "core/log.h"
#include "edge/availability.h"
#include "edge/body.h"
#include "edge/build_relation.h"
#include "edge/resolution.h"
#include "edge/signature.h"
#include "edge/signatures.g.h"
#include "edge/translation.h"

namespace edgeplus::resolve {

namespace signatures = ::edgeplus::edge155::signatures;

// The running msedge.dll's code and the image's own list of function starts.
struct Image {
  uintptr_t base = 0;
  uint32_t size = 0;
  uint32_t timestamp = 0;
  uint32_t textRva = 0;
  uint32_t textSize = 0;
  uint32_t textRawOffset = 0;  // where .text sits in the file on disk
  const RUNTIME_FUNCTION* functions = nullptr;
  size_t functionCount = 0;
};

inline constexpr char kTextSectionName[] = ".text";

[[nodiscard]] inline std::optional<Image> ReadImage(uintptr_t base) {
  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  const bool hasDosSignature = dos->e_magic == IMAGE_DOS_SIGNATURE;
  if (!hasDosSignature) {
    return std::nullopt;
  }
  const auto* ntHeaders =
      reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
  const bool hasNtSignature = ntHeaders->Signature == IMAGE_NT_SIGNATURE;
  if (!hasNtSignature) {
    return std::nullopt;
  }
  Image image;
  image.base = base;
  image.size = ntHeaders->OptionalHeader.SizeOfImage;
  image.timestamp = ntHeaders->FileHeader.TimeDateStamp;
  const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(ntHeaders);
  for (WORD i = 0; i < ntHeaders->FileHeader.NumberOfSections; i++) {
    const bool isText =
        std::strncmp(reinterpret_cast<const char*>(section[i].Name),
                     kTextSectionName, IMAGE_SIZEOF_SHORT_NAME) == 0;
    if (isText) {
      image.textRva = section[i].VirtualAddress;
      image.textSize =
          std::min(section[i].Misc.VirtualSize, section[i].SizeOfRawData);
      image.textRawOffset = section[i].PointerToRawData;
    }
  }
  const IMAGE_DATA_DIRECTORY& exceptions =
      ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
  image.functions = reinterpret_cast<const RUNTIME_FUNCTION*>(
      base + exceptions.VirtualAddress);
  image.functionCount = exceptions.Size / sizeof(RUNTIME_FUNCTION);
  const bool isUsable = image.textSize != 0 && image.functionCount != 0;
  return isUsable ? std::optional<Image>(image) : std::nullopt;
}

// The code to search. Preferably msedge.dll as the file on disk has it: the
// loaded copy carries whatever has been written over it since (another hook's
// jump in a function's first bytes, the patches this DLL itself leaves behind
// across a hot reload), none of which a signature of the build's own bytes
// matches. The file is used only if it is the very build that is loaded.
class CodeView {
 public:
  CodeView(const Image& image, HMODULE module) {
    wchar_t path[MAX_PATH];
    const bool hasPath = GetModuleFileNameW(module, path, MAX_PATH) != 0;
    if (hasPath) {
      MapFile(path, image);
    }
    if (!view_) {
      text_ = std::span<const uint8_t>(
          reinterpret_cast<const uint8_t*>(image.base + image.textRva),
          image.textSize);
    }
  }
  ~CodeView() {
    if (view_) {
      UnmapViewOfFile(view_);
    }
  }
  CodeView(const CodeView&) = delete;
  CodeView& operator=(const CodeView&) = delete;
  CodeView(CodeView&&) = delete;
  CodeView& operator=(CodeView&&) = delete;

  [[nodiscard]] std::span<const uint8_t> Text() const {
    return text_;
  }
  [[nodiscard]] bool IsFromDisk() const {
    return view_ != nullptr;
  }

 private:
  void MapFile(const wchar_t* path, const Image& image) {
    const handles::UniqueHandle file{
        CreateFileW(path, GENERIC_READ,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
    if (!file) {
      return;
    }
    const handles::UniqueHandle mapping{
        CreateFileMappingW(file.Get(), nullptr, PAGE_READONLY, 0, 0, nullptr)};
    if (!mapping) {
      return;
    }
    const void* const view =
        MapViewOfFile(mapping.Get(), FILE_MAP_READ, 0, 0, 0);
    if (!view) {
      return;
    }
    const auto* dos = static_cast<const IMAGE_DOS_HEADER*>(view);
    const auto* ntHeaders = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
        static_cast<const uint8_t*>(view) + dos->e_lfanew);
    const bool isSameBuild =
        ntHeaders->FileHeader.TimeDateStamp == image.timestamp &&
        ntHeaders->OptionalHeader.SizeOfImage == image.size;
    if (!isSameBuild) {
      UnmapViewOfFile(view);
      return;
    }
    view_ = view;
    text_ = std::span<const uint8_t>(
        static_cast<const uint8_t*>(view) + image.textRawOffset,
        image.textSize);
  }

  const void* view_ = nullptr;
  std::span<const uint8_t> text_;
};

// How long the function starting at rva is, by the exception table; 0 when no
// function starts there.
[[nodiscard]] inline uint32_t FunctionSizeAt(const Image& image,
                                             uintptr_t rva) {
  const RUNTIME_FUNCTION* const end = image.functions + image.functionCount;
  const RUNTIME_FUNCTION* const found =
      std::lower_bound(image.functions, end, rva,
                       [](const RUNTIME_FUNCTION& function, uintptr_t wanted) {
                         return function.BeginAddress < wanted;
                       });
  const bool isStart = found != end && found->BeginAddress == rva;
  return isStart ? found->EndAddress - found->BeginAddress : 0;
}

[[nodiscard]] inline bool IsInText(const Image& image, uintptr_t rva) {
  return rva >= image.textRva && rva < image.textRva + image.textSize;
}

// The RIP-relative operand at `site + operandOffset`: the 4 displacement bytes
// after a ModRM byte of that form. The address it names, or nullopt when the
// bytes there are not such an instruction or the address is not in the image's
// data.
[[nodiscard]] inline std::optional<uintptr_t> DataAddressAt(
    const Image& image, uintptr_t displacementRva, uint8_t extraBytes) {
  constexpr uint8_t kModRmRipMask = 0xC7;
  constexpr uint8_t kModRmRipForm = 0x05;
  constexpr uintptr_t kDisplacementSize = 4;
  const auto* modRm =
      reinterpret_cast<const uint8_t*>(image.base + displacementRva - 1);
  const bool isRipForm = (*modRm & kModRmRipMask) == kModRmRipForm;
  if (!isRipForm) {
    return std::nullopt;
  }
  int32_t displacement = 0;
  std::memcpy(&displacement,
              reinterpret_cast<const void*>(image.base + displacementRva),
              sizeof(displacement));
  const uintptr_t target =
      displacementRva + kDisplacementSize + extraBytes +
      static_cast<uintptr_t>(static_cast<intptr_t>(displacement));
  const bool isData = target < image.size && !IsInText(image, target);
  return isData ? std::optional<uintptr_t>(target) : std::nullopt;
}

// Where the `call rel32` / `jmp rel32` whose 4 displacement bytes are at
// displacementRva lands: nullopt unless the opcode before them is one of those and
// the target starts a function of about the size the entry was signed with.
[[nodiscard]] inline std::optional<uintptr_t> CallTargetAt(
    const Image& image, uintptr_t displacementRva,
    const signatures::Entry& entry) {
  constexpr uint8_t kCallOpcode = 0xE8;
  constexpr uint8_t kJumpOpcode = 0xE9;
  constexpr uintptr_t kDisplacementSize = 4;
  const uint8_t opcode =
      *reinterpret_cast<const uint8_t*>(image.base + displacementRva - 1);
  const bool isRelativeBranch = opcode == kCallOpcode || opcode == kJumpOpcode;
  if (!isRelativeBranch) {
    return std::nullopt;
  }
  int32_t displacement = 0;
  std::memcpy(&displacement,
              reinterpret_cast<const void*>(image.base + displacementRva),
              sizeof(displacement));
  const uintptr_t target =
      displacementRva + kDisplacementSize +
      static_cast<uintptr_t>(static_cast<intptr_t>(displacement));
  const bool isInText = IsInText(image, target);
  if (!isInText) {
    return std::nullopt;
  }
  // A leaf thunk has no unwind entry (targetSize 0); anything else must start a
  // function about the size the entry was signed with.
  const bool hasKnownSize = entry.targetSize != 0;
  if (hasKnownSize) {
    const uint32_t size = FunctionSizeAt(image, target);
    const bool isExpectedFunction =
        size != 0 && resolution::IsSizePlausible(entry.targetSize, size);
    if (!isExpectedFunction) {
      return std::nullopt;
    }
  }
  const std::optional<signature::Pattern> opening =
      signature::Parse(entry.targetPattern);
  const std::span<const uint8_t> code(
      reinterpret_cast<const uint8_t*>(image.base + target),
      opening ? opening->bytes.size() : 0);
  const bool isOpeningAsSigned =
      !opening || signature::MatchesAt(code, 0, *opening);
  return isOpeningAsSigned ? std::optional<uintptr_t>(target) : std::nullopt;
}

struct Outcome {
  uint16_t availableFeatures = 0;
  size_t resolvedEntries = 0;
  size_t totalEntries = 0;
  size_t misplacedEntries = 0;  // found elsewhere than pinned
};

enum class RunMode {
  kApply,     // fill translation.h and availability.h
  kSelfTest,  // on the pinned build: change nothing, count the entries found
              // anywhere but where offsets.h says they are
};

// Whether the body of a placed entry, read from the code being searched, is
// the code it was signed from.
[[nodiscard]] inline bool IsBodyUnchanged(const Image& image,
                                          std::span<const uint8_t> text,
                                          const signatures::Body& body,
                                          uintptr_t anchor) {
  const bool hasBody = signatures::HasBody(body);
  const bool isAnchorPastOffset = anchor >= body.offset;
  if (!hasBody || !isAnchorPastOffset) {
    return false;
  }
  const uintptr_t start = anchor - body.offset;
  const bool isInCode = start >= image.textRva &&
                        start - image.textRva + body.size <= text.size();
  if (!isInCode) {
    return false;
  }
  const bool isSizeAsListed =
      !body.isListed || FunctionSizeAt(image, start) == body.size;
  if (!isSizeAsListed) {
    return false;
  }
  return body::IsUnchanged(body,
                           text.subspan(start - image.textRva, body.size));
}

// Where the entry is in this build; `anchor` is where its body's anchor is
// (signature_table.h's Body: the entry itself, or for data the function that
// refers to it).
[[nodiscard]] inline std::optional<uintptr_t> PlaceEntry(
    const Image& image, const signatures::Entry& entry,
    const std::vector<std::vector<uintptr_t>>& hitsPerProbe,
    edge_build::Relation relation, resolution::Status* status,
    uintptr_t* anchor) {
  const bool isSite = entry.kind == signatures::EntryKind::kSite;
  const auto isPlausible = [&](uintptr_t rva) {
    if (isSite) {
      return IsInText(image, rva);
    }
    const uint32_t size = FunctionSizeAt(image, rva);
    return size != 0 && resolution::IsSizePlausible(entry.functionSize, size);
  };
  const resolution::Verdict verdict =
      resolution::Decide(entry, hitsPerProbe, relation, isPlausible);
  *status = verdict.status;
  if (verdict.status != resolution::Status::kResolved) {
    return std::nullopt;
  }
  const bool isInside = entry.kind == signatures::EntryKind::kInside;
  if (isInside) {
    *anchor = verdict.rva + entry.operandOffset;
    return *anchor;
  }
  const bool isCallee = entry.kind == signatures::EntryKind::kCallee;
  if (isCallee) {
    const std::optional<uintptr_t> callee =
        CallTargetAt(image, verdict.rva + entry.operandOffset, entry);
    if (!callee) {
      *status = resolution::Status::kRejected;
      return std::nullopt;
    }
    *anchor = *callee;
    return callee;
  }
  const bool isData = entry.kind == signatures::EntryKind::kData;
  *anchor = verdict.rva;
  if (!isData) {
    return verdict.rva;
  }
  const std::optional<uintptr_t> referenced = DataAddressAt(
      image, verdict.rva + entry.operandOffset, entry.operandExtra);
  if (!referenced) {
    *status = resolution::Status::kRejected;
    return std::nullopt;
  }
  return *referenced + static_cast<intptr_t>(entry.dataDelta);
}

// Resolve every entry of signatures::kEntries against the image at `base`, fill
// translation.h and availability.h. translation stays inactive (every address
// unplaced) when the image cannot be read at all.
[[nodiscard]] inline Outcome Run(uintptr_t base, edge_build::Relation relation,
                                 RunMode mode = RunMode::kApply) {
  Outcome outcome;
  outcome.totalEntries = std::size(signatures::kEntries);
  const bool isApplying = mode == RunMode::kApply;
  const std::optional<Image> image = ReadImage(base);
  if (!image) {
    Log("[resolve] cannot read msedge.dll's sections");
    edge::translation::g_isActive = isApplying;
    return outcome;
  }
  const ULONGLONG started = GetTickCount64();

  signature::MultiScanner scanner;
  constexpr size_t kNoProbe = ~size_t{0};
  std::vector<std::array<size_t, signatures::kProbesPerEntry>> probeIds;
  for (const signatures::Entry& entry : signatures::kEntries) {
    std::array<size_t, signatures::kProbesPerEntry> ids{};
    ids.fill(kNoProbe);
    for (size_t i = 0; i < entry.probes.size(); i++) {
      if (signatures::IsEmpty(entry.probes[i])) {
        continue;
      }
      std::optional<signature::Pattern> pattern =
          signature::Parse(entry.probes[i].pattern);
      if (pattern) {
        ids[i] = scanner.Add(std::move(*pattern));
      }
    }
    probeIds.push_back(ids);
  }
  const CodeView code(*image, reinterpret_cast<HMODULE>(base));
  Log("[resolve] searching msedge.dll's code %s",
      code.IsFromDisk() ? "as the file on disk has it" : "in memory");
  scanner.Scan(code.Text());

  uint16_t unresolvedFeatures = 0;
  std::vector<std::pair<uintptr_t, uintptr_t>> placed;
  std::vector<uint16_t> placedFeatures;
  for (size_t index = 0; index < std::size(signatures::kEntries); index++) {
    const signatures::Entry& entry = signatures::kEntries[index];
    std::vector<std::vector<uintptr_t>> hitsPerProbe(
        signatures::kProbesPerEntry);
    for (size_t i = 0; i < signatures::kProbesPerEntry; i++) {
      const bool hasProbe = probeIds[index][i] != kNoProbe;
      if (!hasProbe) {
        continue;
      }
      for (const size_t position : scanner.HitsOf(probeIds[index][i])) {
        hitsPerProbe[i].push_back(image->textRva + position);
      }
    }
    resolution::Status status = resolution::Status::kUnsigned;
    uintptr_t anchor = 0;
    std::optional<uintptr_t> rva =
        PlaceEntry(*image, entry, hitsPerProbe, relation, &status, &anchor);
    const bool isUnverified =
        rva && resolution::NeedsUnchangedBody(relation) &&
        !IsBodyUnchanged(*image, code.Text(), entry.body, anchor);
    // Where a changed entry now sits is where a port starts reading.
    uintptr_t changedRva = 0;
    if (isUnverified) {
      status = resolution::Status::kChanged;
      changedRva = *rva;
      rva.reset();
    }
    if (rva) {
      placed.emplace_back(entry.pinnedRva, *rva);
      placedFeatures.push_back(entry.features);
      // Logged on every run: on another build it is the new address a port
      // writes into offsets.h.
      const bool isMisplaced = *rva != entry.pinnedRva;
      if (isMisplaced) {
        outcome.misplacedEntries++;
        Log("[resolve] %s: found at 0x%llx, offsets.h says 0x%llx", entry.name,
            static_cast<unsigned long long>(*rva),
            static_cast<unsigned long long>(entry.pinnedRva));
      }
      continue;
    }
    unresolvedFeatures =
        static_cast<uint16_t>(unresolvedFeatures | entry.features);
    // Every one is logged: the list is what a port has to look at, feature by
    // feature.
    Log("[resolve] %s (rva 0x%llx, features 0x%02x): %s at 0x%llx", entry.name,
        static_cast<unsigned long long>(entry.pinnedRva), entry.features,
        resolution::DescribeStatus(status),
        static_cast<unsigned long long>(changedRva));
  }

  outcome.availableFeatures =
      static_cast<uint16_t>(signatures::kEveryFeature & ~unresolvedFeatures);
  outcome.resolvedEntries = placed.size();
  if (isApplying) {
    // An entry stays usable while at least one feature that calls it is
    // available; one only a stood-down feature calls is withdrawn, so a
    // half-installed feature cannot hook it.
    std::vector<edge::translation::Placement> usable;
    for (size_t i = 0; i < placed.size(); i++) {
      const bool isUsed = (placedFeatures[i] & outcome.availableFeatures) != 0;
      if (isUsed) {
        usable.push_back(placed[i]);
      }
    }
    edge::translation::Fill(std::move(usable));
    edge::translation::g_isActive = true;
    availability::g_features = outcome.availableFeatures;
  }
  Log("[resolve] %zu of %zu entries placed in %llu ms; features available: "
      "0x%02x; %zu found away from offsets.h",
      outcome.resolvedEntries, outcome.totalEntries,
      static_cast<unsigned long long>(GetTickCount64() - started),
      outcome.availableFeatures, outcome.misplacedEntries);
  return outcome;
}

}  // namespace edgeplus::resolve
