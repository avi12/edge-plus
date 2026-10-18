// fluent_tokens.h — the look shared by the small pages of our own that Edge
// shows (the find bar). One concern: the Fluent 2 design tokens by hand (like
// the panel app), light and dark following the browser, and the base rules for
// the page, buttons and inputs written once. A page adds its own rules after
// these.
#pragma once

namespace fixedge::fluent_tokens {

// What every such page starts with: the document type, the language, the
// character set and the colour schemes it supports.
inline constexpr char kPageHead[] = R"HTML(<!doctype html>
<html lang="en">
<meta charset="utf-8">
<meta name="color-scheme" content="light dark">
)HTML";

// The rules inside the page's <style>.
inline constexpr char kBaseCss[] = R"CSS(
:root {
  --surface: #ffffff; --surface-hover: #f5f5f5; --surface-pressed: #e0e0e0;
  --stroke: #d1d1d1; --text: #242424; --text-muted: #616161;
  --brand: #0f6cbd; --brand-hover: #115ea3; --brand-pressed: #0f548c; --on-brand: #ffffff;
  --danger: #b10e1c; --success: #0e700e;
  --radius: 4px; --radius-lg: 8px;
  --font: "Segoe UI Variable Text", "Segoe UI", system-ui, sans-serif;
}
@media (prefers-color-scheme: dark) {
  :root {
    --surface: #292929; --surface-hover: #3d3d3d; --surface-pressed: #1f1f1f;
    --stroke: #666666; --text: #ffffff; --text-muted: #adadad;
    --brand: #479ef5; --brand-hover: #62abf5; --brand-pressed: #2886de; --on-brand: #000000;
    --danger: #dc626d; --success: #54b054;
  }
}
* { box-sizing: border-box; }
html, body { height: 100%; margin: 0; }
body {
  overflow: hidden;
  background: var(--surface); color: var(--text);
  font: 400 13px/1.4 var(--font);
}
button {
  font: inherit; color: var(--text); background: var(--surface);
  border: 1px solid var(--stroke); border-radius: var(--radius);
  height: 30px; padding: 0 10px; display: inline-flex; align-items: center; gap: 6px;
  cursor: pointer; white-space: nowrap;
}
button:hover { background: var(--surface-hover); }
button:active { background: var(--surface-pressed); }
button:focus-visible, input:focus-visible { outline: 2px solid var(--brand); outline-offset: 1px; }
button svg { width: 16px; height: 16px; fill: currentColor; flex: none; }
button.primary { background: var(--brand); border-color: transparent; color: var(--on-brand); font-weight: 600; }
button.primary:hover { background: var(--brand-hover); }
button.primary:active { background: var(--brand-pressed); }
button:disabled { opacity: .5; cursor: default; }
@media (prefers-reduced-motion: no-preference) {
  button { transition: background-color 80ms ease-out; }
}
)CSS";

}  // namespace fixedge::fluent_tokens
