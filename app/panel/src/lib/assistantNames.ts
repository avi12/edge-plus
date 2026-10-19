// The one home of what the panel calls the browser assistant, and of the enhanced find's wording that
// names it. The DLL has its own copy of the name (chat_target.h), which must match.
const ASSISTANT_NAME = "Copilot";

/** The regular-expression mode, as the user reads it; the DLL's `kRegExpName` is the same word. */
const REGEXP_NAME = "RegExp";

export const FIND_TITLE = `Enhanced find: ${REGEXP_NAME} and Ask ${ASSISTANT_NAME}`;

export const FIND_DESCRIPTION =
  `Adds ${REGEXP_NAME} (with its flags) and Ask ${ASSISTANT_NAME} to the options of Edge's Ctrl+F bar. ` +
  `${REGEXP_NAME} highlights every match; Ask sends your question to ${ASSISTANT_NAME}, which opens in its side panel. ` +
  `Live, no restart. Open Edge first.`;
