// remix_editor.h — the open Create a remix pages (the blueprint bubble's page
// and its remix pages), and the mode the Appearance editor among them previews,
// read from the address fragment it publishes (remix_colors.h). UI thread.
#pragma once
#include <string>
#include <vector>
#include "edge/edge.h"
#include "theme/followed_pages.h"
#include "theme/remix_colors.h"
#include "theme/theme_tone.h"

namespace edgeplus::remix_editor {

[[nodiscard]] inline std::vector<void*> RemixPages() {
  std::vector<void*> pages;
  for (void* contents : edge::AllWebContents()) {
    const bool isRemixPage =
        edge::IsPageLive(contents) &&
        followed_pages::IsRemixPage(edge::TabUrl(contents));
    if (isRemixPage) {
      pages.push_back(contents);
    }
  }
  return pages;
}

// What the open Appearance editor shows: the mode it previews and the colour
// pair it holds, as of its address now (it publishes both as they change, so
// this is fresher than the DLL's own copy, which a poll saves).
struct Editor {
  theme_tone::Tone preview = theme_tone::Tone::kUnknown;  // none open
  remix_colors::Pair pair;
  bool hasPair = false;
};

[[nodiscard]] inline Editor OpenEditor() {
  for (void* contents : RemixPages()) {
    const std::string url = edge::TabUrl(contents);
    Editor editor;
    editor.preview = remix_colors::PreviewFromUrl(url);
    if (editor.preview != theme_tone::Tone::kUnknown) {
      editor.hasPair = remix_colors::PairFromUrl(url, &editor.pair);
      return editor;
    }
  }
  return {};
}
}  // namespace edgeplus::remix_editor
