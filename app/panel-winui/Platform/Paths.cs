using System;
using System.IO;

namespace EdgePlus;

// The files the panel reads and writes, in the one place that names them.
internal static class Paths {
  // What was built INTO the app: the icon, the managed and WinUI runtime.
  public static string ContentDir => AppContext.BaseDirectory;

  public static string WindowIcon => Path.Combine(ContentDir, "Assets", "edgeplus.ico");
}
