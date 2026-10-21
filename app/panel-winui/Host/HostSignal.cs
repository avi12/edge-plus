using System;
using System.Collections.Generic;

namespace EdgePlus;

// Something the host tells the panel (a Windows setting changed; the window
// was brought back). An IObservable, because the panel is loaded in its own
// context and may share only framework types with the host; each
// subscription is disposed when the panel is torn down, so the host keeps no
// reference into an unloaded panel.
internal sealed class HostSignal : IObservable<EventArgs> {
  private readonly List<IObserver<EventArgs>> _observers = [];

  public IDisposable Subscribe(IObserver<EventArgs> observer) {
    _observers.Add(observer);
    return new Subscription(() => _observers.Remove(observer));
  }

  public void Raise() {
    foreach (IObserver<EventArgs> observer in _observers.ToArray()) {
      observer.OnNext(EventArgs.Empty);
    }
  }

  private sealed class Subscription(Action unsubscribe) : IDisposable {
    public void Dispose() => unsubscribe();
  }
}
