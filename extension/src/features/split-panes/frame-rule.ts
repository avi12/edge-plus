// Most sites refuse to be shown inside another page, with X-Frame-Options or a CSP frame-ancestors
// directive. A pane is exactly that, so for frames inside split tabs - and nowhere else - the
// refusal is lifted. Neither header can be edited in part, so the frame's CSP goes with it; every
// other tab keeps every header.

import { isSplitPageUrl } from "./split-state";

const FRAME_RULE_ID = 1;

/** Recomputes the rule from the tabs open now; `tabId` is one about to become a split */
export async function allowFramesInSplitTabs(tabId?: number) {
  const tabs = await browser.tabs.query({});
  const tabIds = tabs.filter(tab => isSplitPageUrl(tab.url)).map(tab => tab.id!);
  const isAboutToSplit = tabId !== undefined && !tabIds.includes(tabId);
  if (isAboutToSplit) {
    tabIds.push(tabId);
  }
  const hasSplitTabs = tabIds.length > 0;
  await browser.declarativeNetRequest.updateSessionRules({
    removeRuleIds: [FRAME_RULE_ID],
    addRules: hasSplitTabs
      ? [{
        id: FRAME_RULE_ID,
        priority: 1,
        action: {
          type: "modifyHeaders",
          responseHeaders: [
            { header: "x-frame-options", operation: "remove" },
            { header: "content-security-policy", operation: "remove" }
          ]
        },
        condition: { tabIds, resourceTypes: ["sub_frame"] }
      }]
      : []
  });
}

export const stopAllowingFrames = () =>
  browser.declarativeNetRequest.updateSessionRules({ removeRuleIds: [FRAME_RULE_ID] });
