<script lang="ts">
  import { isSplitPageUrl } from "@/features/split-panes/split-state";
  import { addressFrom, faviconOf, isFrameable } from "@/lib/address";
  import type { Browser } from "wxt/browser";

  const loading = (async () => {
    const tab = await browser.tabs.getCurrent();
    const tabs = await browser.tabs.query({ windowId: tab?.windowId });
    return tabs.filter(other => isFrameable(other.url) && !isSplitPageUrl(other.url));
  })();

  /** Like Edge's own split: the picked tab moves into this pane rather than being copied */
  async function take(tab: Browser.tabs.Tab) {
    await browser.tabs.remove(tab.id!);
    location.href = tab.url!;
  }
</script>

<main>
  <form onsubmit={e => {
    e.preventDefault();
    const elInput = (e.currentTarget as HTMLFormElement).elements.namedItem("address") as HTMLInputElement;
    location.href = addressFrom(elInput.value);
  }}>
    <label for="address">Open in this pane</label>
    <input id="address" name="address" autocomplete="off" placeholder="Search or enter an address" spellcheck="false" />
  </form>

  {#await loading then tabs}
    {@const hasMovableTabs = tabs.length > 0}
    {#if hasMovableTabs}
      <section>
        <h2>Or move a tab here</h2>
        <ul>
          {#each tabs as tab (tab.id)}
            <li>
              <button onclick={() => take(tab)}>
                <img alt="" src={faviconOf(tab.url!)} />
                <span class="title">{tab.title}</span>
                <span class="host">{new URL(tab.url!).host}</span>
              </button>
            </li>
          {/each}
        </ul>
      </section>
    {/if}
  {/await}
</main>
