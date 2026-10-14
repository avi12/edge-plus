// Minimal LoadLibrary injector for testing fixedge-split.dll against an Edge
// instance we launched ourselves. Usage: inject.exe <pid> <abs\path\to.dll>
// (The shipping path is the companion app injecting into the browser process;
// this is the dev harness.)
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

// argv positions: inject.exe <pid> <dll path>
constexpr int kPidArgument = 1;
constexpr int kDllPathArgument = 2;
constexpr int kArgumentCount = 3;

constexpr int kDecimalBase = 10;
constexpr int kUsageExitCode = 2;

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

  HANDLE process =
      OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                      PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                  FALSE, pid);
  if (!process) {
    wprintf(L"OpenProcess failed: %lu\n", GetLastError());
    return EXIT_FAILURE;
  }

  void* remotePath = VirtualAllocEx(process, nullptr, pathBytes,
                                    MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!remotePath) {
    wprintf(L"VirtualAllocEx failed: %lu\n", GetLastError());
    return EXIT_FAILURE;
  }
  if (!WriteProcessMemory(process, remotePath, dllPath, pathBytes, nullptr)) {
    wprintf(L"WriteProcessMemory failed: %lu\n", GetLastError());
    return EXIT_FAILURE;
  }

  // kernel32 is mapped at the same address in every process, so our
  // LoadLibraryW is a valid start address in the target too.
  auto loadLibrary = reinterpret_cast<LPTHREAD_START_ROUTINE>(
      GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW"));
  HANDLE thread = CreateRemoteThread(process, nullptr, 0, loadLibrary,
                                     remotePath, 0, nullptr);
  if (!thread) {
    wprintf(L"CreateRemoteThread failed: %lu\n", GetLastError());
    return EXIT_FAILURE;
  }
  WaitForSingleObject(thread, INFINITE);
  DWORD exitCode = 0;
  GetExitCodeThread(thread, &exitCode);
  wprintf(L"LoadLibraryW returned 0x%08lx (module handle low bits); %s\n",
          exitCode, exitCode ? L"loaded" : L"FAILED to load");
  VirtualFreeEx(process, remotePath, 0, MEM_RELEASE);
  CloseHandle(thread);
  CloseHandle(process);
  return exitCode ? EXIT_SUCCESS : EXIT_FAILURE;
}
