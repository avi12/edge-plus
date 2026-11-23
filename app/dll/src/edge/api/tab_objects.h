// tab_objects.h — a tab's view and its EdgeTab object: telling one from the
// other, its title label, and whether it is vertical, active or in a split.
#pragma once
#include <cstdint>
#include <cstring>
#include "edge/api/binding.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

inline bool IsTabViewInSplit(void* tabView) {
  return (Field<uint8_t>(tabView, edge155::off::TABVIEW_split_has_value) & 1) !=
         0;
}
// The SplitTabId token of the split a tab view is in (IsTabViewInSplit).
inline const uint8_t* TabViewSplitToken(void* tabView) {
  return static_cast<const uint8_t*>(tabView) +
         edge155::off::TABVIEW_split_token;
}
inline bool IsTabViewInSameSplit(void* tabView, void* other) {
  const bool isSameClass = Field<void*>(other, 0) == Field<void*>(tabView, 0);
  return isSameClass && IsTabViewInSplit(other) &&
         memcmp(reinterpret_cast<uint8_t*>(tabView) +
                    edge155::off::TABVIEW_split_token,
                reinterpret_cast<uint8_t*>(other) +
                    edge155::off::TABVIEW_split_token,
                edge155::off::TABVIEW_split_token_size) == 0;
}

// The EdgeTab (its Tab base) a style serves.
inline void* TabObjectOfStyleViews(void* styleViews) {
  return Field<void*>(styleViews, edge155::off::STYLEVIEWS_tab);
}

inline void* TabViewOfStyleViews(void* styleViews) {
  void* tab = TabObjectOfStyleViews(styleViews);
  if (!tab) {
    return nullptr;
  }
  return static_cast<uint8_t*>(tab) + edge155::off::EDGETAB_view_offset;
}

inline void* TabTitleLabel(void* tabObject) {
  return Field<void*>(tabObject, edge155::off::TAB_title);
}

inline bool IsEdgeTabView(void* view) {
  return VtableRva(view) == Rva(edge155::rva::EdgeTab_vftable);
}

// The EdgeTab (its Tab base, the `this` of Tab and EdgeTab methods) whose view
// is `view`; the inverse of TabViewOfStyleViews's step.
inline void* TabObjectOfView(void* view) {
  return static_cast<uint8_t*>(view) - edge155::off::EDGETAB_view_offset;
}

// Whether an EdgeTab view is laid out as a row of the vertical tab list, as
// EdgeTabStyleViews::UseVerticalStyle asks the tab's layout controller.
inline bool IsVerticalTabView(void* view) {
  if (!IsEdgeTabView(view)) {
    return false;
  }
  void* const controller = Field<void*>(
      TabObjectOfView(view), edge155::off::EDGETAB_layout_controller);
  if (!controller) {
    return false;
  }
  using Mode_t = uint32_t(__fastcall*)(void*);
  void* const* const vtable = *static_cast<void***>(controller);
  const uint32_t mode = reinterpret_cast<Mode_t>(
      vtable[edge155::off::LAYOUT_mode_slot])(controller);
  const bool isVerticalMode = mode - edge155::off::LAYOUT_vertical_first <
                              edge155::off::LAYOUT_vertical_count;
  return isVerticalMode;
}

// Whether the tab object is the strip's one active tab (no split counted).
inline bool IsTheActiveTabObject(void* tabObject) {
  void* controller =
      Field<void*>(tabObject, edge155::off::EDGETAB_layout_controller);
  if (!controller) {
    return false;
  }
  using Fn = bool(__fastcall*)(void*, void*);
  return reinterpret_cast<Fn>(VirtualFunction(
      controller, edge155::off::SLOTCONTROLLER_vtbl_IsActiveTab))(
      controller,
      static_cast<uint8_t*>(tabObject) + edge155::off::EDGETAB_view_offset);
}

inline bool IsTabObjectInSplit(void* tabObject) {
  return (Field<uint8_t>(tabObject, edge155::off::TABOBJECT_has_split_id) &
          1) != 0;
}

// Whether an EdgeTab object (not its view) is the strip's active tab.
inline bool IsTabObjectActive(void* tabObject) {
  using IsActive_t = bool(__fastcall*)(void*);
  return Callable<IsActive_t>(edge155::rva::Tab_IsActive)(tabObject);
}

}  // namespace edgeplus::edge
