// reading_mode.h — reading mode's page theme follows Windows light and dark. The
// theme picked for each mode is kept in a file of the DLL's own; the current
// one is set through dom_distiller's own setter, which persists it for later
// reading views and switches the open ones live (docs/re/reading-mode-theme.md).
// While on, a window timer runs the page script in every reading view. UI thread.
#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "channel.h"
#include "config.h"
#include "edge.h"
#include "handles.h"
#include "log.h"
#include "reading_scripts.h"
#include "reading_themes.h"
#include "theme.h"
#include "web_text.h"

namespace edgeplus::reading_mode {

inline constexpr char kChoicesRelativePath[] = "\\EdgePlus\\reading-themes.bin";
inline constexpr char kReadingViewPrefix[] = "read://";
// Often enough that the switch is in the flyout as it opens.
inline constexpr UINT kPollIntervalMs = 400;

// The page script's answer: ok, then the light and the dark pick.
enum class PickField : size_t {
  kTag,
  kLight,
  kDark,
  kCount,
};

inline constinit bool g_isActive = false;
inline constinit reading_themes::Choices g_choices{};

[[nodiscard]] inline reading_themes::Choices LoadChoices() {
  const std::string path = config::LocalAppDataPath(kChoicesRelativePath);
  const handles::UniqueHandle file{
      path.empty()
          ? INVALID_HANDLE_VALUE
          : CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)};
  std::array<uint8_t, reading_themes::kChoicesFileSize> bytes{};
  DWORD read = 0;
  const bool isRead =
      file && ReadFile(file.Get(), bytes.data(),
                       static_cast<DWORD>(bytes.size()), &read, nullptr);
  return isRead ? reading_themes::ChoicesFromBytes(std::span(bytes).first(read))
                : reading_themes::Choices{};
}

inline void SaveChoices(const reading_themes::Choices& choices) {
  const std::string path = config::LocalAppDataPath(kChoicesRelativePath);
  const handles::UniqueHandle file{
      path.empty()
          ? INVALID_HANDLE_VALUE
          : CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                        FILE_ATTRIBUTE_NORMAL, nullptr)};
  if (!file) {
    Log("[reading] could not save the choices");
    return;
  }
  const auto bytes = reading_themes::BytesOf(choices);
  DWORD written = 0;
  WriteFile(file.Get(), bytes.data(), static_cast<DWORD>(bytes.size()),
            &written, nullptr);
}

[[nodiscard]] inline std::vector<void*> ReadingViews() {
  std::vector<void*> views;
  for (void* contents : edge::AllWebContents()) {
    const bool isReadingView =
        edge::IsPageLive(contents) &&
        edge::TabUrl(contents).starts_with(kReadingViewPrefix);
    if (isReadingView) {
      views.push_back(contents);
    }
  }
  return views;
}

inline void ApplyTo(void* browserContext) {
  if (!g_isActive || !browserContext) {
    return;
  }
  const bool isDark = theme::IsOsDarkMode();
  const reading_themes::ReadingTheme theme =
      reading_themes::ForMode(g_choices, isDark);
  edge::SetReadingTheme(browserContext, static_cast<int32_t>(theme));
  Log("[reading] %s -> theme %d", isDark ? "dark" : "light",
      static_cast<int32_t>(theme));
}

inline void OnAnswer(void* browserContext, const edge::ScriptResult& result) {
  if (!g_isActive || !result.isString) {
    return;
  }
  const std::vector<std::string> fields = web_text::SplitFields(result.text);
  const auto field = [&](PickField which) -> const std::string& {
    return fields[static_cast<size_t>(which)];
  };
  const bool isPick = fields.size() == static_cast<size_t>(PickField::kCount) &&
                      field(PickField::kTag) == web_text::kOkTag;
  reading_themes::Choices picked;
  const bool isValid =
      isPick && reading_themes::ChoicesFromFields(
                    field(PickField::kLight), field(PickField::kDark), &picked);
  if (!isValid || picked == g_choices) {
    return;
  }
  const bool isDark = theme::IsOsDarkMode();
  const bool isCurrentModeChanged = reading_themes::ForMode(picked, isDark) !=
                                    reading_themes::ForMode(g_choices, isDark);
  g_choices = picked;
  SaveChoices(picked);
  Log("[reading] picked light %d, dark %d", static_cast<int32_t>(picked.light),
      static_cast<int32_t>(picked.dark));
  if (isCurrentModeChanged) {
    ApplyTo(browserContext);
  }
}

inline void Poll() {
  if (!g_isActive) {
    return;
  }
  const std::string script =
      reading_scripts::DecorateScript(theme::IsOsDarkMode(), g_choices);
  for (void* contents : ReadingViews()) {
    void* browserContext = edge::BrowserContextOf(contents);
    edge::RunScript(contents, script,
                    [browserContext](const edge::ScriptResult& result) {
                      OnAnswer(browserContext, result);
                    });
  }
}

inline void StartPolling(HWND window) {
  SetTimer(window, channel::TimerIdOf(channel::WindowTimer::kReadingPoll),
           kPollIntervalMs, nullptr);
}

inline void Stop(HWND window) {
  KillTimer(window, channel::TimerIdOf(channel::WindowTimer::kReadingPoll));
  const std::string script = reading_scripts::RemoveScript();
  for (void* contents : ReadingViews()) {
    edge::RunScript(contents, script, nullptr);
  }
}

// True when it has just come on (control.h then sets the theme in every window
// and starts the polling).
[[nodiscard]] inline bool ApplyConfig(const config::Wire& wire) {
  const bool wasActive = g_isActive;
  g_isActive = wire.readingThemeActive != 0;
  const bool isTurningOn = !wasActive && g_isActive;
  if (isTurningOn) {
    g_choices = LoadChoices();
  }
  if (wasActive != g_isActive) {
    Log("[reading] follows Windows %s (light %d, dark %d)",
        g_isActive ? "on" : "off", static_cast<int32_t>(g_choices.light),
        static_cast<int32_t>(g_choices.dark));
  }
  return isTurningOn;
}

}  // namespace edgeplus::reading_mode
