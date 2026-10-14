import type { PaneReport } from "@/lib/pane-bridge";
import { PaneCommand, paneCommandSchema, PaneMessageType } from "@/lib/pane-bridge";
import { defineContentScript } from "#imports";

/** A single-page app changes its address without a navigation event a content script can hear */
const ADDRESS_POLL_MS = 500;

export default defineContentScript({
  matches: ["<all_urls>"],
  allFrames: true,
  runAt: "document_start",
  main() {
    const extensionOrigin = new URL(browser.runtime.getURL("/")).origin;
    // Only a page sitting directly in a split page's pane talks to it; every other frame on the
    // web is left alone
    const isPaneOfSplitPage = window.parent !== window && window.parent === window.top &&
      location.ancestorOrigins?.[0] === extensionOrigin;
    if (!isPaneOfSplitPage) {
      return;
    }

    let lastReport = "";
    function report() {
      const message: PaneReport = { type: PaneMessageType.report, url: location.href, title: document.title };
      const serialized = JSON.stringify(message);
      const isAlreadyReported = serialized === lastReport;
      if (isAlreadyReported) {
        return;
      }
      lastReport = serialized;
      window.parent.postMessage(message, extensionOrigin);
    }

    report();
    document.addEventListener("DOMContentLoaded", report);
    addEventListener("load", report);
    setInterval(report, ADDRESS_POLL_MS);

    addEventListener("message", e => {
      const isFromSplitPage = e.source === window.parent && e.origin === extensionOrigin;
      if (!isFromSplitPage) {
        return;
      }
      const parsed = paneCommandSchema.safeParse(e.data);
      if (!parsed.success) {
        return;
      }
      switch (parsed.data.command) {
        case PaneCommand.back:
          history.back();
          return;
        case PaneCommand.forward:
          history.forward();
          return;
        case PaneCommand.reload:
          location.reload();
      }
    });
  }
});
