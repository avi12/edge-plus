// The browser assistants' marks for the assistant dropdown, hand-drawn (no
// icon library). Gemini's four-point spark, each point in one of Google's colours, as Chrome draws it;
// Copilot's is Edge's own current logo, the rainbow ribbon.
import copilotMark from "@/assets/copilot.png";

export const GEMINI_ICON =
  `<svg viewBox="0 0 24 24" stroke="none">` +
  `<path fill="#ea4335" d="M12 12 L9.07 9.07 A10 10 0 0 0 12 2 A10 10 0 0 0 14.93 9.07 Z"/>` +
  `<path fill="#4285f4" d="M12 12 L14.93 9.07 A10 10 0 0 0 22 12 A10 10 0 0 0 14.93 14.93 Z"/>` +
  `<path fill="#34a853" d="M12 12 L14.93 14.93 A10 10 0 0 0 12 22 A10 10 0 0 0 9.07 14.93 Z"/>` +
  `<path fill="#fbbc04" d="M12 12 L9.07 14.93 A10 10 0 0 0 2 12 A10 10 0 0 0 9.07 9.07 Z"/>` +
  `</svg>`;

export const COPILOT_ICON = `<img src="${copilotMark}" alt="">`;
