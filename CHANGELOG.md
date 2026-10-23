# Changelog

One section per release, newest first, headed `## <version>` to match
`<Version>` in `app/panel-winui/Directory.Build.props`. `tools/Deploy-Store.ps1`
sends the section's text to the Microsoft Store as the release's
"What's new", so write it for users: plain sentences, no file names, at most
1500 characters.

## 0.1.0

The first release.

- Split a tab into any grid of panes, side by side or stacked, each with its own address bar.
- Move between panes with Shift + Alt and an arrow key.
- Use one Edge theme when Windows is light and another when it is dark, switched live.
- Cut-corner tabs, and settings pages that follow the theme.
- Hide Edge's developer-mode prompt.
- Let sites ask again to handle email and calendar links, like Chrome.
- Search a page with a regular expression, or ask Copilot, from Edge's own find bar.
- Edge+ updates itself from the Microsoft Store while its window is closed.
