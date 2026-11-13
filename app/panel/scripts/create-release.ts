import { copyFileSync, mkdirSync, readFileSync, rmSync } from "node:fs";
import path from "node:path";

import { createRelease, feedFileName } from "winui-svelte/updater";

// Packs the release the installed copies update from: the Squirrel-style zip
// and releases.json winui-svelte's createRelease writes, plus the exe a user
// downloads to install (the same exe, which installs itself when run outside
// the install), in out/release for .github/workflows/release.yml to attach.
//
//   pnpm create-release
//
// EDGEPLUS_CHANNEL names a channel (beta writes releases.beta.json, the file
// feedFileName gives it); without it the feed is releases.json, the one an
// installed copy reads.

const ROOT = path.join(import.meta.dirname, "..");
const CORE = path.join(ROOT, "src-tauri", "target", "release");
const OUTPUT = path.join(ROOT, "..", "..", "out", "release");
const APP = path.join(ROOT, "dist-app");
const PRODUCT_NAME = "EdgePlus";
const SETUP_NAME = `${PRODUCT_NAME}-Setup.exe`;
const APP_FILES = ["edgeplus.exe", "edgeplus-launcher.exe"];
const CHANNEL = process.env["EDGEPLUS_CHANNEL"];
const VERSION_LINE = /^version\s*=\s*"([^"]+)"/m;

function productVersion() {
  const manifest = readFileSync(path.join(ROOT, "src-tauri", "Cargo.toml"), "utf8");
  const version = VERSION_LINE.exec(manifest)?.[1];
  if (version === undefined) {
    throw new Error("Cargo.toml names no version");
  }

  return version;
}

rmSync(APP, {
  recursive: true,
  force: true
});
mkdirSync(APP, { recursive: true });
mkdirSync(OUTPUT, { recursive: true });
for (const name of APP_FILES) {
  copyFileSync(path.join(CORE, name), path.join(APP, name));
}
copyFileSync(path.join(CORE, "edgeplus.exe"), path.join(OUTPUT, SETUP_NAME));

const release = await createRelease({
  appDirectory: APP,
  name: PRODUCT_NAME,
  version: productVersion(),
  outputDirectory: OUTPUT,
  ...(CHANNEL !== undefined && {
    channel: CHANNEL
  })
});
process.stdout.write(
  `${release.file} (${release.size} bytes, sha256 ${release.sha256}), feed ${feedFileName(CHANNEL)}\n`
);
