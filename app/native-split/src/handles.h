// handles.h — scope owners for Win32 resources (RAII). What outlives a scope is
// not owned here: the DLL's signal events live until unload, and a window class
// brush belongs to its class once registered.
#pragma once
#include <windows.h>
#include <utility>

namespace fixedge::handles {

template <class Traits>
class Unique {
 public:
  using Handle = typename Traits::Handle;

  Unique() noexcept = default;
  explicit Unique(Handle owned) noexcept : handle(owned) {
  }
  ~Unique() {
    Reset();
  }
  Unique(const Unique&) = delete;
  Unique& operator=(const Unique&) = delete;
  Unique(Unique&& other) noexcept
      : handle(std::exchange(other.handle, Handle{})) {
  }
  Unique& operator=(Unique&& other) noexcept {
    if (this != &other) {
      Reset(std::exchange(other.handle, Handle{}));
    }
    return *this;
  }

  [[nodiscard]] Handle Get() const noexcept {
    return handle;
  }
  [[nodiscard]] bool IsValid() const noexcept {
    return Traits::IsValid(handle);
  }
  explicit operator bool() const noexcept {
    return IsValid();
  }

  Handle Release() noexcept {
    return std::exchange(handle, Handle{});
  }

  void Reset(Handle replacement = Handle{}) noexcept {
    if (IsValid()) {
      Traits::Close(handle);
    }
    handle = replacement;
  }

 private:
  Handle handle{};
};

// Win32 spells "no handle" two ways: null from most creators,
// INVALID_HANDLE_VALUE from CreateFile.
struct KernelTraits {
  using Handle = HANDLE;
  static bool IsValid(HANDLE handle) noexcept {
    return handle != nullptr && handle != INVALID_HANDLE_VALUE;
  }
  static void Close(HANDLE handle) noexcept {
    CloseHandle(handle);
  }
};
using UniqueHandle = Unique<KernelTraits>;

template <class GdiHandle>
struct GdiObjectTraits {
  using Handle = GdiHandle;
  static bool IsValid(GdiHandle object) noexcept {
    return object != nullptr;
  }
  static void Close(GdiHandle object) noexcept {
    DeleteObject(object);
  }
};
using UniqueBitmap = Unique<GdiObjectTraits<HBITMAP>>;
using UniqueBrush = Unique<GdiObjectTraits<HBRUSH>>;

struct MemoryDcTraits {
  using Handle = HDC;
  static bool IsValid(HDC context) noexcept {
    return context != nullptr;
  }
  static void Close(HDC context) noexcept {
    DeleteDC(context);
  }
};
using UniqueMemoryDc = Unique<MemoryDcTraits>;

// A null window lends the whole screen's DC.
class WindowDc {
 public:
  explicit WindowDc(HWND ofWindow) noexcept
      : window(ofWindow), context(GetDC(ofWindow)) {
  }
  ~WindowDc() {
    if (context) {
      ReleaseDC(window, context);
    }
  }
  WindowDc(const WindowDc&) = delete;
  WindowDc& operator=(const WindowDc&) = delete;
  WindowDc(WindowDc&&) = delete;
  WindowDc& operator=(WindowDc&&) = delete;

  [[nodiscard]] HDC Get() const noexcept {
    return context;
  }

 private:
  HWND window = nullptr;
  HDC context = nullptr;
};

// Declare the object's owner first, so the displaced object is selected back
// before the object itself is deleted.
class SelectedObject {
 public:
  SelectedObject(HDC intoContext, HGDIOBJ object) noexcept
      : context(intoContext), previous(SelectObject(intoContext, object)) {
  }
  ~SelectedObject() {
    SelectObject(context, previous);
  }
  SelectedObject(const SelectedObject&) = delete;
  SelectedObject& operator=(const SelectedObject&) = delete;
  SelectedObject(SelectedObject&&) = delete;
  SelectedObject& operator=(SelectedObject&&) = delete;

 private:
  HDC context = nullptr;
  HGDIOBJ previous = nullptr;
};

class OpenedClipboard {
 public:
  OpenedClipboard() noexcept : isOpen(OpenClipboard(nullptr) != FALSE) {
  }
  ~OpenedClipboard() {
    if (isOpen) {
      CloseClipboard();
    }
  }
  OpenedClipboard(const OpenedClipboard&) = delete;
  OpenedClipboard& operator=(const OpenedClipboard&) = delete;
  OpenedClipboard(OpenedClipboard&&) = delete;
  OpenedClipboard& operator=(OpenedClipboard&&) = delete;

  [[nodiscard]] bool IsOpen() const noexcept {
    return isOpen;
  }

 private:
  bool isOpen = false;
};

class GlobalMemoryLock {
 public:
  explicit GlobalMemoryLock(HGLOBAL lockedMemory) noexcept
      : memory(lockedMemory),
        data(lockedMemory ? GlobalLock(lockedMemory) : nullptr) {
  }
  ~GlobalMemoryLock() {
    if (data) {
      GlobalUnlock(memory);
    }
  }
  GlobalMemoryLock(const GlobalMemoryLock&) = delete;
  GlobalMemoryLock& operator=(const GlobalMemoryLock&) = delete;
  GlobalMemoryLock(GlobalMemoryLock&&) = delete;
  GlobalMemoryLock& operator=(GlobalMemoryLock&&) = delete;

  [[nodiscard]] const void* Data() const noexcept {
    return data;
  }

 private:
  HGLOBAL memory = nullptr;
  void* data = nullptr;
};

}  // namespace fixedge::handles
