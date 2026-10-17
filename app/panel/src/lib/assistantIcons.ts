// The browser assistants' marks for the assistant dropdown, hand-drawn (no
// icon library). Gemini's four-point spark in its blue-to-violet gradient;
// Copilot's is Edge's own current logo, the rainbow ribbon.
import copilotMark from "@/assets/copilot.png";

export const GEMINI_ICON =
  `<svg viewBox="0 0 24 24" stroke="none">` +
  `<defs><linearGradient id="gemini-spark" x1="4" y1="20" x2="20" y2="4" gradientUnits="userSpaceOnUse">` +
  `<stop offset="0" stop-color="#1c7dff"/><stop offset="0.55" stop-color="#8b6cf0"/><stop offset="1" stop-color="#d9669a"/>` +
  `</linearGradient></defs>` +
  `<path fill="url(#gemini-spark)" d="M12 2 C12.6 7.6 16.4 11.4 22 12 C16.4 12.6 12.6 16.4 12 22 C11.4 16.4 7.6 12.6 2 12 C7.6 11.4 11.4 7.6 12 2 Z"/></svg>`;

export const COPILOT_ICON = `<img src="${copilotMark}" alt="">`;
