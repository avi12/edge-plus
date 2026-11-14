// handles.h — scope owners for Win32 resources (RAII). What outlives a scope is
// not owned here: the DLL's signal events live until unload, and a window class
// brush belongs to its class once registered.
#pragma once
#include <windows.h>
#include <utility>

namespace edgeplus::handles {

template <class Traits>
class Unique {
 public:
  using Handle = typename Traits::Handle;

  Unique() noexcept = default;
  explicit Unique(Handle owned) noexcept : handle_(owned) {
  }
  ~Unique() {
    Reset();
  }
  Unique(const Unique&) = delete;
  Unique& operator=(const Unique&) = delete;
  Unique(Unique&& other) noexcept
      : handle_(std::exchange(other.handle_, Handle{})) {
  }
  Unique& operator=(Unique&& other) noexcept {
    if (this != &other) {
      Reset(std::exchange(other.handle_, Handle{}));
    }
    return *this;
  }

  [[nodiscard]] Handle Get() const noexcept {
    return handle_;
  }
  [[nodiscard]] bool IsValid() const noexcept {
    return Traits::IsValid(handle_);
  }
  explicit operator bool() const noexcept {
    return IsValid();
  }

  Handle Release() noexcept {
    return std::exchange(handle_, Handle{});
  }

  void Reset(Handle replacement = Handle{}) noexcept {
    if (IsValid()) {
      Traits::Close(handle_);
    }
    handle_ = replacement;
  }

 private:
  Handle handle_{};
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
      : window_(ofWindow), context_(GetDC(ofWindow)) {
  }
  ~WindowDc() {
    if (context_) {
      ReleaseDC(window_, context_);
    }
  }
  WindowDc(const WindowDc&) = delete;
  WindowDc& operator=(const WindowDc&) = delete;
  WindowDc(WindowDc&&) = delete;
  WindowDc& operator=(WindowDc&&) = delete;

  [[nodiscard]] HDC Get() const noexcept {
    return context_;
  }

 private:
  HWND window_ = nullptr;
  HDC context_ = nullptr;
};

// Declare the object's owner first, so the displaced object is selected back
// before the object itself is deleted.
class SelectedObject {
 public:
  SelectedObject(HDC intoContext, HGDIOBJ object) noexcept
      : context_(intoContext), previous_(SelectObject(intoContext, object)) {
  }
  ~SelectedObject() {
    SelectObject(context_, previous_);
  }
  SelectedObject(const SelectedObject&) = delete;
  SelectedObject& operator=(const SelectedObject&) = delete;
  SelectedObject(SelectedObject&&) = delete;
  SelectedObject& operator=(SelectedObject&&) = delete;

 private:
  HDC context_ = nullptr;
  HGDIOBJ previous_ = nullptr;
};

class OpenedClipboard {
 public:
  OpenedClipboard() noexcept : isOpen_(OpenClipboard(nullptr) != FALSE) {
  }
  ~OpenedClipboard() {
    if (isOpen_) {
      CloseClipboard();
    }
  }
  OpenedClipboard(const OpenedClipboard&) = delete;
  OpenedClipboard& operator=(const OpenedClipboard&) = delete;
  OpenedClipboard(OpenedClipboard&&) = delete;
  OpenedClipboard& operator=(OpenedClipboard&&) = delete;

  [[nodiscard]] bool IsOpen() const noexcept {
    return isOpen_;
  }

 private:
  bool isOpen_ = false;
};

class GlobalMemoryLock {
 public:
  explicit GlobalMemoryLock(HGLOBAL lockedMemory) noexcept
      : memory_(lockedMemory),
        data_(lockedMemory ? GlobalLock(lockedMemory) : nullptr) {
  }
  ~GlobalMemoryLock() {
    if (data_) {
      GlobalUnlock(memory_);
    }
  }
  GlobalMemoryLock(const GlobalMemoryLock&) = delete;
  GlobalMemoryLock& operator=(const GlobalMemoryLock&) = delete;
  GlobalMemoryLock(GlobalMemoryLock&&) = delete;
  GlobalMemoryLock& operator=(GlobalMemoryLock&&) = delete;

  [[nodiscard]] const void* Data() const noexcept {
    return data_;
  }

 private:
  HGLOBAL memory_ = nullptr;
  void* data_ = nullptr;
};

}  // namespace edgeplus::handles
