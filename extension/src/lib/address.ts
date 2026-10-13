/** Edge's own default engine, for a pane's address box: an extension cannot search in a frame */
const SEARCH_URL = "https://www.bing.com/search?q=";

const HAS_SCHEME = /^[a-z][a-z\d+.-]*:/i;
const LOOKS_LIKE_HOST = /^[^\s]+\.[^\s]+$/;

/** What typing into an address box means: an address as is, a bare host over https, else a search */
export function addressFrom(text: string) {
  const typed = text.trim();
  if (HAS_SCHEME.test(typed)) {
    return typed;
  }
  if (LOOKS_LIKE_HOST.test(typed)) {
    return `https://${typed}`;
  }
  return SEARCH_URL + encodeURIComponent(typed);
}

/** Edge's own icon for any address, served to extensions that ask for the `favicon` permission */
export function faviconOf(url: string) {
  const favicon = new URL("_favicon/", browser.runtime.getURL("/"));
  favicon.searchParams.set("pageUrl", url);
  favicon.searchParams.set("size", "32");
  return favicon.href;
}
