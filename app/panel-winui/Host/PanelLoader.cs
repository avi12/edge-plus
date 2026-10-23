using System;
using System.IO;
using System.Reflection;
using System.Runtime.CompilerServices;
using System.Runtime.Loader;
using Microsoft.UI.Dispatching;
using Microsoft.UI.Xaml;
using Microsoft.UI.Xaml.Controls;

namespace EdgePlus;

// Loads the panel for the life of the process and, in a dev run, swaps each
// rebuild in live (the panel's version of the split DLL's hot reload). Its
// background service starts at load, so a windowless boot launch is the
// always-on listener and a window is mounted on the same running panel.
//
// The panel's collectible context resolves nothing itself: WinUI, WinRT and the
// framework fall through to the default context, so only the panel's own code
// unloads. That holds only while the panel has no compiled XAML and subclasses
// no WinRT type (both register with WinUI for the life of the process).
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
  private WindowView? _view;
  private IDisposable? _mounted;

  public PanelLoader(DispatcherQueue dispatcher) {
    _dispatcher = dispatcher;
    _settle = dispatcher.CreateTimer();
    _settle.Interval = s_settleDelay;
    _settle.IsRepeating = false;
    _settle.Tick += (_, _) => SwapIfNewBuild();
  }

  // Raised on the UI thread, with a line for the window to show.
  public event Action<string>? Swapped;

  // Null outside a dev run.
  public static string? DevBuildOutput {
    get {
      string? output = Metadata(PanelBuildOutputKey);
      return PanelSource.IsDevRun(AppContext.BaseDirectory, Metadata(RepositoryRootKey), output)
          ? output
          : null;
    }
  }

  public void Start() {
    RemoveOldCopies();
    if (!TryLoad(ReadCurrentBuild())) {
      Swapped?.Invoke($"The panel failed to load; the reason is in {LogName}.");
    }
    if (DevBuildOutput is string output) {
      Watch(output);
    }
  }

  public void Show(Window window, ContentControl slot, HostSignal settingChanged,
                   HostSignal broughtBack) {
    _view = new WindowView(window, slot, settingChanged, broughtBack);
    _ = TryMount();
  }

  // The panel keeps running without its window.
  public void Hide() {
    _mounted?.Dispose();
    _mounted = null;
    if (_view is not null) {
      _view.Slot.Content = null;
    }
    _view = null;
  }

  // Whether the process outlives its window: the panel decides (it does while
  // Edge+ starts with Windows, so the listener keeps listening).
  public bool IsResident() {
    try {
      return _context is not null
             && _context.EntryMethod(ResidentMethod).Invoke(null, null) is true;
    } catch (TargetInvocationException error) {
      Log($"resident check failed: {error.InnerException ?? error}");
      return false;
    }
  }

  // Never unloaded.
  public static void RunDevModeWaiter() {
    PanelContext context = LoadCopy(ReadCurrentBuild());
    context.EntryMethod(WaiterMethod).Invoke(null, null);
  }

  private void Watch(string output) {
    _watcher = new FileSystemWatcher(Path.GetDirectoryName(output)!, Path.GetFileName(output)) {
      NotifyFilter = NotifyFilters.LastWrite | NotifyFilters.Size | NotifyFilters.FileName,
    };
    // Restarting the timer on each event turns a build's burst of writes into one swap.
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
      // Still being written.
      _settle.Start();
      return;
    }
    if (!PanelSource.IsNewBuild(_loadedBytes, candidate)) {
      return;
    }
    byte[] previous = _loadedBytes;
    WeakReference oldContext = Unload();
    // A build that fails to load must not take the window with it.
    if (!TryLoad(candidate) || !TryMount()) {
      Unload();
      _ = TryLoad(previous);
      _ = TryMount();
      return;
    }
    WatchUnload(oldContext);
  }

  private bool TryLoad(byte[] assembly) =>
      TryRun(() => {
        _context = LoadCopy(assembly);
        _loadedBytes = assembly;
        _service = (IDisposable)_context.EntryMethod(StartMethod).Invoke(null, null)!;
      });

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

  // Unloading only begins here and completes once nothing reaches the context,
  // hence the weak reference. NoInlining keeps this frame's locals from pinning it.
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

  private sealed record WindowView(Window Window, ContentControl Slot, HostSignal SettingChanged,
                                   HostSignal BroughtBack);

  // A dev run's build output is newer than the copy embedded in the exe.
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

  // Unshared, so a build still writing it is refused rather than read half-written.
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

  // So a crash log still has line numbers.
  private static void CopySymbols(string copy) {
    if (DevBuildOutput is not string output) {
      return;
    }
    string symbols = Path.ChangeExtension(output, PanelSource.SymbolsExtension);
    if (File.Exists(symbols)) {
      File.Copy(symbols, Path.ChangeExtension(copy, PanelSource.SymbolsExtension), overwrite: true);
    }
  }

  // A copy still loaded is locked, so its delete fails and it stays.
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

  // Load returns null, so every other reference resolves in the default context.
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
