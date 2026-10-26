# Scheduling experiment

Question: does a `$state` change made inside a WinUI callback reach the
controls without the app calling `flushSync`?

Answer: yes, when the WinUI loop is started with `Application.startScheduled`
(as Edge+'s `session.ts` does). WinUI then runs from a libuv callback with no
JavaScript under it, so each time WinUI calls back into JS (an event, a
`DispatcherQueue` timer tick) Node drains the microtask queue when that callback
returns. Svelte schedules its flush with `queueMicrotask`, so the change is on
the control before the next callback runs. Verified for a control event
(button click through UI Automation), a dispatcher timer tick, and a host
message taken off a `MessagePort` by a timer.

Run: `pwsh run.ps1` (background window, off screen, never activated; ends only
the process it starts). It prints the log and fails if any trigger was not
applied by the microtask.

Not covered: a loop started from JavaScript (not scheduled) keeps that script
on the stack for good, and no microtask would ever run; the package's window
start must therefore always use `startScheduled`.
