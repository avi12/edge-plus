import { Application } from "../winui.ts";

export interface RunWindowOptions {
  // Runs on the UI thread once WinUI is up (its own resources installed): the
  // place to register controls and open windows.
  onStart: () => void;
}

// Starts WinUI on this thread, which must be the UI thread (thread.ts); the
// promise settles when the application exits.
//
// Scheduled rather than started in place: WinUI's loop then runs from a libuv
// callback with no JavaScript under it, so each event handler drains the
// promise continuations it queues. Started from JavaScript, the loop keeps
// that script on the stack for good, and no `await` ever resumes.
export async function runWindow(options: RunWindowOptions): Promise<void> {
  await Application.startScheduled(() => {
    Application.create(options.onStart);
  });
}

export function exitApplication(): void {
  Application.current.exit();
}
