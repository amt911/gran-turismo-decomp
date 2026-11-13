# Cheat device panel — its own menu, apart from F9 (design)

Status: proposal approved by the user on 2026-11-13 ("Lo demás me vale"), amending
`2026-11-11-cheat-device-design.md` § View.

## Problem

Phase 1 drew the cheat device as a delegated page of the lcs host overlay (F9 / F10). While that overlay is open, lcs
zeroes the real pad (`display_window_input` returns an empty state when `host_settings_open()`), so with a real
controller the device opened by SELECT+START cannot be navigated or closed: the host's pad filter never sees the
D-pad, Cross or Circle. The in-game checks inject the pad below lcs (`GT_PAD`), so they passed. The user also wants
the device to look like a cheat device and to be separate from the F9 menu.

## Design

1. **A second overlay in lcs: the panel** (patch `0068-host-panel.patch`). `host_panel_set(const HostPanel *)`,
   `host_panel_set_open(bool)`, `host_panel_open()`. The panel's view comes from a callback, like the delegated page.
   It is composited through the same paths as the settings overlay (SDL software, Vulkan, DX12), **only when the
   settings overlay is closed**. It does **not** touch input: `display_window_input` keeps returning the real pad and
   keyboard, so the host's filter sees them and hides them from the game (as it already does while the menu is open).
2. **Look** (`rasterize_panel_overlay`, one rasterizer for all paths): a framed box with a coloured title bar (the
   path, e.g. `CHEATS > RACE`), rows with the selected one on a highlight bar and its value right-aligned, and a footer bar with the button hints
   (`X SELECT  O BACK  <> CHANGE` on the pad; the keyboard names when the last input was a key). A refused binding's
   reason replaces the title for 3 s, as today.
3. **Opening and closing**: the action `device.open`, default **SELECT+START** on the pad and **F7** on the keyboard (F8 is lcs's GE trace key)
   (rebindable in Controls). Circle (pad) / Escape or Backspace (keyboard) goes back; at the root it closes.
   F9 goes back to what it was before phase 1: the lcs MODS page (the mods list, update and download rows). F10 stays
   host settings. Opening F9/F10 while the panel is up closes the panel.
4. **Keyboard inside the panel**: arrows, Return / Keypad Enter, Escape / Backspace, read by the host's SDL key poll
   (the same one that reads hotkeys); while the panel is open those keys do not fire hotkeys.
5. **The game while the panel is open** keeps running (as with the GTA cheat device); the pad and the keys are hidden
   from it by the filter. Pausing is not part of this change.

## Verification

- Unit tests: lcs `host_panel` state (open/close, independent of the settings overlay, input not zeroed: a test of the
  decision function), the rasterizer (title bar, highlight row, footer drawn in the expected boxes), the host's
  panel key mapping (keyboard and pad), F7 default and its clash rules.
- In-game routes: `cheats_pad`, `cheats_hotkey`, `cheats_conflict` open the panel with SELECT+START and must log
  `[padmenu] panel opened/closed`; a new check asserts `host_settings_open` stayed false the whole time (the condition
  that broke the real controller) and that the frames while open show the panel box. Sabotage: a build whose SELECT+START
  opens the lcs overlay page instead of the panel (`GT_PANEL_SABOTAGE_USE_OVERLAY`, the phase-1 behaviour that broke the
  real controller) must FAIL that check.
- The real-controller path (SDL GameController) is not reachable from a headless route; the user checks it by hand
  (Xbox One pad over Bluetooth) — written in the PR's manual test plan.
