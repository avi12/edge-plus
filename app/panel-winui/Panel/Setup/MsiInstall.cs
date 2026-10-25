using System;
using System.IO;
using System.Text;
using Microsoft.Win32;

namespace EdgePlus;

// A copy the MSI installed (app/installer) lives under Program Files, where it
// cannot swap its own exe, so it updates by running the newer release's MSI
// once the panel has exited. Package.wxs writes the key read here and names the
// shortcuts after the product.
internal static class MsiInstall {
  private const string RegistryKeyPath = @"Software\EdgePlus";
  private const string InstallDirValue = "InstallDir";
  private const string ShortcutExtension = ".lnk";
  // The installer's properties that keep or drop each shortcut.
  private const string DesktopShortcutProperty = "DESKTOPSHORTCUT";
  private const string StartMenuShortcutProperty = "STARTMENUSHORTCUT";
  private const string PowerShellCommand =
      "powershell.exe -NoProfile -NonInteractive -WindowStyle Hidden -EncodedCommand";

  // True when the installer recorded the folder this exe runs from.
  public static bool IsInstalledAt(string executable) {
    using RegistryKey? key = Registry.LocalMachine.OpenSubKey(RegistryKeyPath);
    return key?.GetValue(InstallDirValue) is string folder
        && IsSameFolder(folder, Path.GetDirectoryName(executable));
  }

  public static bool IsSameFolder(string folder, string? other) =>
      other is not null
      && string.Equals(Path.TrimEndingDirectorySeparator(Path.GetFullPath(folder)),
                       Path.TrimEndingDirectorySeparator(Path.GetFullPath(other)),
                       StringComparison.OrdinalIgnoreCase);

  // Where the installer puts a shortcut in a desktop or Start menu folder.
  public static string ShortcutFile(string folder, string productName) =>
      Path.Combine(folder, productName + ShortcutExtension);

  // Unattended with a progress bar (Windows still asks to allow it), and a
  // shortcut the user removed stays removed: the installer would otherwise put
  // both back.
  public static string InstallerArguments(string msi, bool hasDesktopShortcut,
                                          bool hasStartMenuShortcut) {
    var arguments = new StringBuilder($"/i \"{msi}\" /passive /norestart");
    if (!hasDesktopShortcut) {
      arguments.Append($" {DesktopShortcutProperty}=\"\"");
    }
    if (!hasStartMenuShortcut) {
      arguments.Append($" {StartMenuShortcutProperty}=\"\"");
    }
    return arguments.ToString();
  }

  // Waits for the panel to exit, so the installer finds its exe free, then
  // starts the exe again: the new one, or the old one if the installer was
  // declined.
  public static string UpdateScript(int panelProcessId, string installerArguments,
                                    string executable) =>
      $"Wait-Process -Id {panelProcessId} -ErrorAction SilentlyContinue\n"
      + $"Start-Process msiexec.exe -ArgumentList {Quoted(installerArguments)} -Wait\n"
      + $"Start-Process {Quoted(executable)}\n";

  // A hidden PowerShell running the script, encoded so no path in it needs
  // quoting on the command line.
  public static string HiddenPowerShell(string script) =>
      $"{PowerShellCommand} {Convert.ToBase64String(Encoding.Unicode.GetBytes(script))}";

  // A PowerShell single-quoted string, where only a quote needs escaping.
  private static string Quoted(string text) => $"'{text.Replace("'", "''")}'";
}
