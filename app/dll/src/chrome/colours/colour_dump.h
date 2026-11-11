// colour_dump.h — what Edge's ColorProvider answers, and which colours a view
// asks for, by name: tools for finding the id a piece of UI draws with (a
// script's tools, the way tabrows.h's Dump is). UI thread for the commands; the
// counting runs wherever the provider is read.
#pragma once
#include <algorithm>
#include <atomic>
#include <string_view>
#include <vector>
#include "chrome/colours/colour_ids.h"
#include "core/log.h"
#include "edge/edge.h"

namespace edgeplus::colour_dump {

inline constexpr int kFirstId = 0;
inline constexpr int kIdCount = colour_ids::kMaxId;

// One line per id: "[colours] 0x5ca 0xff202124".
inline void Dump(void* view) {
  for (int colorId = kFirstId; colorId < kFirstId + kIdCount; colorId++) {
    const std::optional<uint32_t> color = edge::ViewColor(view, colorId);
    if (!color) {
      Log("[colours] no provider for the view");
      return;
    }
    Log("[colours] 0x%x 0x%08x", colorId, *color);
  }
}

inline constinit bool g_isTracing = false;
inline constinit std::atomic<uint32_t> g_hits[kIdCount] = {};

// Called by the provider hook for every read while tracing.
inline void Count(int colorId) noexcept {
  const bool isInRange = colorId >= 0 && colorId < kIdCount;
  if (isInRange) {
    g_hits[colorId].fetch_add(1, std::memory_order_relaxed);
  }
}

// lParam 1 starts counting from zero; 0 stops and logs each id read, by name
// and with the colour the provider gives now, most-read last.
inline void Trace(bool isStarting) {
  if (isStarting) {
    for (auto& hits : g_hits) {
      hits.store(0, std::memory_order_relaxed);
    }
    g_isTracing = true;
    return;
  }
  g_isTracing = false;
  for (int colorId = kFirstId; colorId < kFirstId + kIdCount; colorId++) {
    const uint32_t hits = g_hits[colorId].load(std::memory_order_relaxed);
    if (hits == 0) {
      continue;
    }
    const std::string_view name = colour_ids::NameOf(colorId);
    Log("[colours-read] 0x%x %.*s x%u", colorId, static_cast<int>(name.size()),
        name.data(), hits);
  }
}

}  // namespace edgeplus::colour_dump
