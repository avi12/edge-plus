// The one home of what the panel calls the browser assistant, and of the enhanced find's wording that
// names it. The DLL has its own copy of the name (chat_target.h), which must match.
const ASSISTANT_NAME = "Copilot";

/** The regular-expression mode, as the user reads it; the DLL's `kRegExpName` is the same word. */
const REGEXP_NAME = "RegExp";

export const FIND_TITLE = `Enhanced find: ${REGEXP_NAME} and Ask ${ASSISTANT_NAME}`;

export const FIND_DESCRIPTION =
  `Edge's own Ctrl+F bar gets two more options, ${REGEXP_NAME} and Ask ${ASSISTANT_NAME}, next to Match case in its ` +
  `options menu. With ${REGEXP_NAME} on, what you type is a pattern: every match on the page is highlighted and ` +
  `counted in the bar's own n/m, and Enter or the arrows step through them. Under ${REGEXP_NAME} come the ` +
  `JavaScript flags as switches, each with its letter (every match is always found; Multiline starts on, so it is ` +
  `case sensitive until you switch on Ignore case, or start with (?i)); Unicode and Unicode sets exclude each other. ` +
  `Shift+Enter types a line break (as \\n) into the box instead of stepping back, and a paste with line breaks ` +
  `keeps them. With Ask ${ASSISTANT_NAME} on, the bar widens, and what you type is a question that you send ` +
  `with Enter; ${ASSISTANT_NAME} names ` +
  `the paragraphs of the page that answer it; they are highlighted. Either one greys out Match case and Whole ` +
  `word, and turns the other off. Typing /pattern/i or "? your question" works as a shortcut with both off. ` +
  `Anything else is Edge's normal find, and Edge pages and PDFs keep Edge's find. ` +
  `${ASSISTANT_NAME} answers from its own web page in your Edge profile, so it may ask you to sign in or pass a ` +
  `check once. Live, no restart. Open Edge first.`;
