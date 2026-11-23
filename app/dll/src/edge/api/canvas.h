// canvas.h — Skia and gfx::Canvas, for painting a tab: a filled rect, a
// rounded rect with its own corner radii, and an empty path.
#pragma once
#include "chrome/tabs/tab_edge.h"
#include "edge/api/binding.h"
#include "edge/geom.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

inline void CanvasDrawRect(void* canvas, const Rect& rect, const void* flags) {
  using DrawRectFn = void(__fastcall*)(void*, const Rect*, const void*);
  Callable<DrawRectFn>(edge155::rva::Canvas_DrawRect)(canvas, &rect, flags);
}

// Sets `roundedRect` (an SkRRect) to `skRect` with each corner's own radius.
inline void SetRoundedRectRadii(void* roundedRect, const void* skRect,
                                const tab_edge::CornerRadii& radii) {
  using SetRadiiFn =
      void(__fastcall*)(void*, const void*, const tab_edge::CornerRadius*);
  Callable<SetRadiiFn>(edge155::rva::SkRRect_setRectRadii)(roundedRect, skRect,
                                                           radii.corners);
}

// SkPathFillType.
enum class PathFillType : int {
  kWinding = 0,
};

// Constructs an empty SkPath in `outPath` (uninitialised storage) and returns
// it, as a function returning an SkPath through a hidden pointer does.
inline void* ConstructEmptyPath(void* outPath) {
  using ConstructFn = void(__fastcall*)(void*, PathFillType);
  Callable<ConstructFn>(edge155::rva::SkPath_ctor)(outPath,
                                                   PathFillType::kWinding);
  return outPath;
}

}  // namespace edgeplus::edge
