// build_relation.h — how far the running msedge.dll is from the build offsets.h
// is pinned to; it decides how much a signature hit has to prove before it is
// believed. Pure.
#pragma once

namespace edgeplus::edge_build {

struct Version {
  int major;
  int minor;
  int build;
  int revision;
};

enum class Relation {
  kPinned,         // the exact build: offsets.h is trusted as it is
  kSamePatchLine,  // the same major.minor.build, another revision: a security
                   // patch, which in practice moves code but not layouts
  kOtherBuild,     // a newer or older line: code and layouts may both differ
};

[[nodiscard]] constexpr Relation Classify(const Version& running,
                                          const Version& pinned) noexcept {
  const bool isSameLine = running.major == pinned.major &&
                          running.minor == pinned.minor &&
                          running.build == pinned.build;
  if (!isSameLine) {
    return Relation::kOtherBuild;
  }
  const bool isSameRevision = running.revision == pinned.revision;
  return isSameRevision ? Relation::kPinned : Relation::kSamePatchLine;
}

}  // namespace edgeplus::edge_build
