using System;
using System.Threading;

namespace EdgePlus;

// A thread still running code of this assembly would keep the host from
// unloading it on a live swap, so every thread here can be stopped.
internal sealed class PollThread(string name, TimeSpan interval) {
  // An injection in progress can take seconds.
  private static readonly TimeSpan s_stopWait = TimeSpan.FromSeconds(15);

  private readonly CancellationTokenSource _stop = new();
  private Thread? _thread;

  public void Start(Action first, Action poll) {
    _thread = new Thread(() => {
      first();
      while (!_stop.Token.WaitHandle.WaitOne(interval)) {
        poll();
      }
    }) {
      IsBackground = true,
      Name = name
    };
    _thread.Start();
  }

  public void Stop() {
    _stop.Cancel();
    _thread?.Join(s_stopWait);
  }
}
