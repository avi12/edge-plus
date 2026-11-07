import { connect } from "node:net";

// What a dev run's core starts as "the window" (EDGEPLUS_WINDOW_COMMAND): it
// carries the core's stdin and stdout, JSON lines untouched, to the named pipe
// of the dev server (dev-server.ts), which keeps the real window up. The core
// leaving closes this process's stdin; the pipe then closes and the window
// stays.
//
//   node relay.ts <pipe name>

const pipeName = process.argv[2];
if (pipeName === undefined) {
  process.stderr.write("relay: no pipe name given\n");
  process.exit(2);
}

const socket = connect(pipeName);
process.stdin.pipe(socket);
socket.pipe(process.stdout);
socket.on("error", error => {
  process.stderr.write(`relay: ${error.message}\n`);
  process.exit(1);
});
// Letting stdin go ends the process once stdout has drained.
socket.on("close", () => process.stdin.destroy());
