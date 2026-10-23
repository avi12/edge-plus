using System;
using System.Collections.Generic;

namespace EdgePlus;

// An IObservable because the panel, in its own load context, may share only
// framework types with the host. The panel disposes each subscription at
// teardown, so the host keeps no reference into an unloaded panel.
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
