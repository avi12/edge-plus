// The one home of what the panel calls each browser assistant, and of the enhanced find's wording
// that follows the chosen one. The DLL has its own copy of the names (chat_target.h), which must match.
import { Assistant } from "@/lib/bridge";

export const ASSISTANT_NAMES: Record<Assistant, string> = {
  [Assistant.Copilot]: "Copilot",
  [Assistant.Gemini]: "Gemini"
};

/** The regular-expression mode, as the user reads it; the DLL's `kRegExpName` is the same word. */
const REGEXP_NAME = "RegExp";

export function findTitle(assistant: Assistant): string {
  return `Enhanced find: ${REGEXP_NAME} and Ask ${ASSISTANT_NAMES[assistant]}`;
}

export function findDescription(assistant: Assistant): string {
  const name = ASSISTANT_NAMES[assistant];
  return (
    `Edge's own Ctrl+F bar gets two more options, ${REGEXP_NAME} and Ask ${name}, next to Match case in its ` +
    `options menu. With ${REGEXP_NAME} on, what you type is a pattern: every match on the page is highlighted and ` +
    `counted in the bar's own n/m, and Enter or the arrows step through them (it is case sensitive; start with ` +
    `(?i) to ignore case). With Ask ${name} on, what you type is a question, and ${name} names the paragraphs of ` +
    `the page that answer it; they are highlighted. Either one greys out Match case and Whole word, and turns the ` +
    `other off. Typing /pattern/i or "? your question" works as a shortcut with both off. Anything else is ` +
    `Edge's normal find, and Edge pages and PDFs keep Edge's find. ` +
    `${name} answers from its own web page in your Edge profile, so it may ask you to sign in or pass a ` +
    `check once. Live, no restart. Open Edge first.`
  );
}
