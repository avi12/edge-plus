// A split tab's whole state - its pane tree and which pane is focused - lives in the tab's own
// address, after the #. That makes the address the one source of truth: the split page renders
// from it, the background edits it by updating the tab's URL (a same-document change, so no pane
// reloads), and Edge restores it with the tab after a restart.

import type { Node, Pane } from "./layout";
import { Axis, panes } from "./layout";
import { z } from "@/lib/zod";

export const SPLIT_PAGE = "/split.html";
export const PANE_START_PAGE = "/pane-start.html";

/** Unique within one tab is enough, and every character lengthens the tab's address */
const PANE_ID_LENGTH = 8;

const paneSchema = z.object({ id: z.string(), url: z.string() });
const nodeSchema: z.ZodType<Node> = z.lazy(() => z.union([
  paneSchema,
  z.object({ axis: z.enum(Axis), children: z.array(nodeSchema).min(2), weights: z.array(z.number().positive()) })
]));
const stateSchema = z.object({ root: nodeSchema, activePaneId: z.string() });

export type SplitState = z.infer<typeof stateSchema>;

export const newPane = (url = ""): Pane => ({ id: crypto.randomUUID().slice(0, PANE_ID_LENGTH), url });

const toBase64Url = (text: string) =>
  btoa(String.fromCharCode(...new TextEncoder().encode(text))).replaceAll("+", "-").replaceAll("/", "_").replace(/=+$/, "");

function fromBase64Url(text: string) {
  const binary = atob(text.replaceAll("-", "+").replaceAll("_", "/"));
  return new TextDecoder().decode(Uint8Array.from(binary, character => character.charCodeAt(0)));
}

export const encodeState = (state: SplitState) => `#${toBase64Url(JSON.stringify(state))}`;

export function decodeState(hash: string) {
  try {
    const parsed = stateSchema.safeParse(JSON.parse(fromBase64Url(hash.replace(/^#/, ""))));
    return parsed.success ? parsed.data : null;
  } catch {
    return null;
  }
}

export const splitPageUrl = (state: SplitState) => browser.runtime.getURL(SPLIT_PAGE) + encodeState(state);

export const isSplitPageUrl = (url = "") => url.startsWith(browser.runtime.getURL(SPLIT_PAGE));

export function stateOfTab(url = "") {
  if (!isSplitPageUrl(url)) {
    return null;
  }
  return decodeState(new URL(url).hash);
}

/** The pane the start page stands in for has no address yet, so it has nothing to give back */
export const addressesOf = (state: SplitState) => panes(state.root).map(pane => pane.url).filter(Boolean);
