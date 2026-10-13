// What a split page and the pages in its panes say to each other with postMessage. A pane's page
// is another site, so the split page cannot read its address or drive its history directly; the
// pane-bridge content script, running inside the pane, does both on its behalf.

import { z } from "@/lib/zod";

/** The strings are the wire contract between the split page and the content script */
export enum PaneMessageType {
  report = "fix-edge:pane-report",
  command = "fix-edge:pane-command"
}

export enum PaneCommand {
  back = "back",
  forward = "forward",
  reload = "reload"
}

export const paneReportSchema = z.object({
  type: z.literal(PaneMessageType.report),
  url: z.string(),
  title: z.string()
});

export const paneCommandSchema = z.object({
  type: z.literal(PaneMessageType.command),
  command: z.enum(PaneCommand)
});

export type PaneReport = z.infer<typeof paneReportSchema>;
export type PaneCommandMessage = z.infer<typeof paneCommandSchema>;
