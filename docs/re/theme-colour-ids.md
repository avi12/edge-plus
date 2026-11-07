# Theme colour ids and the vertical tabs (156.0.4314.58)

What a theme extension's `colors` keys reach in Edge's vertical tabs, and what
only the DLL can change. Read from the pinned `msedge.dll` with its public PDB
(`03775A411C2857584C4C44205044422E1`) and checked live in a throwaway Edge.

## The manifest key -> `BrowserThemePack` colour id table

A `{const char* key; int id}` array in `.rdata` (file offset `0x123dfcf0` for
`tab_text`); `BrowserThemePack::GetColor` is keyed on these ids
(`offsets.h THEMECOLOR_*` / `VTABCOLOR_*`).

| id | key | id | key |
|---|---|---|---|
| 0x00 | bookmark_text | 0x13 | tab_background_text |
| 0x01 | button_background | 0x14 | tab_background_text_incognito |
| 0x02 | frame | 0x15 | tab_background_text_inactive |
| 0x03 | frame_incognito | 0x16 | tab_background_text_incognito_inactive |
| 0x04 | frame_inactive | 0x17 | toolbar |
| 0x05 | frame_incognito_inactive | 0x18 | toolbar_button_icon |
| 0x08 | ntp_background | 0x19 | toolbar_text |
| 0x09 | ntp_link | 0x23 | vertical_frame |
| 0x0a | ntp_header | 0x24 | vertical_frame_incognito |
| 0x0b | ntp_text | 0x25 | vertical_active_tab |
| 0x0c | omnibox_background | 0x26 | vertical_active_tab_incognito |
| 0x0d | omnibox_text | 0x27 | vertical_background_tab |
| 0x0e | background_tab | 0x28 | vertical_background_tab_incognito |
| 0x0f | background_tab_incognito | 0x29 | vertical_tab_text |
| 0x10 | background_tab_inactive | 0x2a | vertical_tab_text_incognito |
| 0x11 | background_tab_incognito_inactive | 0x2b | vertical_tab_background_text |
| 0x12 | tab_text | 0x2c | vertical_tab_background_text_incognito |
| | | 0x2d / 0x2e | vertical_toolbar_button_icon (/ incognito) |

## A theme cannot set the `vertical_*` keys

For a theme extension, `ThemeService::SetBrowserPackForUnifiedThemeExtension`
calls `BrowserThemePack::EdgeSetAdjustThemePackForVT`, which sets the pack's
byte at `+0xb1`. With it set, `CreateToolbarImageAndColors` calls
`EdgeSetVTabAndRelatedColor` (rva `0x88426b2`), which overwrites 0x23-0x2c
from one colour: the K-means colour of the toolbar image when the theme has
one, else `toolbar` (0x17).

| ids | value |
|---|---|
| 0x23 / 0x24 rail | the toolbar colour |
| 0x25 / 0x26 selected tab | `AlphaBlend(white, toolbar, a)`, a white wash |
| 0x27 / 0x28 idle tab | the toolbar colour |
| 0x29-0x2c texts | white on a dark toolbar, black on a light one |

Live: a theme naming every `vertical_*` key in loud colours painted none of
them. Each pair is a normal / an incognito profile, so Edge does not dim
anything in an unfocused window.

## What paints the tab titles

Every vertical tab's title, the selected one included, is the theme's
`tab_text` (0x12), through the ui ids `0xadb..0xade`.
`tab_background_text` (0x13) does not reach the vertical tabs. It colours only
the horizontal idle tabs. Live: `tab_text` set to `#56feff` and
`tab_background_text` to `#00a6a6` titled all four vertical tabs `#56feff`.

## So, with the Cyberpunk skin

| what | who | how |
|---|---|---|
| rail, toolbar, frame | the theme | `toolbar`, `frame` (the rail follows `toolbar`) |
| idle titles | the DLL, from the theme | `tab_background_text` (`_inactive` when the window is unfocused), when it reads at 4.5:1 on the rail; else Edge's `tab_text` |
| selected plate and its title | the DLL | the skin's red plate and ink (`cyberpunk_skin.h`), dimmed 30% into the rail when the window is unfocused |
| hovered tab | the DLL, from the theme | the idle-title colour washed over the rail (16% dark, 8% light) |

The DLL reads the theme's colours through the unhooked `GetColor`, so a theme
change carries through with no colour written into the DLL except the plate.
