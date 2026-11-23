// menus.h — Edge's menus: a ui::SimpleMenuModel's items and labels, a
// views::MenuItemView and its icon, and the menu that is open.
#pragma once
#include <cstdint>
#include <cstring>
#include "edge/api/binding.h"
#include "edge/api/strings.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

// Make a menu item measure itself again after `child` was added to it.
inline void MenuItemRemeasure(void* menuItem, void* child) {
  using ChangedFn = void(__fastcall*)(void*, void*);
  Callable<ChangedFn>(edge155::rva::MenuItemView_ChildPreferredSizeChanged)(
      menuItem, child);
}

inline void* MenuItemById(void* rootMenuItem, int commandId) {
  using ByIdFn = void*(__fastcall*)(void*, int);
  return Callable<ByIdFn>(edge155::rva::MenuItemView_GetMenuItemByID)(
      rootMenuItem, commandId);
}

// ui::SimpleMenuModel::AddItemWithIcon with `icon` (a gfx::VectorIcon) drawn
// the way Edge draws its split ⋯ items' icons.
inline void MenuAddItemWithIcon(void* model, int commandId,
                                const U16String* label, const void* icon) {
  alignas(16) uint8_t colorVariant[edge155::off::ColorVariant_size] = {};
  using ColorFn = void*(__fastcall*)(void*, int);
  Callable<ColorFn>(edge155::rva::ColorVariant_ctor)(
      colorVariant, edge155::off::MENU_icon_color_id);
  uint64_t colorValue = 0;
  memcpy(&colorValue, colorVariant, sizeof(colorValue));
  alignas(16) uint8_t image[edge155::off::ImageModel_size_bound] = {};
  using FromIconFn =
      void*(__fastcall*)(void*, const void*, uint64_t, int, const void*);
  Callable<FromIconFn>(edge155::rva::ImageModel_FromVectorIcon)(
      image, icon, colorValue, 0, nullptr);
  using AddFn = void(__fastcall*)(void*, int, const void*, const void*);
  Callable<AddFn>(edge155::rva::SimpleMenuModel_AddItemWithIcon)(
      model, commandId, label, image);
  using DtorFn = void(__fastcall*)(void*);
  Callable<DtorFn>(edge155::rva::ImageModel_dtor)(image);
}

// ui::SimpleMenuModel::AddSeparator(model, NORMAL_SEPARATOR).
inline void MenuAddSeparator(void* model) {
  using SeparatorFn = void(__fastcall*)(void*, int);
  Callable<SeparatorFn>(edge155::rva::SimpleMenuModel_AddSeparator)(
      model, edge155::off::MENU_normal_separator);
}

// ui::SimpleMenuModel::AddItem(model /*rcx*/, int command_id /*edx*/, &u16string /*r8*/).
inline void MenuAddItem(void* model, int commandId, const U16String* label) {
  using Fn = void*(__fastcall*)(void*, int, const void*);
  Callable<Fn>(edge155::rva::SimpleMenuModel_AddItem)(model, commandId, label);
}

// A ui::SimpleMenuModel's items, by index (each call CHECKs the index).
inline size_t MenuItemCount(void* model) {
  using CountFn = size_t(__fastcall*)(void*);
  return Callable<CountFn>(edge155::rva::SimpleMenuModel_GetItemCount)(model);
}
inline int MenuCommandIdAt(void* model, size_t index) {
  using IdFn = int(__fastcall*)(void*, size_t);
  return Callable<IdFn>(edge155::rva::SimpleMenuModel_GetCommandIdAt)(model,
                                                                      index);
}
inline void MenuRemoveItemAt(void* model, size_t index) {
  using RemoveFn = void(__fastcall*)(void*, size_t);
  Callable<RemoveFn>(edge155::rva::SimpleMenuModel_RemoveItemAt)(model, index);
}
// The label is copied into the item.
inline void MenuSetLabel(void* model, size_t index, const U16String* label) {
  using LabelFn = void(__fastcall*)(void*, size_t, const void*);
  Callable<LabelFn>(edge155::rva::SimpleMenuModel_SetLabel)(model, index,
                                                            label);
}

inline void* MenuItemIconView(void* menuItem) {
  return Field<void*>(menuItem, edge155::off::MENUITEM_icon_view);
}

// The open menu's MenuController, or null.
inline const void* ActiveMenu() {
  using ActiveFn = const void*(__fastcall*)();
  return Callable<ActiveFn>(edge155::rva::MenuController_GetActiveInstance)();
}

// The views::BubbleBorder::Arrow of a bubble's border.
inline int BubbleBorderArrow(const void* border) {
  return Field<int>(border, edge155::off::BUBBLEBORDER_arrow);
}

// Close the open menu, if any, as MenuRunner::Cancel does.
inline void CancelActiveMenu() {
  const void* controller = ActiveMenu();
  if (!controller) {
    return;
  }
  using CancelFn = void(__fastcall*)(const void*, int, int);
  Callable<CancelFn>(edge155::rva::MenuController_Cancel)(
      controller, edge155::off::MENU_exit_all, 0);
}

// The MenuItemView whose views::LayoutDelegate base `layoutDelegate` is.
inline void* MenuItemOfLayoutDelegate(void* layoutDelegate) {
  return static_cast<uint8_t*>(layoutDelegate) -
         edge155::off::MENUITEM_layout_delegate;
}

}  // namespace edgeplus::edge
