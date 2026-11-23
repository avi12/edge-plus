// copilot_panel.h — Copilot's side panel: sending it a prompt, showing and
// closing it, and finding its chat page and frame.
#pragma once
#include <cstdint>
#include <cstring>
#include <string_view>
#include <string>
#include "edge/api/binding.h"
#include "edge/api/strings.h"
#include "edge/api/web_contents.h"
#include "edge/offsets.h"

namespace edgeplus::edge {

// Send `prompt` (with `pageText` as the page it is about) to Copilot's side
// panel of `browser`, the way the page's Ask Copilot menu does: the panel opens
// and the prompt goes into it.
inline void SendPromptToSidePanel(void* browser, const std::string& text,
                                  const std::string& title) {
  const StdStringView page = MakeStdStringView(text);
  const StdStringView asked = MakeStdStringView(title);
  static const std::string kEmpty;
  const StdStringView noContext = MakeStdStringView(kEmpty);
  const StdStringView noAccount = MakeStdStringView(kEmpty);
  using Fn = void(__fastcall*)(void*, const void*, const void*, const void*,
                               const void*, bool);
  Callable<Fn>(edge155::rva::CopilotUtils_SendPageContextualPromptToChatImpl)(
      browser, &page, &noContext, &asked, &noAccount, false);
}

// Hand `text` (as the page the question is about) and `title` (the prompt) to
// the Copilot chat page in `frame`, as SendPageContextualPromptToChat does after
// it has opened the side panel, but without opening it: the chat page lives on
// hidden once it has been opened. False when Edge refused (the frame is not live
// or active).
inline bool AskChatFrame(void* frame, const std::string& text,
                         const std::string& title) {
  using ConstructFn = void(__fastcall*)(void*);
  using AssignFn = void*(__fastcall*)(void*, const void*);
  using DispatchFn = bool(__fastcall*)(void*, const void*);
  alignas(16) uint8_t request[edge155::off::ASKREQUEST_size] = {};
  constexpr char kTextType[] = "text";
  constexpr char kEmpty[] = "";
  const StdStringView prompt = MakeStdStringView(title);
  const StdStringView page = MakeStdStringView(text);
  Callable<ConstructFn>(edge155::rva::AskCopilotRequest_ctor)(request);
  Callable<AssignFn>(edge155::rva::StdString_assign)(request, &prompt);
  std::memcpy(request + edge155::off::ASKREQUEST_type, kTextType,
              sizeof(kTextType));
  request[edge155::off::ASKREQUEST_type + edge155::off::U16STRING_short_size] =
      sizeof(kTextType) - 1;
  request[edge155::off::ASKREQUEST_flag] = 0;
  request[edge155::off::ASKREQUEST_flag + 1] = 0;
  Callable<AssignFn>(edge155::rva::OptionalString_assignString)(
      request + edge155::off::ASKREQUEST_text, &page);
  Callable<AssignFn>(edge155::rva::OptionalString_assignEmpty)(
      request + edge155::off::ASKREQUEST_trailing, kEmpty);
  const bool isDelivered = Callable<DispatchFn>(
      edge155::rva::IframelessPromptDispatcher_DispatchAskCopilotToDocument)(
      frame, request);
  Callable<ConstructFn>(edge155::rva::AskCopilotRequest_dtor)(request);
  return isDelivered;
}

inline bool IsCopilotPanelVisible(void* browser) {
  using Fn = bool(__fastcall*)(void*);
  return Callable<Fn>(edge155::rva::CopilotUtils_IsUndersideChatVisible)(
      browser);
}

// Have Edge create and load the Copilot chat page of `browser`'s side panel
// without showing the panel (Edge's own front-load). The page appears among
// AllWebContents a moment later, hidden, and takes questions by AskChatFrame.
inline void FrontLoadCopilotChat(void* browser) {
  struct WeakPointer {
    void* reference = nullptr;
    void* target = nullptr;
  };
  WeakPointer weakBrowser;
  using GetWeakFn = void*(__fastcall*)(void*, WeakPointer*);
  using FrontLoadFn = void(__fastcall*)(WeakPointer*);
  Callable<GetWeakFn>(edge155::rva::Browser_GetWeakPtr)(browser, &weakBrowser);
  Callable<FrontLoadFn>(edge155::rva::CopilotUtils_FrontLoadSidePaneImmediate)(
      &weakBrowser);
}

inline void CloseCopilotPanel(void* browser) {
  using Fn = void(__fastcall*)(void*, bool);
  Callable<Fn>(edge155::rva::CopilotUtils_CloseUndersideChat)(browser, true);
}

// Where Copilot's chat lives. The panel is either a shell page around an iframe
// of Copilot's web app, or (the iframeless panel, 155.0.4283.62) the web app's
// own WebContents on its Edge-only host.
inline constexpr char kPanelShellUrl[] = "edge://discover-chat-v2/";
inline constexpr char kIframelessPanelUrl[] =
    "https://cr.m365copilotapp.svc.cloud.microsoft/";
inline constexpr std::string_view kChatHosts[] = {
    "copilot.microsoft.com", "cr.m365copilotapp.svc.cloud.microsoft"};

// The page of Copilot's side panel, or null while it is not open.
inline void* CopilotSidePanelContents() {
  for (void* contents : AllWebContents()) {
    const std::string url = IsPageLive(contents) ? TabUrl(contents) : "";
    const bool isPanel =
        url.starts_with(kPanelShellUrl) || url.starts_with(kIframelessPanelUrl);
    if (isPanel) {
      return contents;
    }
  }
  return nullptr;
}

// The frame of Copilot's side panel that holds the chat, or null.
inline void* CopilotChatFrame() {
  void* panel = CopilotSidePanelContents();
  if (!panel) {
    return nullptr;
  }
  for (void* frame : FramesOf(panel)) {
    const std::string url = FrameUrl(frame);
    for (const std::string_view host : kChatHosts) {
      if (url.find(host) != std::string::npos) {
        return frame;
      }
    }
  }
  return nullptr;
}

// Whether the chat frame is the iframeless panel's page itself, the only kind
// IframelessPromptDispatcher::DispatchAskCopilotToDocument takes (it refuses a
// frame that is not its page's primary main frame, so the shell's iframe).
inline bool IsChatPageFrame(void* frame) {
  return FrameUrl(frame).starts_with(kIframelessPanelUrl);
}

}  // namespace edgeplus::edge
