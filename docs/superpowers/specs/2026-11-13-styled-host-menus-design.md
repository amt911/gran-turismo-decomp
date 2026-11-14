# Styled host menus (cheat device panel, F9 MODS, F10 settings)

Approved by the user on 2026-11-13 (mockup: Race, Controls capture and F9 MODS boards). Implemented by lcs patch
`0069-styled-host-menus.patch` and `host/ui_row.*`, `host/ui_view.*`.

## Look

- Real anti-aliased text: **Barlow** (titles and values in Barlow Condensed SemiBold / Bold, body in Barlow Regular / Medium; SIL OFL 1.1),
  drawn with **stb_truetype** (public domain). Both are fetched by `tools/setup.sh` at a pinned commit with a sha256
  (`config/ui_assets.sha256`) into `tools/src/fonts` / `tools/src/stb`, never committed. Without the font the UI falls back to the 5x7
  look and logs once; it never crashes.
- The game's colours: near-black navy body (88 %), 1 px light border, rounded corners; **blue** (47,127,232) for the header underline,
  the breadcrumb's last segment and the selected row (4 px bar + translucent fill); **red** (232,56,79) for refusals and the Circle
  icon; ON pill green, OFF pill grey, UPDATE / INSTALL pill amber, ERROR pill red. Text white / #c9d1e0 / #9aa6bd.
- Rasterized at 2x (1040 x 1024 texture for a 520 x 512 layout) and presented top right at 40 % of the output width (never below 520 px,
  never above the texture), with linear filtering, on the SDL, Vulkan and DX12 paths.

## Layout

Header: `CHEAT DEVICE` (condensed bold, letter-spaced), breadcrumb `Cheats > Race`, `3 / 6` at the right; F9 / F10: the title and a tab
strip `SETTINGS  MODS`. Rows 44 px: label left, value right by kind (toggle pill, `< choice >`, bar + `70%`, binding chip, chevron,
plain text, coloured pill) and an optional small grey hint line. A refusal is a red message box for 3 s under the header. Footer: drawn
pad glyphs (Select, Back, Change) and the open binding right-aligned (`SELECT + START . F7`); while capturing, "Press the new
combination" and "hold to cancel" with the red Circle.

## Data

`lcs::HostSettingsView` gains `styled`, `items[]` (`HostViewRow`: kind, tone, on, percent, label, value, hint), `position`, `count`,
`crumb`, `message`, `message_is_error`, `binding`, `capturing`, `tabs`, `tab`; `HostExtraPage` gains `item` for structured list rows.
Views that do not fill them (the F10 page) are parsed from their text rows. The old text rows stay filled for the fallback.
Mods do not change: `host/ui_row.cpp` infers the kind of a v4 entry from its text (`ON` / `OFF` toggle, `NN%` bar, any other value of
an adjustable row a choice; mod-list rows: toggle, ERROR pill with the reason as hint, store states as pills with the version as hint).

## Verification

lcs test modes `styled` (blue underline / bar, green and grey pills, red message box, F10 text rows as pills, structured extra page) and
`styled_nofont` (5x7 fallback); host `ui_row` and `cheat_device` tests (kinds, chips, message, position); `ui_panel_dump` writes the
screenshots. Each colour / kind check was sabotaged and seen to fail (PR description).
