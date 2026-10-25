//! A watcher's thread: runs `first` once, then `poll` every interval until it
//! is stopped.

use std::sync::mpsc::{self, RecvTimeoutError, Sender};
use std::thread::{self, JoinHandle};
use std::time::Duration;

pub struct PollThread {
    stop: Sender<()>,
    thread: Option<JoinHandle<()>>,
}

impl PollThread {
    pub fn start(
        name: &str,
        interval: Duration,
        first: impl FnOnce() + Send + 'static,
        poll: impl Fn() + Send + 'static,
    ) -> Self {
        let (stop, stopped) = mpsc::channel::<()>();
        let thread = thread::Builder::new()
            .name(name.to_owned())
            .spawn(move || {
                first();
                while let Err(RecvTimeoutError::Timeout) = stopped.recv_timeout(interval) {
                    poll();
                }
            })
            .ok();
        Self { stop, thread }
    }

    /// Waits for a poll in progress (an injection can take seconds).
    pub fn stop(&mut self) {
        let _ = self.stop.send(());
        if let Some(thread) = self.thread.take() {
            let _ = thread.join();
        }
    }
}

impl Drop for PollThread {
    fn drop(&mut self) {
        self.stop();
    }
}
