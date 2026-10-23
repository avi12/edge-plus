// Minimal LoadLibrary injector for testing fixedge-split.dll against an Edge
// instance we launched ourselves (the dev harness; the panel injects in use).
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "handles.h"

namespace {

using fixedge::handles::UniqueHandle;

constexpr int kPidArgument = 1;
constexpr int kDllPathArgument = 2;
constexpr int kArgumentCount = 3;

constexpr wchar_t kKernelModule[] = L"kernel32.dll";
constexpr char kLoadLibraryExport[] = "LoadLibraryW";
constexpr int kDecimalBase = 10;
constexpr int kUsageExitCode = 2;

// Memory committed in another process, released there when this goes out of
// scope, so a failed write or thread start leaves nothing behind in Edge.
class RemoteAllocation {
 public:
  RemoteAllocation(HANDLE inProcess, SIZE_T byteCount) noexcept
      : process(inProcess),
        address(VirtualAllocEx(inProcess, nullptr, byteCount,
                               MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE)) {
  }
  ~RemoteAllocation() {
    if (address) {
      VirtualFreeEx(process, address, 0, MEM_RELEASE);
    }
  }
  RemoteAllocation(const RemoteAllocation&) = delete;
  RemoteAllocation& operator=(const RemoteAllocation&) = delete;
  RemoteAllocation(RemoteAllocation&&) = delete;
  RemoteAllocation& operator=(RemoteAllocation&&) = delete;

  [[nodiscard]] void* Address() const noexcept {
    return address;
  }

 private:
  HANDLE process = nullptr;
  void* address = nullptr;
};

}  // namespace

int wmain(int argc, wchar_t** argv) {
  const bool hasExpectedArguments = argc == kArgumentCount;
  if (!hasExpectedArguments) {
    wprintf(L"usage: inject.exe <pid> <dll path>\n");
    return kUsageExitCode;
  }
  const DWORD pid = wcstoul(argv[kPidArgument], nullptr, kDecimalBase);
  const wchar_t* dllPath = argv[kDllPathArgument];
  const SIZE_T pathBytes = (wcslen(dllPath) + 1) * sizeof(wchar_t);

  // Declared in the order they are made, so they are given back in reverse:
  // the thread handle, the remote path, then the process handle.
  const UniqueHandle process{
      OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                      PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                  FALSE, pid)};
  if (!process) {
    wprintf(L"OpenProcess failed: %lu\n", GetLastError());
    return EXIT_FAILURE;
  }

  const RemoteAllocation remotePath{process.Get(), pathBytes};
  if (!remotePath.Address()) {
    wprintf(L"VirtualAllocEx failed: %lu\n", GetLastError());
    return EXIT_FAILURE;
  }
  const bool isPathWritten =
      WriteProcessMemory(process.Get(), remotePath.Address(), dllPath,
                         pathBytes, nullptr) != 0;
  if (!isPathWritten) {
    wprintf(L"WriteProcessMemory failed: %lu\n", GetLastError());
    return EXIT_FAILURE;
  }

  // kernel32 is mapped at the same address in every process, so our
  // LoadLibraryW is a valid start address in the target too.
  auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
      GetProcAddress(GetModuleHandleW(kKernelModule), kLoadLibraryExport));
  const UniqueHandle thread{
      CreateRemoteThread(process.Get(), nullptr, 0, loadLibrary,
                         remotePath.Address(), 0, nullptr)};
  if (!thread) {
    wprintf(L"CreateRemoteThread failed: %lu\n", GetLastError());
    return EXIT_FAILURE;
  }
  WaitForSingleObject(thread.Get(), INFINITE);
  DWORD exitCode = 0;
  GetExitCodeThread(thread.Get(), &exitCode);
  wprintf(L"LoadLibraryW returned 0x%08lx (module handle low bits); %s\n",
          exitCode, exitCode ? L"loaded" : L"FAILED to load");
  return exitCode ? EXIT_SUCCESS : EXIT_FAILURE;
}
