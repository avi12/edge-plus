using System;
using System.IO;
using System.Net.Http;
using System.Threading;
using System.Threading.Tasks;

namespace EdgePlus;

// A newer release whose exe is downloaded and checked, waiting for the user to
// restart into it.
internal sealed record ReadyUpdate(UpdateOffer Offer, string File);

// Asks the repository's update manifest (UpdateFeed) for a newer release at
// start and then every few hours, and downloads its exe in the background so
// the window can offer a restart at once. Only a published exe replaces itself:
// a build run from a checkout is never touched. Off (Settings.AutoUpdate),
// Edge+ never contacts GitHub.
internal sealed class UpdateWatch {
  // How often the loop wakes. It only goes to the network every s_checkEvery,
  // but a request for a check (the switch turned on) is seen within a tick.
  private static readonly TimeSpan s_tick = TimeSpan.FromMinutes(1);
  private static readonly TimeSpan s_checkEvery = TimeSpan.FromHours(6);
  // After a failed check (no network at sign-in, GitHub down).
  private static readonly TimeSpan s_retryAfter = TimeSpan.FromMinutes(30);
  private static readonly TimeSpan s_requestTimeout = TimeSpan.FromMinutes(10);
  // The host's own assembly sits beside a build from a checkout, never beside a
  // published single-file exe.
  private const string HostAssemblyFileName = "edgeplus.dll";
  private const string DownloadSuffix = ".part";

  private readonly PollThread _thread = new("Update watch", s_tick);
  private readonly CancellationTokenSource _cancel = new();
  private DateTime _nextCheckUtc = DateTime.MinValue;
  private volatile bool _isCheckRequested;
  private volatile ReadyUpdate? _pending;

  // Raised on the watcher's thread.
  public event Action<ReadyUpdate>? Ready;

  public ReadyUpdate? Pending => _pending;

  public void Start() => _thread.Start(Poll, Poll);

  public void Stop() {
    _cancel.Cancel();
    _thread.Stop();
    _cancel.Dispose();
  }

  public void CheckSoon() => _isCheckRequested = true;

  private void Poll() {
    bool isDue = _isCheckRequested || DateTime.UtcNow >= _nextCheckUtc;
    if (!isDue) {
      return;
    }
    _isCheckRequested = false;
    if (!IsWanted()) {
      _nextCheckUtc = DateTime.UtcNow + s_checkEvery;
      return;
    }
    try {
      Look();
      _nextCheckUtc = DateTime.UtcNow + s_checkEvery;
    } catch (Exception error) when (error is HttpRequestException or IOException
                                        or UnauthorizedAccessException or InvalidDataException
                                        or OperationCanceledException) {
      PanelLog.Write("update check failed", error);
      _nextCheckUtc = DateTime.UtcNow + s_retryAfter;
    }
  }

  private static bool IsWanted() => Settings.Load().AutoUpdate && IsPublishedExe();

  private static bool IsPublishedExe() {
    string? folder = Path.GetDirectoryName(Environment.ProcessPath);
    return folder is not null && !File.Exists(Path.Combine(folder, HostAssemblyFileName));
  }

  private void Look() {
    if (!Version.TryParse(Product.Version, out Version? current)) {
      return;
    }
    using var client = new HttpClient { Timeout = s_requestTimeout };
    client.DefaultRequestHeaders.UserAgent.ParseAdd($"EdgePlus/{Product.Version}");
    string manifest = client.GetStringAsync(UpdateFeed.Url, _cancel.Token).GetAwaiter().GetResult();
    UpdateOffer? offer = UpdateFeed.Parse(manifest, current);
    bool isNew = offer is not null && (_pending is null || _pending.Offer.Version < offer.Version);
    if (!isNew) {
      return;
    }
    string file = Download(client, offer!);
    _pending = new ReadyUpdate(offer!, file);
    Ready?.Invoke(_pending);
  }

  // Checked against the manifest's digest before it is kept; a download from an
  // earlier run that still matches is used as it is.
  private string Download(HttpClient client, UpdateOffer offer) {
    Directory.CreateDirectory(Paths.UpdateFolder);
    string target = Path.Combine(Paths.UpdateFolder, $"EdgePlus-{offer.Version}.exe");
    if (File.Exists(target) && UpdateInstall.IsDigestOf(target, offer.Sha256)) {
      return target;
    }
    string partial = target + DownloadSuffix;
    Task<HttpResponseMessage> request = client.GetAsync(offer.Download,
                                                        HttpCompletionOption.ResponseHeadersRead,
                                                        _cancel.Token);
    using (HttpResponseMessage response = request.GetAwaiter().GetResult()) {
      response.EnsureSuccessStatusCode();
      using Stream source = response.Content.ReadAsStream(_cancel.Token);
      using FileStream destination = File.Create(partial);
      source.CopyToAsync(destination, _cancel.Token).GetAwaiter().GetResult();
    }
    if (!UpdateInstall.IsDigestOf(partial, offer.Sha256)) {
      File.Delete(partial);
      throw new InvalidDataException("The downloaded update does not match its digest.");
    }
    File.Move(partial, target, overwrite: true);
    DeleteOtherDownloads(target);
    return target;
  }

  private static void DeleteOtherDownloads(string keep) {
    foreach (string file in Directory.EnumerateFiles(Paths.UpdateFolder)) {
      bool isOther = !string.Equals(file, keep, StringComparison.OrdinalIgnoreCase);
      if (isOther) {
        try {
          File.Delete(file);
        } catch (IOException) {
        } catch (UnauthorizedAccessException) {
        }
      }
    }
  }
}
