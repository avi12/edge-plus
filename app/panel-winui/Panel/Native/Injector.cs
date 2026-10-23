using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Text;

namespace EdgePlus;

// LoadLibraryW on a remote thread. Unloading is the DLL's own job, on its
// unload event (DllChannel).
internal static class Injector {
  private const uint LoadLibraryTimeoutMilliseconds = 10_000;
  // A DLL that unloaded itself but was re-injected meanwhile can hold several
  // references; this many FreeLibrary calls clears any real count.
  private const int MaxReferences = 16;
  private const string Kernel32 = "kernel32.dll";
  private const string LoadLibraryExport = "LoadLibraryW";
  private const string FreeLibraryExport = "FreeLibrary";

  // A re-inject only bumps the module's reference count, so it is harmless.
  public static int InjectInto(IEnumerable<int> pids, string dllPath) {
    byte[] pathBytes = Encoding.Unicode.GetBytes(dllPath + "\0");
    int injected = 0;
    foreach (int pid in pids) {
      if (InjectOne(pid, pathBytes)) {
        injected++;
      }
    }
    return injected;
  }

  // A DLL that unloaded itself but was re-injected meanwhile keeps an extra
  // reference: hooks gone, unload event closed, yet still mapped, so LoadLibrary
  // would only bump the count and never run DllMain again.
  public static void FreeModule(int pid, string moduleName) {
    for (int i = 0; i < MaxReferences; i++) {
      IntPtr? moduleBase = EdgeProcesses.ModuleBase(pid, moduleName);
      if (moduleBase is null) {
        return;
      }
      if (!CallRemote(pid, FreeLibraryExport, moduleBase.Value, ProcessAccess.Thread)) {
        return;
      }
    }
  }

  private static bool InjectOne(int pid, byte[] pathBytes) {
    IntPtr process = OpenProcess(ProcessAccess.Thread | ProcessAccess.Memory, false, (uint)pid);
    if (process == IntPtr.Zero) {
      return false;
    }
    try {
      IntPtr remotePath = VirtualAllocEx(process, IntPtr.Zero, (UIntPtr)pathBytes.Length,
                                         AllocationType.Commit | AllocationType.Reserve,
                                         MemoryProtection.ReadWrite);
      if (remotePath == IntPtr.Zero) {
        return false;
      }
      try {
        bool isPathWritten = WriteProcessMemory(process, remotePath, pathBytes,
                                                (UIntPtr)pathBytes.Length, out _);
        if (!isPathWritten) {
          return false;
        }
        return RunRemoteThread(process, LoadLibraryExport, remotePath);
      } finally {
        VirtualFreeEx(process, remotePath, UIntPtr.Zero, FreeType.Release);
      }
    } finally {
      CloseHandle(process);
    }
  }

  private static bool CallRemote(int pid, string export, IntPtr argument, ProcessAccess access) {
    IntPtr process = OpenProcess(access, false, (uint)pid);
    if (process == IntPtr.Zero) {
      return false;
    }
    try {
      return RunRemoteThread(process, export, argument);
    } finally {
      CloseHandle(process);
    }
  }

  // kernel32 is mapped at the same address in every process of a boot, so this
  // process's export address is the remote one too.
  private static bool RunRemoteThread(IntPtr process, string export, IntPtr argument) {
    IntPtr start = GetProcAddress(GetModuleHandleW(Kernel32), export);
    if (start == IntPtr.Zero) {
      return false;
    }
    IntPtr thread = CreateRemoteThread(process, IntPtr.Zero, UIntPtr.Zero, start, argument, 0,
                                       out _);
    if (thread == IntPtr.Zero) {
      return false;
    }
    WaitForSingleObject(thread, LoadLibraryTimeoutMilliseconds);
    CloseHandle(thread);
    return true;
  }

  [Flags]
  private enum ProcessAccess : uint {
    CreateThread = 0x0002,
    VmOperation = 0x0008,
    VmRead = 0x0010,
    VmWrite = 0x0020,
    QueryInformation = 0x0400,
    Thread = CreateThread | QueryInformation | VmOperation,
    Memory = VmWrite | VmRead,
  }

  [Flags]
  private enum AllocationType : uint {
    Commit = 0x1000,
    Reserve = 0x2000,
  }

  private enum FreeType : uint {
    Release = 0x8000,
  }

  private enum MemoryProtection : uint {
    ReadWrite = 0x04,
  }

  [DllImport("kernel32.dll", SetLastError = true)]
  private static extern IntPtr OpenProcess(ProcessAccess access, bool inheritHandle,
                                           uint processId);

  [DllImport("kernel32.dll", SetLastError = true)]
  private static extern IntPtr VirtualAllocEx(IntPtr process, IntPtr address, UIntPtr size,
                                              AllocationType allocationType,
                                              MemoryProtection protection);

  [DllImport("kernel32.dll", SetLastError = true)]
  [return: MarshalAs(UnmanagedType.Bool)]
  private static extern bool WriteProcessMemory(IntPtr process, IntPtr address, byte[] buffer,
                                                UIntPtr size, out UIntPtr written);

  [DllImport("kernel32.dll", SetLastError = true)]
  private static extern IntPtr CreateRemoteThread(IntPtr process, IntPtr attributes,
                                                  UIntPtr stackSize, IntPtr start,
                                                  IntPtr parameter, uint flags,
                                                  out uint threadId);

  [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  private static extern IntPtr GetModuleHandleW(string name);

  [DllImport("kernel32.dll", CharSet = CharSet.Ansi, SetLastError = true)]
  private static extern IntPtr GetProcAddress(IntPtr module, string name);

  [DllImport("kernel32.dll")]
  private static extern uint WaitForSingleObject(IntPtr handle, uint milliseconds);

  [DllImport("kernel32.dll")]
  [return: MarshalAs(UnmanagedType.Bool)]
  private static extern bool VirtualFreeEx(IntPtr process, IntPtr address, UIntPtr size,
                                           FreeType freeType);

  [DllImport("kernel32.dll")]
  [return: MarshalAs(UnmanagedType.Bool)]
  private static extern bool CloseHandle(IntPtr handle);
}
