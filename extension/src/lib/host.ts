// The bridge to the Fix Edge app: its native host relays the switches the moment the app saves
// them, and the files of a newer build the moment the app unpacks them, so Edge never restarts.

import { HOST_NAME } from "@/features/catalog";
import { z } from "@/lib/zod";
import type { Browser } from "wxt/browser";

/** The strings are the wire contract with the app - `app/host/NativeHost.cs` names them too */
enum HostMessageType {
  settings = "settings",
  reload = "reload"
}

enum ExtensionMessageType {
  hello = "hello",
  status = "status"
}

const RECONNECT_MS = 5000;

export const settingsSchema = z.object({
  features: z.record(z.string(), z.object({ enabled: z.boolean().optional() })).optional()
});

export type Settings = z.infer<typeof settingsSchema>;

const hostMessageSchema = z.discriminatedUnion("type", [
  z.object({ type: z.literal(HostMessageType.settings), settings: settingsSchema }),
  z.object({ type: z.literal(HostMessageType.reload) })
]);

let port: Browser.runtime.Port | null = null;

export function connectHost({ onSettings }: { onSettings: (settings: Settings) => Promise<void> }) {
  port = browser.runtime.connectNative(HOST_NAME);
  port.onMessage.addListener(async message => {
    const parsed = hostMessageSchema.safeParse(message);
    if (!parsed.success) {
      return;
    }
    switch (parsed.data.type) {
      case HostMessageType.reload:
        browser.runtime.reload();
        return;
      case HostMessageType.settings:
        await onSettings(parsed.data.settings);
    }
  });
  port.onDisconnect.addListener(() => {
    // Read so Edge does not log it as unchecked: the app is not installed or not registered yet,
    // and the last switches it sent keep working
    void browser.runtime.lastError;
    port = null;
    setTimeout(() => connectHost({ onSettings }), RECONNECT_MS);
  });
  port.postMessage({ type: ExtensionMessageType.hello, version: browser.runtime.getManifest().version });
}

export function reportToHost(features: Record<string, Record<string, number>>) {
  port?.postMessage({ type: ExtensionMessageType.status, features });
}
