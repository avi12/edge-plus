// The browser assistants' marks for the assistant dropdown, hand-drawn (no
// icon library). Gemini's four-point spark in its blue-to-violet gradient;
// Copilot's rounded ribbon in its blue-to-pink one.

export const GEMINI_ICON =
  `<svg viewBox="0 0 24 24" stroke="none">` +
  `<defs><linearGradient id="gemini-spark" x1="4" y1="20" x2="20" y2="4" gradientUnits="userSpaceOnUse">` +
  `<stop offset="0" stop-color="#1c7dff"/><stop offset="0.55" stop-color="#8b6cf0"/><stop offset="1" stop-color="#d9669a"/>` +
  `</linearGradient></defs>` +
  `<path fill="url(#gemini-spark)" d="M12 2 C12.6 7.6 16.4 11.4 22 12 C16.4 12.6 12.6 16.4 12 22 C11.4 16.4 7.6 12.6 2 12 C7.6 11.4 11.4 7.6 12 2 Z"/></svg>`;

export const COPILOT_ICON =
  `<svg viewBox="0 0 24 24" stroke="none">` +
  `<defs><linearGradient id="copilot-ribbon" x1="3" y1="21" x2="21" y2="3" gradientUnits="userSpaceOnUse">` +
  `<stop offset="0" stop-color="#2f8bff"/><stop offset="0.5" stop-color="#5b5fe0"/><stop offset="1" stop-color="#f06aa8"/>` +
  `</linearGradient></defs>` +
  `<path fill="url(#copilot-ribbon)" d="M12 3 C15 3 17 4.6 17.9 7.4 L20.6 15.6 C21.4 18 20.4 20 18 20 C16.2 20 15 19 14.4 17.2 L12 10 L9.6 17.2 C9 19 7.8 20 6 20 C3.6 20 2.6 18 3.4 15.6 L6.1 7.4 C7 4.6 9 3 12 3 Z"/></svg>`;
