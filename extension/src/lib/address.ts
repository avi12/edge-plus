/** Edge's own default engine, for a pane's address box: an extension cannot search in a frame */
const SEARCH_URL = "https://www.bing.com/search?q=";

/** What a pane with no address yet opens as once it is a tab again */
export const NEW_TAB_URL = "edge://newtab/";

/** Edge's own pages and other extensions' pages cannot be shown in a frame */
const FRAMEABLE_SCHEMES = ["http:", "https:", "file:"];

const SCHEME_PATTERN = /^[a-z][a-z\d+.-]*:/i;
const BARE_HOST_PATTERN = /^[^\s]+\.[^\s]+$/;

const FAVICON_SIZE_PX = 32;

/** What typing into an address box means: an address as is, a bare host over https, else a search */
export function addressFrom(text: string) {
  const typed = text.trim();
  const hasScheme = SCHEME_PATTERN.test(typed);
  if (hasScheme) {
    return typed;
  }
  const isBareHost = BARE_HOST_PATTERN.test(typed);
  if (isBareHost) {
    return `https://${typed}`;
  }
  return SEARCH_URL + encodeURIComponent(typed);
}

export const isFrameable = (url = "") => FRAMEABLE_SCHEMES.some(scheme => url.startsWith(scheme));

/** Edge's own icon for any address, served to extensions that ask for the `favicon` permission */
export function faviconOf(url: string) {
  const favicon = new URL("_favicon/", browser.runtime.getURL("/"));
  favicon.searchParams.set("pageUrl", url);
  favicon.searchParams.set("size", String(FAVICON_SIZE_PX));
  return favicon.href;
}
