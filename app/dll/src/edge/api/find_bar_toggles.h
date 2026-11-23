// find_bar_toggles.h — the find bar's option toggles: finding, adding and
// setting one.
#pragma once
#include <cstdint>
#include "edge/api/binding.h"
#include "edge/api/find_bar.h"
#include "edge/api/strings.h"
#include "edge/api/views.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

// The options view (a column of toggles), or null before the bar first needed it.
inline void* FindOptionsView(void* findBarView) {
  return Field<void*>(findBarView, edge155::off::FINDBARVIEW_options_view);
}

// The toggle built for `id`, or null. A walk down the options view's map, as
// Edge's own AddFindInPageOptionChildView does.
inline void* FindToggle(void* findBarView, int toggleId) {
  void* optionsView = FindOptionsView(findBarView);
  if (!optionsView) {
    return nullptr;
  }
  auto* node =
      Field<uint8_t*>(optionsView, edge155::off::FINDOPTIONS_toggle_map_root);
  while (node) {
    const int key = Field<int32_t>(node, edge155::off::MAPNODE_key);
    if (toggleId == key) {
      return Field<void*>(node, edge155::off::MAPNODE_value);
    }
    node = Field<uint8_t*>(node, toggleId < key ? edge155::off::MAPNODE_left
                                                : edge155::off::MAPNODE_right);
  }
  return nullptr;
}

inline void* FindBarViewOfToggle(void* toggle) {
  void* host = Field<void*>(toggle, edge155::off::FINDTOGGLE_host);
  return host ? Field<void*>(host, edge155::off::FINDBARHOST_view) : nullptr;
}

inline int FindToggleId(void* toggle) {
  return Field<int32_t>(toggle, edge155::off::FINDTOGGLE_id);
}

inline bool FindToggleIsOn(void* toggle) {
  const void* const button =
      Field<void*>(toggle, edge155::off::FINDTOGGLE_button);
  using Fn = bool(__fastcall*)(const void*);
  return (button != nullptr) &&
         Callable<Fn>(edge155::rva::EdgeToggleButton_GetIsOn)(button);
}

// Move the switch alone: Edge's SetToggleState would also write the id into its
// own find parameters, which means nothing for our two.
inline void FindToggleSetOn(void* toggle, bool isOn) {
  const void* const button =
      Field<void*>(toggle, edge155::off::FINDTOGGLE_button);
  using Fn = void(__fastcall*)(const void*, bool);
  if (button) {
    Callable<Fn>(edge155::rva::EdgeToggleButton_SetIsOn)(button, isOn);
  }
}

// Enable or grey out a whole toggle: the title and the switch (a disabled
// switch takes no click).
inline void FindToggleSetEnabled(void* toggle, bool isEnabled) {
  ViewSetEnabled(Field<void*>(toggle, edge155::off::FINDTOGGLE_label),
                 isEnabled);
  ViewSetEnabled(Field<void*>(toggle, edge155::off::FINDTOGGLE_button),
                 isEnabled);
}

inline void FindToggleSetTitle(void* toggle, U16View title) {
  const void* const label =
      Field<void*>(toggle, edge155::off::FINDTOGGLE_label);
  using Fn = void(__fastcall*)(const void*, const U16View*);
  Callable<Fn>(edge155::rva::Label_SetText)(label, &title);
}

// Build toggle `id` at the end of the options view, sized like Edge's own
// (whose width it reads off one that exists). Null when there is no options view
// or no toggle of Edge's to size by.
inline void* AddFindToggle(void* findBarView, int toggleId) {
  const void* const optionsView = FindOptionsView(findBarView);
  void* sibling = nullptr;
  for (const int siblingId : {edge155::off::FINDTOGGLE_id_match_case,
                              edge155::off::FINDTOGGLE_id_whole_word,
                              edge155::off::FINDTOGGLE_id_diacritics}) {
    if (!sibling) {
      sibling = FindToggle(findBarView, siblingId);
    }
  }
  if (!optionsView || !sibling) {
    return nullptr;
  }
  using Fn = void(__fastcall*)(const void*, int, int, void*);
  Callable<Fn>(
      edge155::rva::FindOptionsControlView_AddFindInPageOptionChildView)(
      optionsView, toggleId,
      Field<int32_t>(sibling, edge155::off::FINDTOGGLE_width),
      Field<void*>(findBarView, edge155::off::FINDBARVIEW_host));
  return FindToggle(findBarView, toggleId);
}

// The options view grew or shrank: lay it out again and let the bar re-measure.
inline void FindBarRelayoutOptions(void* findBarView) {
  InvalidateLayout(FindOptionsView(findBarView));
  FindBarPreferredSizeChanged(findBarView);
}

}  // namespace edgeplus::edge
