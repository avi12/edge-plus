using Microsoft.Win32;

namespace EdgePlus;

// The registry checks write under a throwaway HKCU\Software\EdgePlusTest key,
// never the real Run key, and delete it afterwards.
internal static class StartupEntryTests {
  private const string TestRoot = @"Software\EdgePlusTest";
  private const string TestName = "Edge+ test";

  public static void Run() {
    Expect.Equal("\"C:\\Program Files\\Edge+\\edgeplus.exe\" --background",
                 StartupEntry.RunCommand(@"C:\Program Files\Edge+\edgeplus.exe"),
                 "the command quotes a path with spaces");
    Expect.That(StartupEntry.IsDisabledFlag([0x03, 0, 0]), "0x03 is off");
    Expect.That(StartupEntry.IsDisabledFlag([0x07]), "0x07 is off");
    Expect.That(!StartupEntry.IsDisabledFlag([0x02, 0, 0]), "0x02 is on");
    Expect.That(!StartupEntry.IsDisabledFlag([0x06]), "0x06 is on");
    Expect.That(!StartupEntry.IsDisabledFlag([]), "an empty flag is on");

    StartupKeys keys = new($@"{TestRoot}\Run", $@"{TestRoot}\Approved");
    Cleanup();
    try {
      Expect.That(!StartupEntry.IsEnabled(keys, TestName), "off before it is written");
      StartupEntry.Enable(keys, TestName, "x.exe --background");
      Expect.That(StartupEntry.IsEnabled(keys, TestName), "on once written");

      using (RegistryKey approved = Registry.CurrentUser.CreateSubKey(keys.Approved)) {
        approved.SetValue(TestName, new byte[] { 0x03, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
                          RegistryValueKind.Binary);
      }
      Expect.That(!StartupEntry.IsEnabled(keys, TestName), "Task Manager's flag shows off");
      StartupEntry.Enable(keys, TestName, "x.exe --background");
      Expect.That(StartupEntry.IsEnabled(keys, TestName), "enabling clears Task Manager's flag");

      StartupEntry.Disable(keys, TestName);
      Expect.That(!StartupEntry.IsEnabled(keys, TestName), "off once removed");
      StartupEntry.Disable(keys, TestName);
      Expect.That(!StartupEntry.IsEnabled(keys, TestName), "removing twice is not an error");

      MigratesOnlyATauriEntry(keys);
    } finally {
      Cleanup();
    }
  }

  private static void MigratesOnlyATauriEntry(StartupKeys keys) {
    string winUi = @"C:\Edge+\edgeplus.exe";
    Expect.That(StartupEntry.IsTauriCommand(
                    "\"C:\\repos\\fix-edge\\target\\debug\\fix-edge-panel.exe\" --background"),
                "the Tauri source build is the Tauri panel");
    Expect.That(StartupEntry.IsTauriCommand(@"C:\Users\a\AppData\Local\Edge+\Edge+.exe"),
                "the installed Tauri panel, unquoted, is too");
    Expect.That(!StartupEntry.IsTauriCommand(StartupEntry.RunCommand(winUi)),
                "this panel is not");

    Expect.That(!StartupEntry.MigrateFromTauri(keys, TestName, winUi), "no entry, nothing moved");
    StartupEntry.Enable(keys, TestName, "\"D:\\x\\fix-edge-panel.exe\" --background");
    using (RegistryKey approved = Registry.CurrentUser.CreateSubKey(keys.Approved)) {
      approved.SetValue(TestName, new byte[] { 0x03, 0, 0, 0 }, RegistryValueKind.Binary);
    }
    Expect.That(StartupEntry.MigrateFromTauri(keys, TestName, winUi), "a Tauri entry is moved");
    using (RegistryKey run = Registry.CurrentUser.OpenSubKey(keys.Run)!) {
      Expect.Equal(StartupEntry.RunCommand(winUi), run.GetValue(TestName) as string,
                   "it now starts this panel as the listener");
    }
    Expect.That(!StartupEntry.IsEnabled(keys, TestName),
                "Task Manager's off flag is kept through the move");
    Expect.That(!StartupEntry.MigrateFromTauri(keys, TestName, winUi),
                "an entry for this panel is left alone");
  }

  private static void Cleanup() =>
      Registry.CurrentUser.DeleteSubKeyTree(TestRoot, throwOnMissingSubKey: false);
}
