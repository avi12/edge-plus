# The enhanced find

Edge's own `Ctrl+F` bar (its text field, its "n/m" label, its arrow buttons)
stays the only UI. Two switches in its options ("RegExp" and "Ask <assistant>",
beside Match case; see the last section) make what is typed a pattern or a
question. With both off, two things typed into its box are still ours, as
shortcuts; the panel's "Enhanced find" switch turns it all on (the DLL's config
wire, `config.h`, carries it as the last u32):

| typed | what happens |
|---|---|
| `/pattern/flags` (flags from `gimsu`) | a RegExp search of the page: every match highlighted, counted in the bar's own "n/m", Enter / Shift+Enter / the arrows step through them |
| `? your question` (a question mark, a space) | the page's text goes to the **browser assistant** (Copilot), which names the sentences that answer, from one paragraph or several; they are highlighted and stepped through the same way |
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
| sentence labels, prompts that fit a chat, reading its reply | `ask_text.h` | [unit] |
| each assistant's name, page and selectors (one table) | `chat_target.h` | [unit] |
| the scripts that arm and read the side panel's chat | `chat_scripts.h` | [run] |
| a question's steps: side panel, arm, read the answer | `assistant_chat.h` | [run] |
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

Edge exposes no API that returns Copilot's answer, but it does have the function
the page's "Ask Copilot" menu runs, and Copilot's answer is in a page we can
read. So a question takes the same path as a person's:

1. The page script splits the page into one paragraph per block of text and
   each paragraph into sentences, keeping every sentence as a live DOM `Range`.
2. `ask_text.h` drops short labels, cuts long sentences, and plans up to four
   prompts of about 6000 characters: one line per paragraph, each sentence
   under its own `[P12]` label. The prompt asks for labels only (a run of
   neighbours may be written `P3-P5`), most relevant first, `NONE` when nothing
   answers, plus one sentence. Only the named sentences are highlighted, so an
   answer inside a paragraph marks just that part, and an answer spread over
   several paragraphs marks each sentence.
3. `assistant_chat.h` calls `edge_copilot::utils::SendPageContextualPromptToChat`
   (`0x93dc691`: `browser`, the text, a context string, a title, a bool;
   `edge::SendPromptToSidePanel`). The context string must be a valid, empty
   string: in 155.0.4283.62 a null one is read on the non-iframeless path and
   crashed the browser (`msedge.dll+0x93e2bf6`, read of address 0x17). [run]
   It builds an `AskCopilotRequest` and dispatches it to the **side panel**, which
   opens with the prompt as the user's message. (The first argument after the
   browser is the text that appears as the user's message; passing the prompt
   there, not in the title argument, is what shows it in the chat. Both strings
   are libc++ long-form `std::string`s over our buffers, which the callee only
   copies.) [run: the panel opened and its chat showed our whole prompt as "You
   said"]
4. In 155.0.4283.62 the panel is *iframeless*: its WebContents is the Copilot
   web app itself, at `https://cr.m365copilotapp.svc.cloud.microsoft/chat/...`,
   so its main frame is the chat (`edge::CopilotChatFrame` accepts that or the
   shell below). [run] The older shell's WebContents is the one whose URL is `edge://discover-chat-v2/`,
   found in `WebContentsImpl::GetAllWebContents` (`0x28282c6`; it fills a record
   whose first word points at the WebContents pointers and whose second is their
   count, not a begin/end vector). `CopilotSidePanelController::GetHostManagerForBrowser`
   answers null for this "iframeless" panel, so the host manager route does not
   work. The chat itself is an **iframe** of `copilot.microsoft.com` inside that
   page, so scripts go to that frame: `WebContentsImpl::ForEachRenderFrameHost`
   (`0x274b980`, given a `FunctionRef` of `{object, invoker}`) lists the frames,
   `RenderFrameHostImpl::GetLastCommittedURL` (`0x113c6c0`) names them, and
   `RunScriptInFrame` runs ours in the isolated world. [run]
5. `chat_scripts.h` arms the frame (remembers how many answers it held and the
   prompt), then reads it every 0.7 s until the answer stops growing, and
   `assistant_chat.h` hands it back. `ParseAnswer` reads labels **only if they
   were offered in that prompt**.
6. The panel stays out of the way afterwards: when the question found it
   closed, `assistant_chat::ReleasePanel` closes it again with
   `edge_copilot::utils::CloseUndersideChat(browser, true)` (`0x93d99bf`, what
   the panel page's own close message runs) once the ask is over, so the page
   gets its width back. It is still open while the question is answered: the
   chat has to be on screen for Edge to run it. [run]

With the panel's first-use notice ("Navigate the Web with Copilot ... Continue")
showing, the composer is covered and the question waits behind it: the label says
"Accept notice" (tooltip: accept it in the side panel and the question goes
through), for up to two minutes, and the question is delivered by Edge as soon as
the person clicks Continue. We never accept it for them. If the panel does not
open within 20 s the status is "Failed: Copilot isn't ready". [run: "Accept
notice" in the bar; with Continue clicked the prompt and its paragraphs were in
the chat, followed by Copilot's own "your Copilot is migrating" notice for the
throwaway's account, which parses as no answer]

Limits of this path: the panel is found by URL among every WebContents of the
process, so with two browser windows open it can be another window's; and a
question sent while a conversation is open is appended to it.

### Verified, and not

- **Gemini** [run, removed since]: the whole pipeline was run against
  `gemini.google.com/app` while the find bar could also ask Gemini (the choice
  has been removed; see `docs/re/removed-gemini-integration.md`). A real question
  came back as `P2, P5, P1` plus a sentence, was parsed, and three paragraphs were
  highlighted with "1/3" in the bar.
- **Copilot, signed in** [run 2026-10-24, throwaway profile signed in to the
  OS account, `--disable-sync`]: `? which plan includes daily backups` on a
  six-paragraph page: the panel opened, Copilot answered `P3. The Business plan
  includes daily backups.`, the paragraph was highlighted and the bar said 1/1.
  After the sentence split, `? does any plan include phone support` marked only
  "It adds unlimited websites, daily backups and priority support by phone."
  and `? what is the refund policy and when can I reach support` marked a
  sentence in one paragraph and a whole other paragraph; the panel closed
  itself afterwards. Not run: a panel that was already open before the
  question (it must stay open), and a page long enough for several prompts.
- **Copilot** [run, anonymous InPrivate profile, earlier]: `copilot.com` now answers a
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
  showed "Enter to ask", then (Gemini, since removed) the answer's paragraphs painted and "1/3";
  (Copilot, side panel) "Accept notice" until the notice is accepted.
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

## The RegExp and Ask switches in the bar's options (155.0.4283.62)

The bar's "..." button does not open a menu: `FindBarView::EdgeToggleFindOptions`
shows an inline `FindOptionsControlView` (built by `EdgeMaybeAddFindOptionsView`),
a column of `FindInPageOptionToggleView`s. Each is a title `views::Label` and a
`views::EdgeToggleButton`. Edge builds ids 0 Match case, 1 Whole word, 2 Match
diacritics (each only where its feature is on; title strings 0xa0a5 / 0xa0a6 /
0xa0a7), and `edge::GetFindInPageOptionToggleTitleText` returns an **empty** title
for every other id. So the two switches are Edge's own views, built with Edge's
own constructor:

- **Adding them.** `FindOptionsControlView::AddFindInPageOptionChildView(this,
  id, width, FindBarHost*)` creates the toggle for `id` at the end of the column
  and records it in the view's `std::map<int, raw_ptr<toggle>>` (+0x338; the root
  at +0x340, a node's key at +0x20 and value at +0x28). We call it for id 3
  (RegExp) and 4 (Ask) from a hook on `EdgeMaybeAddFindOptionsView` (after the
  original), taking the width from one of Edge's toggles and the host from
  `FindBarView+0x370`, then `InvalidateLayout` on the options view and
  `FindBarView::PreferredSizeChanged`; the bar's window grew by itself.
  [run: the two rows appeared under Match whole word; diacritics is not built in
  this profile]
- **Their words.** `Label::SetText(label, const u16string_view*)` on the toggle's
  label (toggle +0x358): "RegExp" and "Ask <assistant::Name()>", set whenever the
  options are built or shown (a hook on `EdgeToggleFindOptions`, after the
  original) and when the panel's switch changes. [run: "Ask Copilot"]
- **Their state is the switch itself** (`EdgeToggleButton::GetIsOn` on the button
  at toggle +0x360): `find::OptionOf(findBarView)` reads it each time, so it is
  per window and goes with the bar; nothing of ours stores it. Edge's own
  `GetToggleState` / `SetToggleState` read and write the tab helper's find
  parameters by id, which means nothing for ids 3 and 4, so ours bypass them.
- **A click** runs `FindInPageOptionToggleView::OnButtonToggled`, hooked. For ids
  0..2 it is Edge's. For ours the button has **already moved** when the callback
  runs (flipping it again put it back, the first try): we take the new state,
  turn the other of ours off, and search the box again through our own `Find`
  hook. Switching both off re-runs the box as Edge's own find. [run]
- **Greying Edge's.** `View::SetEnabled(false)` on both the label and the
  `EdgeToggleButton` of ids 0..2 (a disabled switch takes no click) while either
  of ours is on; Match diacritics is included, since it means as little to a
  RegExp. [run: Match case and Whole word greyed, then back]
- **What the box means with a switch on.** `find_text::ParseQuery(typed, option)`:
  RegExp: the whole box is the pattern, **case sensitive** (JavaScript's default),
  and a leading `(?i)` is dropped and adds the `i` flag; Ask: the trimmed box is
  the question, asked on Enter as before. Typed slashes and `? ` are plain text
  then. With no switch on the old shortcuts (`/pattern/flags`, `? question`) work
  as before. An empty box searches nothing. [unit: 12 checks; run: `(?i)pric` 1/6
  with "Pricing" and "pricing" marked, `pric` 1/4, a question 1/2]
- **RegExp flags.** While RegExp is on, seven more switches, one per JavaScript
  flag but g, show directly under it (ids 5..11, `find_flags.h`'s table, the ids in
  `offsets.h`), each titled with its letter: Match indices (d),
  Ignore case (i), Multiline (m), Dot matches newline (s), Unicode (u), Unicode
  sets (v), Sticky (y). The titles are pushed in by two em spaces so the column
  reads as a sub-group; with RegExp off they are hidden (`View::SetVisible`, then
  the same relayout as adding a row). They are built by the same
  `AddFindInPageOptionChildView` call, in the same hook, **before** the Ask row so
  that they sit under RegExp (children append in call order). Nothing stores
  them: `find::FlagsOn` reads each switch. A click only re-runs the search
  (`h_button_toggled`). **Multiline is on when a bar is built**
  (`find_flags::DefaultFlags`: a search has always had `^` and `$` at line
  edges), so with nothing touched the behavior is the old one, case sensitive.
  **g has no switch: every match is always found and marked**, so the merged
  flags always carry it. The flags the page runs are `find_flags::Merge(typed flags,
  switches)`, canonical order `dgimsuvy`: typed and switched flags are OR-ed,
  each letter once, so `(?i)` and Ignore case agree. They apply only with the
  RegExp switch on, never to the `/pattern/flags` shortcut (which still implies g
  and m, and now also takes d, v and y). What each does here:
  - **m** off: `^` and `$` stand for the start and end of the whole page text,
    not of each line.
  - **y**: the first match is found anywhere, then each next match must start
    exactly where the one before it ended, so only a contiguous run from the first
    match is marked (`o` on the test page: 1/5 plain, 1/1 sticky). (JavaScript's own sticky would require the
    first match to start at index 0, which for a page search finds almost
    nothing.)
  - **d** asks for match indices: the page's `exec` has them, nothing shows them,
    so it changes nothing visible.
  - **u** and **v** exclude each other, as in JavaScript: turning one on turns the
    other off (`h_button_toggled`). A pattern that the chosen mode refuses (`\-`
    outside a class under `u`/`v`, a `v`-only class under `u`) comes back from
    `new RegExp` as the "Bad RegExp" status with the page's own message, and the
    bar keeps working; typing both `u` and `v` after the closing slash is refused
    the same way.
  y is searched through a global, non-sticky copy of the RegExp until the first
  match, then the sticky one continues from its end.
  [run: 1/6 -> with Ignore case; `^bar$` 1/1, with Multiline off 0/0; `foo.bar`
  0/0, with Dot matches newline 1/1; `o` 1/5, Sticky 1/1; a `v`-only class "Bad
  RegExp", 1/10 with Unicode sets, "Bad RegExp" again once Unicode replaces it]
- **Line breaks in a RegExp.** The box is Edge's own single-line `views::Textfield`,
  which holds no raw line break, and a page's text is one line per block with a
  line break between blocks. So with RegExp on, **Shift+Enter** (or Ctrl+Enter)
  types the two characters `\n` at the caret instead of stepping back (the
  arrow button still does; with RegExp off Shift+Enter is Edge's own
  previous-match), and **Ctrl+V** / Shift+Insert of text that holds CR or LF
  types it with each break written `\n` (Edge's own paste would flatten it; a
  paste without line breaks is left to Edge). A RegExp reads `\n` exactly as a
  real line break. Both go in as posted `WM_CHAR`s to the focused bar window, from
  the `HandleKeyEvent` hook (`find::HandleLineBreakKey`), so a selection is
  replaced like typing. `find_text::EscapeLineBreaks` is the pure part [unit].
  [run: `foo` Shift+Enter `bar` gave the box `foo\nbar`, matching across the
  two blocks]
- **Ask is wide.** Ask Copilot takes a whole question, so with Ask on the bar's
  window is made twice Edge's width (at most the browser window less a margin,
  and at most 1100 px), keeping its right edge (`find::ApplyBarWidth`,
  `SetWindowPos` on the bar's own top-level window, found among the thread's
  windows as the untitled one owned by the browser window). Edge lays the bar out
  from its window, so the box, its flexible view, takes the new room. With Ask off
  or RegExp chosen the width goes back. The Ask switch's title ends "(Enter)",
  and the count label no longer carries "Enter to ask" (its tooltip still does).
- **Switching the enhanced find off** hides ours and enables Edge's (the panel's
  switch goes through `find::Apply`). [infer: not run, it needs the panel's shared
  config event]

Not run: two browser windows (the state is in each window's own views),
the labels changing when the assistant is changed while the options are open
(they refresh on the next show), Match diacritics, the keyboard (tab to a
switch, space), and Shift+Enter / arrows with a switch on.

## The /literal/flags decoration (155.0.4283.62)

With the RegExp switch on, the box reads like a JavaScript literal: `/` before
the text field and `/` plus the flags the search runs with after it (`/gi` with
Ignore case on and Multiline off). The words come from
`find_text::RegExpDecoration`, which runs the same `ParseQuery` and
`find_flags::Merge` as the search, so the shown flags are the searched ones (a
typed `(?i)` shows as `i`) [unit].

- **Where they live.** The text field is not a direct child of `FindBarView`: its
  parent is a row view (vtable rva `0x116baed0`) holding the text field
  (`0x11726410`), the `FindBarMatchCountLabel` (`0x116bb610`), a separator
  (`0x116d19f0`) and the buttons. Two plain `views::Label`s go into that row on
  either side of the text field (`AddChildViewAt` at its index and index + 1), so
  the caret, the selection, copy and paste and the searched text never include
  them, and the row's layout mirrors them in a right-to-left UI. [run]
- **Building a Label.** `CheckedOperatorNew3(0x4a8)` (FindBarMatchCountLabel's own
  fields start at `0x4a8`, so a Label fits), then `views::Label::Label(this, const
  u16string_view*, int context, int style, int directionality)` at `0x26ed562`, the
  overload FindBarMatchCountLabel's constructor calls, with the count's own context
  and style (`Label+0x388` / `+0x38c`). A constructed plain Label's vtable is
  `0x117297d0` (read live; the PDB names several Label vftables). [run]
- **Their look.** `Label::SetFontList` with the text field's `GetFontList` (same
  size and baseline as the typed text: the suffix's `i` covers the same rows as a
  typed `i`), and `Label::SetEnabledColor` with the count label's requested colour
  (the `std::optional<ColorVariant>` at `Label+0x3b0`, engaged flag `+0x3b8`),
  Edge's dimmed secondary text colour for the theme (`#9c9c9c` against the text's
  `#fafafa` in the dark default). [run]
- **Finding them again.** By position: a plain Label right before and right after
  the text field (Edge has none there), so a reloaded DLL reuses the ones an
  earlier load made. With RegExp off, or the find switched off, they are hidden
  (`View::SetVisible`), never removed. [run: shown, then hidden with RegExp off]

## Our switches keep their state when the bar is shown again (155.0.4283.62)

When the bar is shown again (and on a tab change) every switch runs
`FindInPageOptionToggleView::EdgeOnActiveWebContentsChanged` (`0xc8b5ff0`), which
sets it from `edge::GetFindInPageOptionToggleValue(the tab's find helper, id)`:
Edge's own three keep their state there, but for any other id it is false, and it
calls no `OnButtonToggled`. So on reopen RegExp (and Multiline) went off while
the flag rows, the slashes and Edge's greyed switches still showed RegExp on.
That function is hooked: for our ids it does not run (they keep the state they
have, per window, as RegExp always has), and after any switch the options are
synced again from the switches (`SyncToggles`, which drops a re-entrant call made
while it builds a switch). [run: RegExp + Multiline on, Esc, reopen: still on,
flags shown, `/gm`; RegExp off, Esc, reopen: off, flags hidden, no slashes]
