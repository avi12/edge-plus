// events.h — a ui::Event's fields: its type, a key event's key and a mouse
// event's click count and location.
#pragma once
#include <cstdint>
#include <cstring>
#include "edge/api/binding.h"
#include "edge/geom.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

inline int MouseClickCount(const void* mouseEvent) {
  using CountFn = int(__fastcall*)(const void*);
  return Callable<CountFn>(edge155::rva::MouseEvent_GetClickCount)(mouseEvent);
}

inline int EventType(void* event) {
  return Field<int32_t>(event, edge155::off::EVENT_type);
}
// A key event's ui::KeyboardCode (the Windows virtual-key code).
inline int KeyEventCode(void* keyEvent) {
  return Field<uint8_t>(keyEvent, edge155::off::KEYEVENT_key_code);
}

// Where a mouse event happened, in the window's DIPs (its root location).
inline Point EventRootLocation(const void* event) {
  const auto* location =
      reinterpret_cast<const float*>(reinterpret_cast<const uint8_t*>(event) +
                                     edge155::off::EVENT_root_location);
  return Point{static_cast<int32_t>(location[0]),
               static_cast<int32_t>(location[1])};
}

// The Windows virtual-key code of the key `keyEvent` (a ui::KeyEvent) reports
// being pressed, or 0 when it is not a key press.
inline uint8_t PressedKeyCode(const void* keyEvent) {
  const auto* bytes = static_cast<const uint8_t*>(keyEvent);
  int32_t type = 0;
  std::memcpy(&type, bytes + edge155::off::KEYEVENT_type, sizeof(type));
  const bool isPressed = type == edge155::off::KEYEVENT_type_pressed;
  return isPressed ? bytes[edge155::off::KEYEVENT_key_code] : uint8_t{0};
}

inline bool IsEnterPressed(const void* keyEvent) {
  return PressedKeyCode(keyEvent) == edge155::off::KEYEVENT_key_enter;
}

}  // namespace edgeplus::edge
