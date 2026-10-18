# The enhanced find bar

`Ctrl+F` opens a find bar of our own (`app/native-split/src/find.h`) with three
modes: plain **text**, a **regular expression**, and **Ask Copilot**. The panel's
"Enhanced find" switch turns it on (the DLL's config wire, `config.h`, carries it
as the last u32); with it off, or on a page the bar cannot search, Edge's own find
is untouched.

Evidence levels: **[run]** observed in a throwaway Edge (own `--user-data-dir`
marker `fixedge-find-live`, driven over DevTools for stimulus and reading only,
captured with `PrintWindow`); **[unit]** a host unit test under
`app/native-split/test/`; **[infer]** reasoned, not checked.

## Parts

| piece | file | tested |
|---|---|---|
| text to regex, shape check, match stepping, bar and page reports | `find_text.h` | [unit] |
| paragraph labels, prompts that fit Copilot, reading its reply | `ask_text.h` | [unit] |
| where the bar and Copilot's page sit | `find_layout.h` | [unit] |
| the bar's page (a `data:` URL WebView, like the share strip) | `find_page.h` | [run] |
| scripts run in the searched tab (isolated world) | `find_scripts.h` | [run] |
| Copilot's web chat, driven like a person | `copilot_chat.h`, `copilot_scripts.h` | partly, below |
| the controller: polling, tabs, the Ask flow | `find.h` | [run] |

## How it works

- **A bar of our own.** A WebView docked as the last child of the MCV's parent
  (as `gemini.h` does), top right of the page, loading `find_page.h` as a data
  URL. It cannot call us, so a 100 ms UI-thread timer polls it for what the user
  did, exactly as `share.h` polls the share strip. [run]
- **Highlighting without touching the page.** The search script builds the page's
  visible text (a newline between blocks, whitespace squeezed as the page shows
  it), runs the `RegExp`, maps each match back to a DOM `Range`, and paints them
  with the CSS Custom Highlight API (`CSS.highlights`; the stylesheet goes in
  through `adoptedStyleSheets`). Nothing in the page's DOM is changed; closing the
  bar removes the highlights. Hidden text, `script` and `style` are skipped;
  shadow DOM and iframes are not searched. [run, text/regex/case/whole-word,
  counts, next/previous with scrolling, a tab switch]
- **Regex safety.** `find_text::CheckRegex` refuses an unbalanced group or class,
  a lone backslash and a nested repeat such as `(a+)+` (a page's `RegExp` runs on
  its main thread and would hang it); the page's own error message is shown for
  anything else. A search stops at 5000 matches and says so. [unit, run]
- **Ctrl+F.** The window procedure the DLL already installs sees `WM_KEYDOWN`
  for `F` with Control (and no Shift or Alt) and opens the bar when the active
  tab is an http, https or file page that is not a PDF; otherwise it falls
  through to Edge. Ctrl+Shift+Alt+F is the same thing as a registered hot-key,
  and channel codes 810 / 811 (`channel.h`) open and close it for scripts.
  [run: the bar opened from a posted `Ctrl+F` `WM_KEYDOWN` with the key state set
  in the throwaway's own input queue. **Not run: a physical keystroke.** It rests
  on Chromium forwarding keys to the top-level window, which is where the
  subclass sits; if Edge handles Ctrl+F first, use the hot-key. A page that
  handles Ctrl+F itself (a web editor) loses it while the switch is on.]
- Only the first browser window is subclassed by the DLL, so the bar answers
  Ctrl+F there; other windows keep Edge's find (a limit the split's hot-keys
  share).

## Ask Copilot

Edge exposes no API to ask its Copilot a question from outside. The honest
approach used here: a Copilot **web chat of our own** (`copilot.microsoft.com`)
in a hidden WebView in the window's profile, driven like a person would.

1. The page script (`ReadParagraphsScript`) splits the page into one paragraph
   per block of text and remembers where each one is.
2. `ask_text.h` drops short labels, cuts long paragraphs, and plans up to four
   prompts of about 6000 characters, each paragraph under a `[P12]` label. The
   prompt asks Copilot to answer with labels only, most relevant first, `NONE`
   when nothing answers, plus one sentence.
3. `copilot_scripts.h` puts the prompt in Copilot's message box and presses send;
   `copilot_chat.h` waits for the answer to stop growing and hands it back.
4. `ParseAnswer` reads labels from the reply **only if they were offered in that
   prompt**, so a reply cannot mark something that was not sent. The marked
   paragraphs are highlighted in reading order and the bar steps through them.

What was verified, and what was not:

- [run] The whole pipeline against a **mock chat page** that behaves like a chat
  (a box, a send button that wakes on text, an answer that streams): the page was
  read, the prompt arrived with its bracketed labels, the reply was parsed, the
  right paragraphs were highlighted and stepped through, including when the
  answer's markup is unknown (the fallback reads the text after the prompt).
- [run] On the **real** `copilot.com/chat` (an anonymous throwaway profile): the
  message box (`#m365-chat-editor-target-element`, a contenteditable) is found,
  the prompt goes in, the send button (`button.fai-SendButton`) wakes and is
  pressed. The landing page redirects and replaces the document after the first
  load, which the driver survives by sending again.
- [run] The first send then met a **"Security check required" dialog** that only
  a person can pass, and Copilot answered "Your request couldn't be completed".
  The driver sees the dialog, shows Copilot's page under the bar for the user, and
  keeps waiting (it presses send again once the dialog is gone). So **a real
  Copilot answer was never read**: the answer's markup (`data-content=
  "ai-message"` and friends in `copilot_scripts.h`) is a guess, backed by the
  fallback. Whether a signed-in profile gets the security check is unknown.
- Copilot's editor drops the line breaks of a pasted prompt, which is why labels
  are bracketed and the question quoted.
- It is a scrape of a page that is not ours; when Copilot changes its markup the
  selectors at the top of `copilot_scripts.h` are the one place to fix.

## Limits

- A PDF, an Edge page and a page without a live renderer use Edge's own find.
- Closed shadow roots, iframes and text drawn in canvas are not searched.
- Ask Copilot sends the page's visible text (up to about 60000 characters, in at
  most four prompts) to Copilot's web service. That is the feature; it is also
  the reason it is a separate mode and not what Ctrl+F does by default.
- The panel's dev instance signals the DLL's shared unload and config events;
  during testing that unloaded the test DLL from the throwaway Edge more than
  once, and its own DLL was injected in its place.
