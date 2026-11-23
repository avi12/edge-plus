// tab_layout.h — the tab list's layout (TabStripLayoutHelper): its slots, the
// ideal bounds it computes, and the bounds of the views being dragged.
#pragma once
#include <cstdint>
#include "edge/api/binding.h"
#include "edge/geom.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

inline size_t TabLayoutSlotCount(void* helper) {
  return Field<uint64_t>(helper, edge155::off::TABLAYOUT_slot_count);
}
// The tab view in slot `index`, or null for a slot that holds no tab.
inline void* TabLayoutSlotTab(void* helper, size_t index) {
  auto* slots = Field<uint8_t*>(helper, edge155::off::TABLAYOUT_slots);
  uint8_t* slot = slots + index * edge155::off::TABSLOT_size;
  const bool isTab = Field<uint32_t>(slot, edge155::off::TABSLOT_type) ==
                     edge155::off::TABSLOT_type_tab;
  return isTab ? Field<void*>(slot, edge155::off::TABSLOT_view) : nullptr;
}

// CalculateIdealBounds's result: one rect per slot.
inline Rect* IdealBoundsRects(void* result) {
  return Field<Rect*>(result, edge155::off::IDEALBOUNDS_data);
}
inline size_t IdealBoundsCount(void* result) {
  return Field<uint64_t>(result, edge155::off::IDEALBOUNDS_count);
}
// CalculateBoundsForDraggedViews: the dragged views, and the rects it returns.
inline void* const* DraggedViews(void* views) {
  return Field<void* const*>(views, edge155::off::VIEWSPAN_data);
}
inline size_t DraggedViewCount(void* views) {
  return Field<uint64_t>(views, edge155::off::VIEWSPAN_count);
}
inline Rect* RectVectorBegin(void* vector) {
  return Field<Rect*>(vector, edge155::off::RECTVECTOR_data);
}
inline size_t RectVectorCount(void* vector) {
  return Field<uint64_t>(vector, edge155::off::RECTVECTOR_size);
}

}  // namespace edgeplus::edge
