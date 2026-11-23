// remix_follow_script.h — the script that applies an open tab's Appearance
// remix again when Edge's mode flips (remix_darkmode.h), through the tab's
// helper (remix_page_helper.h). Pure: strings only.
#pragma once
#include <string>
#include <string_view>
#include "find/web_text.h"
#include "theme/remix/remix_call.h"
#include "theme/remix/remix_colors.h"
#include "theme/remix/remix_page_helper.h"

namespace edgeplus::remix_follow_script {

// FollowModeScript's pieces, around remix_page_helper::InstalledHelper: only a
// page wearing an Appearance remix (applied by any build of the DLL, or by Edge
// alone) is asked.
inline constexpr std::string_view kFollowModeOpening = R"js((() => {
  if (!globalThis.__blueprintParams) { return; }
  )js";
inline constexpr std::string_view kFollowModePush = ".push(";
inline constexpr std::string_view kFollowModeClosing = R"js();
})();)js";

// The script that applies a tab's Appearance remix again for Edge's new mode
// `isTargetDark`, in `pair`'s colour for it, through the tab's helper.
[[nodiscard]] inline std::string FollowModeScript(
    bool isTargetDark, const remix_colors::Pair& pair) {
  std::string script(kFollowModeOpening);
  script += remix_page_helper::InstalledHelper(pair);
  script += kFollowModePush;
  script += isTargetDark ? "true" : "false";
  script += ", ";
  script += web_text::JsStringLiteral(remix_call::kTemplateFunction);
  script += kFollowModeClosing;
  return script;
}

}  // namespace edgeplus::remix_follow_script
