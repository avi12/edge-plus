import { VIEW_STATE_NAME, viewAsSnapshot } from "./core-link.ts";
import { Panel } from "./panel.svelte.ts";
import type { Action, Bounds, EdgePlusProtocolMap, View } from "#src/protocol.g.ts";
import { CoreMessageType, UiMessageType } from "#src/protocol.g.ts";
import Shell from "#src/window/Shell.svelte";
import type { MessagePort } from "node:worker_threads";
import { mountControl } from "winui-svelte";
import { WindowTheme } from "winui-svelte/enums";
import { createHostChannel, createMessagePortTransport, hostState } from "winui-svelte/state";
import { exitApplication } from "winui-svelte/window";

const BACKGROUND_VARIABLE = "EDGEPLUS_WINDOW_BACKGROUND";
const THEME_VARIABLE = "EDGEPLUS_WINDOW_THEME";
const THEME = Object.values(WindowTheme).find(theme => theme === process.env[THEME_VARIABLE]) ?? WindowTheme.system;

type SessionProtocol = Omit<EdgePlusProtocolMap, typeof CoreMessageType.view | typeof UiMessageType.action>;

/**
 * The window's end of the core's protocol: the View is host state, an action
 * is `channel.send`, and the core's other requests are handlers on the
 * channel's messaging. Call it on the UI thread, from `runWindow`'s onStart.
 */
export function startSession(port: MessagePort) {
  const transport = viewAsSnapshot(createMessagePortTransport({ port }));
  const channel = createHostChannel<Action, SessionProtocol>({ transport });
  const view = hostState<View>({
    channel,
    name: VIEW_STATE_NAME
  });
  const panel = new Panel(async action => {
    try {
      await channel.send(action);
      return null;
    } catch (error) {
      return error instanceof Error ? error.message : String(error);
    }
  });
  let placement: Bounds | null = null;

  function tell(delivery: Promise<void>) {
    delivery.catch(() => {});
  }

  const { messaging } = channel;
  messaging.onMessage(CoreMessageType.place, message => {
    placement = message.data;
  });
  messaging.onMessage(CoreMessageType.notice, message => {
    panel.actionNotice = message.data;
  });
  messaging.onMessage(CoreMessageType.status, message => {
    panel.showStatus(message.data);
  });
  messaging.onMessage(CoreMessageType.bringBack, () => {
    panel.window?.bringToFront();
  });
  messaging.onMessage(CoreMessageType.close, () => {
    const openWindow = panel.window;
    const isWindowOpen = openWindow !== undefined;
    if (!isWindowOpen) {
      exitApplication();
      return;
    }

    openWindow.close();
  });
  void view.ready.then(ready => {
    mountControl({
      component: Shell,
      props: {
        view: ready,
        panel,
        placement,
        isBackground: process.env[BACKGROUND_VARIABLE] === "1",
        theme: THEME,
        refresh: () => tell(messaging.sendMessage(UiMessageType.refresh)),
        reportBounds: (bounds: Bounds) => tell(messaging.sendMessage(UiMessageType.bounds, bounds)),
        onclosed: exitApplication
      }
    });
  });
}
