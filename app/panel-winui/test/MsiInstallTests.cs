namespace EdgePlus;

internal static class MsiInstallTests {
  private const string Msi = @"C:\Users\A\AppData\Local\EdgePlus\update\EdgePlus-0.0.2.msi";
  private const string Exe = @"C:\Program Files\Edge+\edgeplus.exe";

  public static void Run() {
    MatchesTheRecordedFolder();
    KeepsTheShortcutsAsTheyAre();
    WaitsThenInstallsThenRestarts();
  }

  private static void MatchesTheRecordedFolder() {
    Expect.That(MsiInstall.IsSameFolder(@"C:\Program Files\Edge+\", @"C:\Program Files\Edge+"),
                "the installer's trailing separator does not matter");
    Expect.That(MsiInstall.IsSameFolder(@"C:\Program Files\Edge+", @"c:\program files\edge+"),
                "nor does case");
    Expect.That(!MsiInstall.IsSameFolder(@"C:\Program Files\Edge+", @"C:\Tools\Edge+"),
                "a portable copy elsewhere is not the installed one");
    Expect.That(!MsiInstall.IsSameFolder(@"C:\Program Files\Edge+", null),
                "an exe with no folder is not either");
    Expect.Equal(@"C:\Users\Public\Desktop\Edge+.lnk",
                 MsiInstall.ShortcutFile(@"C:\Users\Public\Desktop", "Edge+"),
                 "a shortcut is named after the product");
  }

  private static void KeepsTheShortcutsAsTheyAre() {
    Expect.Equal($"/i \"{Msi}\" /passive /norestart",
                 MsiInstall.InstallerArguments(Msi, hasDesktopShortcut: true, hasStartMenuShortcut: true),
                 "both shortcuts present: the installer's defaults keep them");
    Expect.Equal($"/i \"{Msi}\" /passive /norestart DESKTOPSHORTCUT=\"\"",
                 MsiInstall.InstallerArguments(Msi, hasDesktopShortcut: false, hasStartMenuShortcut: true),
                 "a removed desktop shortcut stays removed");
    Expect.Equal($"/i \"{Msi}\" /passive /norestart DESKTOPSHORTCUT=\"\" STARTMENUSHORTCUT=\"\"",
                 MsiInstall.InstallerArguments(Msi, hasDesktopShortcut: false, hasStartMenuShortcut: false),
                 "and so does a removed Start menu one");
  }

  private static void WaitsThenInstallsThenRestarts() {
    string script = MsiInstall.UpdateScript(42, "/i \"x\" /passive", @"C:\it's\edgeplus.exe");
    Expect.Equal("Wait-Process -Id 42 -ErrorAction SilentlyContinue\n"
                     + "Start-Process msiexec.exe -ArgumentList '/i \"x\" /passive' -Wait\n"
                     + @"Start-Process 'C:\it''s\edgeplus.exe'" + "\n",
                 script, "waits for the panel, installs, then starts the exe, quotes escaped");
    string command = MsiInstall.HiddenPowerShell(script);
    string encoded = command[(command.LastIndexOf(' ') + 1)..];
    Expect.Equal(script,
                 System.Text.Encoding.Unicode.GetString(System.Convert.FromBase64String(encoded)),
                 "the hidden PowerShell carries the script encoded");
    Expect.That(command.StartsWith("powershell.exe ") && command.Contains("-WindowStyle Hidden"),
                "in a hidden window");
  }
}
