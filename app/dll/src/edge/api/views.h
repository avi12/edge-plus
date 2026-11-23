// views.h — views::View, what every part of Edge's window is built from:
// creating and adding one, its visibility, bounds, layout and paint, its place
// in the tree and on screen, its tooltip and colours, and the UI direction.
#pragma once
#include <cstdint>
#include <optional>
#include "edge/api/binding.h"
#include "edge/api/strings.h"
#include "edge/geom.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

inline void* NewView(size_t size) {
  using Fn = void*(__fastcall*)(size_t);
  return Callable<Fn>(edge155::rva::CheckedOperatorNew3)(size);
}

inline void AddChildViewAt(void* parent, void* child, size_t index) {
  using Fn = void*(__fastcall*)(void*, void*, size_t);
  Callable<Fn>(edge155::rva::AddChildViewAtImpl)(parent, child, index);
}

inline void SetViewVisible(void* view, bool isVisible) {
  using Fn = void(__fastcall*)(void*, bool);
  Callable<Fn>(edge155::rva::View_SetVisible)(view, isVisible);
}

inline bool ViewGetVisible(void* view) {
  using Fn = bool(__fastcall*)(void*);
  return Callable<Fn>(edge155::rva::View_GetVisible)(view);
}

inline void InvalidateLayout(void* view) {
  using Fn = void(__fastcall*)(void*);
  Callable<Fn>(edge155::rva::View_InvalidateLayout)(view);
}
inline void SchedulePaint(void* view) {
  using Fn = void(__fastcall*)(void*);
  Callable<Fn>(edge155::rva::View_SchedulePaint)(view);
}
// Force a synchronous relayout now (InvalidateLayout only marks dirty; a pure
// tree edit with no data-model change needs this to re-run the layout detour).
inline void LayoutImmediately(void* view) {
  using Fn = void(__fastcall*)(void*);
  Callable<Fn>(edge155::rva::View_LayoutImmediately)(view);
}

// absl::InlinedVector<ChildLayout, 8>::emplace_back<View*, bool, const
// gfx::Rect&>.
// `viewSlot` -> View*, `visibleSlot` -> bool, `rectSlot` -> gfx::Rect (our Rect).
inline void EmplaceChildLayout(void* vector, void* viewSlot, void* visibleSlot,
                               void* rectSlot) {
  using Fn = void*(__fastcall*)(void*, void*, void*, void*);
  Callable<Fn>(edge155::rva::ChildLayouts_emplace_back)(vector, viewSlot,
                                                        visibleSlot, rectSlot);
}

inline void ViewSetTooltip(void* view, const char* utf8) {
  U16String text{};
  Utf8ToUtf16(&text, utf8);
  using Fn = void(__fastcall*)(void*, const void*);
  Callable<Fn>(edge155::rva::View_SetTooltipText)(view, &text);
}

inline bool ViewIsMouseHovered(void* view) {
  using HoveredFn = bool(__fastcall*)(void*);
  return Callable<HoveredFn>(edge155::rva::View_IsMouseHovered)(view);
}

inline size_t ViewChildCount(void* view) {
  return Field<uint64_t>(view, edge155::off::VIEW_child_count);
}
// Child `index` (< ViewChildCount) of `view`.
inline void* ViewChildAt(void* view, size_t index) {
  return Field<void**>(view, edge155::off::VIEW_children)[index];
}

// A view's current bounds in its parent's coordinates (views::View::bounds_).
inline Rect ViewBounds(void* view) {
  return Field<Rect>(view, edge155::off::VIEW_bounds);
}

inline void* ViewParent(void* view) {
  return Field<void*>(view, edge155::off::VIEW_parent);
}
// The widget's root view: its coordinates are the window's client area in DIPs.
inline void* ViewRoot(void* view) {
  while (void* parent = ViewParent(view)) {
    view = parent;
  }
  return view;
}
inline void ConvertPoint(void* source, void* target, Point* point) {
  using Fn = void(__fastcall*)(void*, void*, Point*);
  Callable<Fn>(edge155::rva::View_ConvertPointToTarget)(source, target, point);
}
// A view's bounds in screen DIPs.
inline Rect ViewBoundsInScreen(void* view) {
  Rect bounds{};
  using BoundsFn = void*(__fastcall*)(void*, Rect*);
  Callable<BoundsFn>(edge155::rva::View_GetBoundsInScreen)(view, &bounds);
  return bounds;
}
inline void ConvertPointFromScreen(void* view, Point* point) {
  using ConvertFn = void(__fastcall*)(void*, Point*);
  Callable<ConvertFn>(edge155::rva::View_ConvertPointFromScreen)(view, point);
}
inline void ConvertPointToScreen(void* view, Point* point) {
  using ConvertFn = void(__fastcall*)(void*, Point*);
  Callable<ConvertFn>(edge155::rva::View_ConvertPointToScreen)(view, point);
}

inline void ViewReorderChild(void* parent, void* child, size_t index) {
  using Fn = void(__fastcall*)(void*, void*, size_t);
  Callable<Fn>(edge155::rva::View_ReorderChildView)(parent, child, index);
}
inline void ViewRemoveChild(void* parent, void* child) {
  using Fn = void(__fastcall*)(void*, void*);
  Callable<Fn>(edge155::rva::View_RemoveChildView)(parent, child);
}

// A plain views::View (no layout manager: its children stay where they are
// put), not yet parented.
inline void* NewPlainView() {
  void* view = NewView(edge155::off::View_size);
  using CtorFn = void*(__fastcall*)(void*);
  Callable<CtorFn>(edge155::rva::View_ctor)(view);
  return view;
}

inline void SetViewBounds(void* view, const Rect& bounds) {
  using SetBoundsFn = void(__fastcall*)(void*, const Rect*);
  Callable<SetBoundsFn>(edge155::rva::View_SetBoundsRect)(view, &bounds);
}

inline Size ViewPreferredSize(void* view) {
  Size size{};
  const uint8_t unboundedSizeBounds[edge155::off::SizeBounds_size] = {};
  using PreferredFn = void*(__fastcall*)(void*, Size*, const void*);
  Callable<PreferredFn>(edge155::rva::View_GetPreferredSize)(
      view, &size, unboundedSizeBounds);
  return size;
}

inline int ProposedHostHeight(void* proposedLayout) {
  return Field<Size>(proposedLayout, edge155::off::PL_host_size).height;
}

// The bounds a views::ProposedLayout gives `child`, or null if it has none.
inline Rect* ProposedChildBounds(void* proposedLayout, const void* child) {
  const auto sizeWord =
      Field<uint64_t>(proposedLayout, edge155::off::PL_child_size_word);
  const bool isOnHeap = (sizeWord & edge155::off::PL_child_on_heap) != 0;
  const uint64_t count = sizeWord >> edge155::off::PL_child_count_shift;
  uint8_t* inlineEntries =
      static_cast<uint8_t*>(proposedLayout) + edge155::off::PL_child_entries;
  uint8_t* entries =
      isOnHeap ? *reinterpret_cast<uint8_t**>(inlineEntries) : inlineEntries;
  for (size_t i = 0; i < count; i++) {
    uint8_t* entry = entries + i * edge155::off::CL_stride;
    const bool isChildEntry =
        Field<void*>(entry, edge155::off::CL_child) == child;
    if (isChildEntry) {
      return reinterpret_cast<Rect*>(entry + edge155::off::CL_bounds);
    }
  }
  return nullptr;
}

inline void SetViewPreferredSize(void* view, const Size& size) {
  struct OptionalSize {  // std::optional<gfx::Size>
    Size size;
    bool hasValue = false;
  };
  const OptionalSize preferred{size, true};
  using PreferredFn = void(__fastcall*)(void*, const OptionalSize*);
  Callable<PreferredFn>(edge155::rva::View_SetPreferredSize)(view, &preferred);
}

inline bool IsUiRightToLeft() {
  using Fn = bool(__fastcall*)();
  return Callable<Fn>(edge155::rva::kBaseI18nIsRtl)();
}

inline void ViewSetEnabled(void* view, bool isEnabled) {
  using Fn = void(__fastcall*)(void*, bool);
  Callable<Fn>(edge155::rva::View_SetEnabled)(view, isEnabled);
}

// A view's colour, as its widget's ColorProvider resolves `colorId`; nullopt
// for a view in no widget. Goes through ColorProvider::GetColor's entry, so
// colour_detours.h's h_getuicolor answers for the ids it recolours.
inline std::optional<uint32_t> ViewColor(void* view, int colorId) {
  using ProviderFn = const void*(__fastcall*)(void*);
  const void* const provider =
      Callable<ProviderFn>(edge155::rva::View_GetColorProvider)(view);
  if (!provider) {
    return std::nullopt;
  }
  using ColorFn = uint32_t(__fastcall*)(const void*, int);
  return Callable<ColorFn>(edge155::rva::ColorProvider_GetColor)(provider,
                                                                 colorId);
}

// The views::View a LocationBarView is, for its colours.
inline void* ViewOfLocationBar(void* locationBar) {
  return static_cast<uint8_t*>(locationBar) + edge155::off::LOCATIONBAR_view;
}

}  // namespace edgeplus::edge
