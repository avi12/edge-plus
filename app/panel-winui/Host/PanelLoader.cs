using System;
using System.IO;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.Loader;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace EdgePlus;

// Loads the panel (Panel\EdgePlus.Panel.csproj) for the life of the process,
// shows it in a window when there is one, and in a dev run swaps each rebuild
// in live — the panel's version of the split DLL's hot reload: tear the old
// panel down, unload its context, load a fresh timestamped copy, start it again
// from the current settings. The window, the process and the single-instance
// handling stay the host's throughout.
//
// The panel runs without a window too: its background service (the Edge and
// theme watchers, the toast) is started at load, so a boot launch with no
// window is the always-on listener, and a window, when shown, is mounted on the
// same running panel.
//
// The panel lives in a collectible AssemblyLoadContext that resolves nothing
// itself: every WinUI, WinRT and framework reference falls through to the
// host's default context, so WinUI objects cross freely and only the panel's
// own code is unloaded. That holds because the panel carries no compiled XAML
// and subclasses no WinRT type (both register with WinUI for the life of the
// process); it builds its UI in C# from the host's App.xaml styles.
internal sealed class PanelLoader {
  private const string ResourceName = "panel/edgeplus-panel.dll";
  private const string EntryTypeName = "EdgePlus.PanelEntry";
  private const string StartMethod = "StartBackground";
  private const string MountMethod = "Mount";
  private const string ResidentMethod = "IsResident";
  private const string WaiterMethod = "RunDevModeWaiter";
  private const string PanelBuildOutputKey = "PanelBuildOutput";
  private const string RepositoryRootKey = "RepositoryRoot";
  // A build writes the output in several steps; a swap waits this long after
  // the last change before reading it.
  private static readonly TimeSpan s_settleDelay = TimeSpan.FromMilliseconds(800);
  // How often, and how many times, to look for the old context to be gone.
  private static readonly TimeSpan s_unloadPoll = TimeSpan.FromMilliseconds(250);
  private const int UnloadPolls = 20;
  private const string LogName = "swap.log";

  private static readonly string s_copiesDirectory =
      Path.Combine(Path.GetTempPath(), "edgeplus-panel");

  private readonly DispatcherQueue _dispatcher;
  private readonly DispatcherQueueTimer _settle;
  private FileSystemWatcher? _watcher;
  private PanelContext? _context;
  private IDisposable? _service;
  private byte[] _loadedBytes = [];
  // The window the panel is shown in, while there is one.
  private WindowView? _view;
  private IDisposable? _mounted;

  public PanelLoader(DispatcherQueue dispatcher) {
    _dispatcher = dispatcher;
    _settle = dispatcher.CreateTimer();
    _settle.Interval = s_settleDelay;
    _settle.IsRepeating = false;
    _settle.Tick += (_, _) => SwapIfNewBuild();
  }

  // Raised on the UI thread after a swap, with a line for the window to show.
  public event Action<string>? Swapped;

  // The panel's build output this run watches, or null outside a dev run.
  public static string? DevBuildOutput {
    get {
      string? output = Metadata(PanelBuildOutputKey);
      return PanelSource.IsDevRun(AppContext.BaseDirectory, Metadata(RepositoryRootKey), output)
          ? output
          : null;
    }
  }

  // Load the panel and start its background service, and in a dev run start
  // watching its build output.
  public void Start() {
    RemoveOldCopies();
    if (!TryLoad(ReadCurrentBuild())) {
      Swapped?.Invoke($"The panel failed to load; the reason is in {LogName}.");
    }
    if (DevBuildOutput is string output) {
      Watch(output);
    }
  }

  // Show the panel in `window`'s slot.
  public void Show(Window window, ContentControl slot, HostSignal settingChanged,
                   HostSignal broughtBack) {
    _view = new WindowView(window, slot, settingChanged, broughtBack);
    _ = TryMount();
  }

  // The window is closing: take the panel out of it; the panel keeps running.
  public void Hide() {
    _mounted?.Dispose();
    _mounted = null;
    if (_view is not null) {
      _view.Slot.Content = null;
    }
    _view = null;
  }

  // Whether the process stays when its window closes: the panel decides (it
  // does while Edge+ starts with Windows, so the listener keeps listening).
  public bool IsResident() {
    try {
      return _context is not null
             && _context.EntryMethod(ResidentMethod).Invoke(null, null) is true;
    } catch (TargetInvocationException error) {
      Log($"resident check failed: {error.InnerException ?? error}");
      return false;
    }
  }

  // The headless dev-mode waiter, run from the same panel code, never unloaded.
  public static void RunDevModeWaiter() {
    PanelContext context = LoadCopy(ReadCurrentBuild());
    context.EntryMethod(WaiterMethod).Invoke(null, null);
  }

  private void Watch(string output) {
    _watcher = new FileSystemWatcher(Path.GetDirectoryName(output)!, Path.GetFileName(output)) {
      NotifyFilter = NotifyFilters.LastWrite | NotifyFilters.Size | NotifyFilters.FileName,
    };
    // Each event restarts the settle timer, so a build's burst of writes
    // becomes one swap once it has finished.
    void OnChange(object sender, FileSystemEventArgs e) =>
        _dispatcher.TryEnqueue(() => {
          _settle.Stop();
          _settle.Start();
        });
    _watcher.Changed += OnChange;
    _watcher.Created += OnChange;
    _watcher.Renamed += (sender, e) => OnChange(sender, e);
    _watcher.EnableRaisingEvents = true;
  }

  private void SwapIfNewBuild() {
    byte[] candidate;
    try {
      candidate = ReadExclusively(DevBuildOutput!);
    } catch (IOException) {
      // Still being written: look again once it settles.
      _settle.Start();
      return;
    }
    if (!PanelSource.IsNewBuild(_loadedBytes, candidate)) {
      return;
    }
    byte[] previous = _loadedBytes;
    WeakReference oldContext = Unload();
    // A build that fails to load must not take the window with it: the
    // previous panel goes back, and the next build is tried as usual.
    if (!TryLoad(candidate) || !TryMount()) {
      Unload();
      _ = TryLoad(previous);
      _ = TryMount();
      return;
    }
    WatchUnload(oldContext);
  }

  // Load a panel build and start its background service; false (and the
  // reason in the log and the title bar) when it throws.
  private bool TryLoad(byte[] assembly) =>
      TryRun(() => {
        _context = LoadCopy(assembly);
        _loadedBytes = assembly;
        _service = (IDisposable)_context.EntryMethod(StartMethod).Invoke(null, null)!;
      });

  // Mount the loaded panel in the window, when there is one.
  private bool TryMount() {
    if (_view is null || _context is null) {
      return true;
    }
    WindowView view = _view;
    return TryRun(() => _mounted = (IDisposable)_context.EntryMethod(MountMethod).Invoke(
        null, [view.Window, view.Slot, view.SettingChanged, view.BroughtBack])!);
  }

  private bool TryRun(Action action) {
    try {
      action();
      return true;
    } catch (TargetInvocationException error) {
      ReportFailure(error.InnerException ?? error);
      return false;
    } catch (Exception error) when (error is BadImageFormatException or IOException
                                        or InvalidOperationException) {
      ReportFailure(error);
      return false;
    }
  }

  private void ReportFailure(Exception error) {
    Log($"load failed: {error}");
    Swapped?.Invoke($"This panel build failed to load; the reason is in {LogName}.");
  }

  // Tear the panel down (its window view, then its service) and drop every
  // reference into it, then unload its context. Returns a weak reference to
  // watch it go: unloading only begins here, and completes when nothing reaches
  // the context any more.
  [MethodImpl(MethodImplOptions.NoInlining)]
  private WeakReference Unload() {
    _mounted?.Dispose();
    _mounted = null;
    if (_view is not null) {
      _view.Slot.Content = null;
    }
    _service?.Dispose();
    _service = null;
    var weak = new WeakReference(_context);
    _context?.Unload();
    _context = null;
    return weak;
  }

  // Look for the old context to be collected, report the swap, and delete
  // the copies nothing holds any more.
  private void WatchUnload(WeakReference oldContext) {
    int polls = 0;
    DispatcherQueueTimer timer = _dispatcher.CreateTimer();
    timer.Interval = s_unloadPoll;
    timer.Tick += (_, _) => {
      polls++;
      GC.Collect();
      GC.WaitForPendingFinalizers();
      bool isUnloaded = !oldContext.IsAlive;
      bool isDone = isUnloaded || polls >= UnloadPolls;
      if (!isDone) {
        return;
      }
      timer.Stop();
      RemoveOldCopies();
      string line = isUnloaded
          ? $"Panel updated {DateTime.Now:HH:mm:ss}"
          : $"Panel updated {DateTime.Now:HH:mm:ss} (the old copy is still held)";
      Log($"swap: old context {(isUnloaded ? "unloaded" : "still alive")} after {polls} polls");
      Swapped?.Invoke(line);
    };
    timer.Start();
  }

  // The window the panel is shown in, and the host's signals for it.
  private sealed record WindowView(Window Window, ContentControl Slot, HostSignal SettingChanged,
                                   HostSignal BroughtBack);

  // The panel this run should load: the dev build output in a dev run (newer
  // than anything carried), else the copy carried inside the exe.
  private static byte[] ReadCurrentBuild() {
    if (DevBuildOutput is string output) {
      return ReadExclusively(output);
    }
    using Stream stream = typeof(PanelLoader).Assembly.GetManifestResourceStream(ResourceName)
        ?? throw new InvalidOperationException($"The build did not embed {ResourceName}");
    using var bytes = new MemoryStream();
    stream.CopyTo(bytes);
    return bytes.ToArray();
  }

  // The whole file, opened unshared, so a build still writing it is refused
  // rather than read half-written.
  private static byte[] ReadExclusively(string path) {
    using var stream = new FileStream(path, FileMode.Open, FileAccess.Read, FileShare.None);
    using var bytes = new MemoryStream();
    stream.CopyTo(bytes);
    return bytes.ToArray();
  }

  private static PanelContext LoadCopy(byte[] assembly) {
    Directory.CreateDirectory(s_copiesDirectory);
    string copy = Path.Combine(s_copiesDirectory, PanelSource.CopyName(DateTime.UtcNow));
    File.WriteAllBytes(copy, assembly);
    CopySymbols(copy);
    var context = new PanelContext();
    context.LoadFromAssemblyPath(copy);
    return context;
  }

  // The dev build's .pdb beside the copy, so a crash log still has lines.
  private static void CopySymbols(string copy) {
    if (DevBuildOutput is not string output) {
      return;
    }
    string symbols = Path.ChangeExtension(output, PanelSource.SymbolsExtension);
    if (File.Exists(symbols)) {
      File.Copy(symbols, Path.ChangeExtension(copy, PanelSource.SymbolsExtension), overwrite: true);
    }
  }

  // Copies no context holds any more; one still loaded is locked and stays.
  private static void RemoveOldCopies() {
    if (!Directory.Exists(s_copiesDirectory)) {
      return;
    }
    foreach (string file in Directory.EnumerateFiles(s_copiesDirectory)) {
      if (!PanelSource.IsCopy(Path.GetFileName(file))) {
        continue;
      }
      try {
        File.Delete(file);
      } catch (IOException) {
      } catch (UnauthorizedAccessException) {
      }
    }
  }

  private static void Log(string line) {
    try {
      Directory.CreateDirectory(s_copiesDirectory);
      File.AppendAllText(Path.Combine(s_copiesDirectory, LogName),
                         $"{DateTime.Now:O} {line}{Environment.NewLine}");
    } catch (IOException) {
    }
  }

  private static string? Metadata(string key) {
    foreach (AssemblyMetadataAttribute attribute in
             typeof(PanelLoader).Assembly.GetCustomAttributes<AssemblyMetadataAttribute>()) {
      if (attribute.Key == key) {
        return attribute.Value;
      }
    }
    return null;
  }

  // A context of the panel assembly alone: every other reference resolves in
  // the default context (Load returns null), which holds WinUI and the host.
  private sealed class PanelContext() : AssemblyLoadContext("Edge+ panel", isCollectible: true) {
    public MethodInfo EntryMethod(string name) {
      foreach (Assembly assembly in Assemblies) {
        Type? entry = assembly.GetType(EntryTypeName);
        if (entry?.GetMethod(name) is MethodInfo method) {
          return method;
        }
      }
      throw new InvalidOperationException($"The panel has no {EntryTypeName}.{name}");
    }

    protected override Assembly? Load(AssemblyName assemblyName) => null;
  }
}
