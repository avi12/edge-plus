using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading;
using System.Threading.Tasks;
using Windows.Services.Store;

namespace EdgePlus;

// Installs Store updates at a moment Edge+ picks rather than Windows: the
// install ends this process, so it waits until no window is open. A mandatory
// update (set on the Partner Center submission, which Windows itself ignores)
// does not wait: it is how a release for a new Edge build is marked, and every
// feature stays paused until it lands. The plain exe has no Store to ask.
internal sealed class StoreUpdates : IDisposable {
  private static readonly TimeSpan s_firstCheckDelay = TimeSpan.FromMinutes(3);
  private static readonly TimeSpan s_checkInterval = TimeSpan.FromHours(6);

  private readonly Func<bool> _isWindowOpen;
  private readonly Timer? _timer;
  private readonly StoreContext? _store;
  private int _isBusy;

  public StoreUpdates(Func<bool> isWindowOpen) {
    _isWindowOpen = isWindowOpen;
    if (!Packaging.IsPackaged) {
      return;
    }
    _store = StoreContext.GetDefault();
    _timer = new Timer(_ => CheckNow(), null, s_firstCheckDelay, s_checkInterval);
  }

  public void CheckNow() {
    if (_store is null) {
      return;
    }
    bool isAlreadyChecking = Interlocked.Exchange(ref _isBusy, 1) == 1;
    if (isAlreadyChecking) {
      return;
    }
    Task.Run(async () => {
      try {
        await CheckAndInstallAsync(_store);
      } catch (Exception error) {
        PanelLog.Write("store update check failed", error);
      } finally {
        Interlocked.Exchange(ref _isBusy, 0);
      }
    });
  }

  public void Dispose() => _timer?.Dispose();

  private async Task CheckAndInstallAsync(StoreContext store) {
    IReadOnlyList<StorePackageUpdate> updates =
        await store.GetAppAndOptionalStorePackageUpdatesAsync();
    if (updates.Count == 0) {
      return;
    }
    // Without consent to download silently ("update apps automatically" off,
    // or a metered connection) the Store installs it on its own schedule.
    if (!store.CanSilentlyDownloadStorePackageUpdates) {
      return;
    }
    StorePackageUpdateResult download =
        await store.TrySilentDownloadStorePackageUpdatesAsync(updates);
    if (download.OverallState != StorePackageUpdateState.Completed) {
      return;
    }
    bool isMandatory = updates.Any(update => update.Mandatory);
    bool isGoodTimeToInstall = isMandatory || !_isWindowOpen();
    if (!isGoodTimeToInstall) {
      return;
    }
    PanelLog.Write($"installing a store update (mandatory: {isMandatory})");
    // This overload installs what is already staged; the process usually ends
    // inside it, and the host's restart registration brings it back.
    await store.TrySilentDownloadAndInstallStorePackageUpdatesAsync(updates);
  }
}
