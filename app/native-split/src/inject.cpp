// Minimal LoadLibrary injector for testing fixedge-split.dll against an Edge
// instance we launched ourselves. Usage: inject.exe <pid> <abs\path\to.dll>
// (The shipping path is the companion app injecting into the browser process;
// this is the dev harness.)
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

int wmain(int argc, wchar_t** argv) {
  if (argc != 3) {
    wprintf(L"usage: inject.exe <pid> <dll path>\n");
    return 2;
  }
  DWORD pid = wcstoul(argv[1], nullptr, 10);
  const wchar_t* dll = argv[2];
  SIZE_T bytes = (wcslen(dll) + 1) * sizeof(wchar_t);

  HANDLE p =
      OpenProcess(PROCESS_CREATE_THREAD | PROCESS_QUERY_INFORMATION |
                      PROCESS_VM_OPERATION | PROCESS_VM_WRITE | PROCESS_VM_READ,
                  FALSE, pid);
  if (!p) {
    wprintf(L"OpenProcess failed: %lu\n", GetLastError());
    return 1;
  }

  void* remote = VirtualAllocEx(p, nullptr, bytes, MEM_COMMIT | MEM_RESERVE,
                                PAGE_READWRITE);
  if (!remote) {
    wprintf(L"VirtualAllocEx failed: %lu\n", GetLastError());
    return 1;
  }
  if (!WriteProcessMemory(p, remote, dll, bytes, nullptr)) {
    wprintf(L"WriteProcessMemory failed: %lu\n", GetLastError());
    return 1;
  }

  auto load = (LPTHREAD_START_ROUTINE)GetProcAddress(
      GetModuleHandleW(L"kernel32.dll"), "LoadLibraryW");
  HANDLE t = CreateRemoteThread(p, nullptr, 0, load, remote, 0, nullptr);
  if (!t) {
    wprintf(L"CreateRemoteThread failed: %lu\n", GetLastError());
    return 1;
  }
  WaitForSingleObject(t, INFINITE);
  DWORD code = 0;
  GetExitCodeThread(t, &code);
  wprintf(L"LoadLibraryW returned 0x%08lx (module handle low bits); %s\n", code,
          code ? L"loaded" : L"FAILED to load");
  VirtualFreeEx(p, remote, 0, MEM_RELEASE);
  CloseHandle(t);
  CloseHandle(p);
  return code ? 0 : 1;
}
