# The enhanced find

Edge's own `Ctrl+F` bar (its text field, its "n/m" label, its arrow buttons)
stays the only UI. Two things typed into its box are ours; the panel's "Enhanced
find" switch turns them on (the DLL's config wire, `config.h`, carries it as the
last u32):

| typed | what happens |
|---|---|
| `/pattern/flags` (flags from `gimsu`) | a RegExp search of the page: every match highlighted, counted in the bar's own "n/m", Enter / Shift+Enter / the arrows step through them |
| `? your question` (a question mark, a space) | the page's paragraphs go to the **chosen browser assistant** (Copilot or Gemini, `assistant::g_assistant`), which names the ones that answer; they are highlighted and stepped through the same way |
| anything else | Edge's own search, untouched (so is every Edge page and PDF) |

A RegExp that is not closed yet (`/pri`) is plain text until its closing slash
is typed. A question needs its **space** after the mark (`?id=3` is text). The
words the user reads are one source per side: `find_text::kRegExpName`
("RegExp") and `chat_target::Name` in the DLL, `assistantNames.ts` in the panel
(the card's title and description follow the Browser assistant card).

Evidence: **[run]** observed in a throwaway Edge (own `--user-data-dir` marker
`fixedge-askg-live`, `--inprivate --disable-sync --disable-extensions`, the bar
and the page captured with `PrintWindow`, keys posted to the throwaway's own
windows); **[unit]** a host unit test under `app/native-split/test/`; **[infer]**
reasoned from disassembly, not run.

## Parts

| piece | file | tested |
|---|---|---|
| what a typed box means, RegExp shape check, match stepping, script answers | `find_text.h` | [unit] |
| paragraph labels, prompts that fit a chat, reading its reply | `ask_text.h` | [unit] |
| each assistant's name, page and selectors (one table) | `chat_target.h` | [unit] |
| the scripts that drive a chat page, for any assistant | `chat_scripts.h` | [run] |
| the per-window hidden chat, its steps | `assistant_chat.h` | [run] |
| where the chat page sits when it needs the user | `find_layout.h` | [unit] |
| scripts run in the searched tab (isolated world) | `find_scripts.h` | [run] |
| the hooks on Edge's find bar and the flow | `find.h` | [run] |
| the bar's RVAs / offsets / wrappers | `offsets.h`, `edge.h` | [run] |

## The native bar (reverse engineered, Edge 155.0.4283.45)

`FindBarView` is the bar's content view (`FindBarHost` is its widget, which
Windows draws as its own title-less top-level window next to the browser's).
Chromium's flow, confirmed in the disassembly:

- **The text changed** -> `FindBarView::Find(this, const u16string_view*)` ->
  `FindBarController::OnUserChangedFindText`, then `FindTabHelper::StartFinding`.
  [run: hooked]
- **Enter** -> `FindBarView::HandleKeyEvent` (the view's `TextfieldController`
  subobject, `this` = view + `0x338`; arguments: the text field, a `ui::KeyEvent`
  with its type at `+0xc` and key code at `+0xb0`). `FindNext` is **inlined**
  into it, so hooking `FindNext` alone misses Enter. Shift is read with
  `GetKeyState`. [run: hooked]
- **An arrow button** -> `FindBarView::FindNext(this, bool reverse)`, which reads
  the text field itself. [infer: hooked, the arrows were not clicked]
- **Close** -> `FindBarController::EndFindSession` (Esc, the X, a click into the
  page). [run: hooked; Esc cleared our highlights]
- **The result** -> `FindBarView::UpdateForResult(this, const
  FindNotificationDetails&, const std::u16string& findText)`: it shows
  "ordinal/matches" in `FindBarMatchCountLabel` (view + `0x398`) and enables or
  disables the arrow buttons. It **rewrites the text field** when `findText`
  differs from it (and the IME is not composing), so we pass the field's own
  text, as a read-only long-form `u16string`. [run]

For one of our queries the `Find` hook first calls Edge's own `Find` with an
empty text (which clears Edge's marks and count), then runs ours; for plain text
it calls straight through.

**`FindNotificationDetails` is `0x28` bytes**: request id `+0`, matches `+4`, the
selection rect `+8`, the active match's 1-based ordinal `+0x18`, the final-update
flag `+0x1c`, and a **ref-counted pointer at `+0x20`** that the copy constructor
(run by `FindBarMatchCountLabel::SetResult`) reads and `lock xadd`s. Built as
`0x20` bytes it read stack garbage and crashed the browser, now and then
(`c0000005` at `FindNotificationDetails::FindNotificationDetails`+0x23). The bar
skips a result equal to the last one it showed, so every result carries a fresh
request id.

**Status text.** The count label is a `views::Label`; its virtual at vtable
`+0x3a8` takes a `const u16string_view*`. A short status ("Enter to ask",
"Asking...", "Needs you", "Bad RegExp") goes straight into it, and the sentence
behind it is the label's tooltip (`View::SetTooltipText`). [run: "Enter to ask",
"Needs you"; the tooltips were set, not hovered]

**Which browser window.** Edge's find bar window also resolves to the browser's
views, so a session keyed to the *foreground window* was keyed to the bar's
window and dropped when that changed. `find::BrowserWindowInUse` takes the
owner / foreground window that has a **title** (the bar's has none), else the
window the DLL subclassed. Only the first browser window is subclassed, which is
the fallback.

## Highlighting without touching the page

The search script builds the page's visible text (a newline between blocks,
whitespace squeezed as the page shows it), runs the `RegExp`, maps each match
back to a DOM `Range`, and paints them with the CSS Custom Highlight API
(`CSS.highlights`; the stylesheet goes in through `adoptedStyleSheets`). Nothing
in the page's DOM is changed; closing the bar removes the highlights. Hidden
text, `script` and `style` are skipped; shadow DOM and iframes are not
searched. A search stops at 5000 matches (the tooltip says so).
`find_text::CheckRegex` refuses an unbalanced group or class, a lone backslash
and a nested repeat such as `(a+)+` (a page's `RegExp` runs on its main thread
and would hang it). [run: counts, the first match scrolled into view, Enter
stepping 1/6 -> 3/6; unit: the shape check]

A RegExp is case sensitive unless it says `i` (as in JavaScript); the bar's own
"Match case" / "Whole word" options are not read for it.

## Asking the assistant

Edge exposes no API to ask Copilot or Gemini from outside. A web chat of our own
(a hidden WebView in the window's profile) is driven like a person would:

1. The page script splits the page into one paragraph per block of text.
2. `ask_text.h` drops short labels, cuts long paragraphs, and plans up to four
   prompts of about 6000 characters, each paragraph under a `[P12]` label. The
   prompt asks for labels only, most relevant first, `NONE` when nothing
   answers, plus one sentence.
3. `chat_scripts.h` puts the prompt in the chat's message box and presses send;
   `assistant_chat.h` waits for the answer to stop growing and hands it back.
   Which assistant, and so which page and selectors (`chat_target.h`), is chosen
   per question from the panel's choice: change the assistant while the bar is
   open and the next question goes to the new one (the bar's wording is read at
   the time it is shown). A chat page left on the other assistant is pointed at
   the new one's page first.
4. `ParseAnswer` reads labels **only if they were offered in that prompt**.

When the assistant needs the person (no message box after 12 s, or a dialog),
its page is shown at the top right of the page, under the bar
(`find_layout::ChatRect`), the label says "Needs you", and the question goes
through once they have dealt with it.

### Verified, and not

- **Gemini** (`gemini.google.com/app`) [run, anonymous InPrivate profile]: the
  whole pipeline. Selectors read off the live page: the message box is
  `rich-textarea .ql-editor[contenteditable="true"]` (also aria-label "Enter a
  prompt here"); the send button is `button[aria-label="Send message"]` and
  exists only while the box has text; an answer is a `message-content` inside a
  `model-response`. A real question about the test page came back as
  `P2, P5, P1` plus a sentence, was parsed, and three paragraphs were highlighted
  with "1/3" in the bar. **Not verified:** the stop-button and dialog
  selectors (`button[aria-label*="Stop"]`, `mat-dialog-container`) and a signed-in
  or consent-page case; if Gemini shows no box the driver shows its page for the
  user as for Copilot.
- **Copilot** [run, anonymous InPrivate profile]: `copilot.com` now answers a
  signed-out profile with a sign-in page; the driver found no box, said
  "Needs you" and showed the page (screenshot). **A real Copilot answer was
  never read in this profile** (earlier, against a mock chat page and a real page
  that met a "Security check required" dialog); the answer selectors in
  `chat_target.h` for Copilot are a guess backed by the fallback that reads the
  text after the prompt, which Gemini's run also exercised the same way (its
  markup was confirmed afterwards).
- It is a scrape of pages that are not ours; when one changes, its row in
  `chat_target.h` is the one place to fix.

## What was run, and what was not

- [run] `/pric/i` typed into the native bar: "1/6", the matches painted in the
  page; Enter stepped to 3/6; Esc cleared the highlights; `? what does it cost`
  showed "Enter to ask", then (Gemini) the answer's paragraphs painted and "1/3";
  (Copilot) "Needs you" with its page at the top right.
- [run] Typing was posted `WM_CHAR`s to the bar's window and Enter a posted
  `WM_KEYDOWN`, not a physical keyboard. Ctrl+F was a posted `WM_KEYDOWN` with the
  throwaway's key state set (Edge opened its own bar).
- **Not run:** the arrow buttons (`FindNext`), Shift+Enter, switching from one of
  our queries back to plain text, a tab switch with the bar open, two browser
  windows, changing the assistant live while the bar is open (the code reads it
  at each use; the panel push path is the existing one), and the bar's tooltips
  on hover.
- A first launch of a throwaway profile with the default flags signed in to the
  OS account and began syncing into it (a sync dialog and a synced extension).
  The harness now runs `--inprivate --disable-sync --disable-extensions`, and
  that profile was deleted.

## Limits

- A PDF, an Edge page and a page without a live renderer use Edge's own find.
- Closed shadow roots, iframes and text drawn in canvas are not searched.
- A question sends the page's visible text (up to about 60000 characters, in at
  most four prompts) to the assistant's web service. That is the feature; it is
  also why it is typed (`? `) and never what Ctrl+F does by default.
- F3 / Ctrl+G (find next from the page) call `StartFinding` directly and are
  not hooked: with one of our queries in the box they run Edge's find of the
  empty text.
- Only the first browser window is subclassed by the DLL; its find hooks work in
  every window, but the fallback window for a bar whose own window cannot be
  found is that first one.
- The panel's dev instance signals the DLL's shared unload and config events;
  during testing that can unload a test DLL, and its own is injected in its
  place.

## Explicit toggles instead of typed prefixes (not built)

Asked for: "Use regular expression" and "Ask <assistant>" as checkable options
in the native bar (mutually exclusive, Match case / Whole word greyed out while
either is on, RegExp case sensitive with a leading `(?i)`). What reading the
disassembly (155.0.4283.45) showed, and what stopped the work:

- **The bar's options button is not a menu.** `FindBarView::
  EdgeOnFindOptionsButtonClicked` -> `EdgeToggleFindOptions` shows or hides an
  inline `FindOptionsControlView` (added by `EdgeMaybeAddFindOptionsView`; its
  children are toggle views added with `AddFindInPageOptionChildView`, and
  `TurnOffAllFindFilters` / `ExtraParameters::GetNumberOfFindOptionsOn` count
  them). Match case and Whole word are toggle views in that row, not
  `SimpleMenuModel` items, so there is no model builder, execute or
  checked/enabled query to hook. Our two options would be two more toggle views
  built with Edge's own view constructors and added to that row; greying Match
  case / Whole word means `View::SetEnabled` on the two existing children.
- **Edge updated under the DLL.** The machine's Edge went from 155.0.4283.45 to
  155.0.4283.62 during this work, and the old build's files are gone. The DLL
  (pinned to .45, `verify.h`) now refuses to hook, so nothing could be run, and
  the disassembly tooling reads the .45 `msedge.dll`. Doing this needs the DLL
  re-pinned to the new build (every RVA in `offsets.h` re-derived against a .62
  PDB) first.
- Nothing for the toggles was written; the typed `/pattern/flags` and
  `? question` forms above are what exists.
