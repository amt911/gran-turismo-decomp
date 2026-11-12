# Standalone app (Dusklight-style) — design

**Status:** draft for the user's approval (user, 2026-11-11: "me gustaría que la arquitectura de la app fuera como con
dusklight, que fuera un programa standalone, que pida la iso del juego para el resto de recursos, y que tenga todo con
interfaz de usuario bonita"). Decisions marked *(Claude)* were taken during unattended work; the open questions at the end
are the user's, and one of them (distribution) is a legal call only the user makes.

## Intent

One program the player runs, with no repo, no Python and no terminal. On first start it asks for the player's own game
ISO, checks it is the supported dump, takes what it needs from it, and from then on opens to a polished home screen:
Play, Mods, Cheats, Settings. Everything the player can configure is reachable from a pad or a mouse. Like Dusklight
(Twilight Princess PC), the program carries none of the game's data: the ISO is the player's.

## What exists today (evidence)

- **Getting the game ready takes a clone and four tools:** `tools/setup.sh` (pinned tools), `python -m tools.extract`
  (ISO SHA1 → 7z → pspdecrypt → EBOOT SHA1 → `extracted/`), `python -m tools.recomp` (EBOOT → `recomp/` C++), `cmake --build`
  (docs/COMPILAR.md).
- **The game reads its files from a directory**: `host/main.cpp` maps `disc0:` to `extracted/iso` and loads
  `extracted/EBOOT.elf`; both paths are relative to the working directory.
- **UI today:** lcs-recomp's settings overlay (F10) and our MODS page (F9), text rows drawn by the host renderer; no
  launcher, no file dialog, no fonts of our own.
- **Mods:** loaded from a folder next to the binary or `GT_MODS_DIR`; the downloader (`host/mod_store.cpp`) fetches
  GitHub releases of gt-mods.

## The hard constraint: the recompiled code

The game runs today because PSPRecomp translates the EBOOT to C++ at build time. That translation is game-derived
(AGENTS.md: never committed). **A binary that contains it is game-derived too**, so a downloadable "app" that works on the
ISO alone is only clean once no recompiled code is left (end state 1 of the roadmap). Until then there are three ways, and
the user chose **A** (2026-11-12):

- **A. Build on the player's machine, hidden behind the app** *(chosen by the user, 2026-11-12)*: the released app contains
  only our code (host, decompiled functions, UI, launcher) and the pinned recompiler; on first start, after the ISO is
  verified, it runs the recompile and a compile of the generated code with a bundled toolchain (clang + lld, ~150 MB
  download, pinned), showing progress (recomp peaks at 3.8 GB, AGENTS.md; the time of the recomp and of the generated C++
  build on a player's machine is to be measured in phase 4). The result is cached per ISO hash. Clean, slower first start, heavier download.
- **B. Ship the binary with the recompiled code inside** (what several recomp ports do). Simplest for the player; the
  binary is game-derived: a legal risk the user would be taking on publicly.
- **C. No release until the decomp is complete**; meanwhile the app is built from source (today's flow) and only the
  launcher/UI part below is new.

Everything below holds for all three; only the first-start pipeline (§2) differs.

## Proposal

### 1. One executable, two modes *(Claude)*

**Turismo PC** (name chosen by the user, 2026-11-12; executable `turismo-pc`) is a single
process. It starts in **launcher mode** (UI only, the emulated runtime not started); **Play** starts the runtime in the
same window. Quitting the game closes the program in version 1 (the runtime cannot be torn down and restarted today;
returning to the launcher is a later step).

### 2. First start: the ISO

- **Pick the ISO:** drag and drop it on the window, or a native file dialog (`nativefiledialog-extended`, zlib licence,
  pinned), or type a path.
- **Verify by hash** (SHA1 `c84473…`, as tools/extract), with a progress bar (a 1 GB read). A different dump gets a clear
  message: which dump is supported, and that no other is.
- **Take what is needed:** the ISO9660 contents the game reads (`disc0:`) are read **straight from the ISO** by a small
  ISO9660 reader in the host (our code), so nothing is copied; the EBOOT is decrypted with pspdecrypt run as a **separate
  bundled program** (it is GPL-3.0: run, not linked) into the app's data folder, and its SHA1 checked.
- **Data folder per platform:** Linux `$XDG_DATA_HOME/turismo-pc`, Windows `%APPDATA%\turismo-pc`, Android app storage. Holds the
  ISO path, the decrypted EBOOT, (option A) the build cache, saves, mods, `bindings.toml` and settings.
- The ISO path is remembered; if the file moves, the launcher asks again (hash checked again).

### 3. UI toolkit *(Claude)*

**Dear ImGui** (MIT, pinned, fetched by `tools/setup.sh`) with its SDL2 + Vulkan backends, which match what the host
already uses (no new graphics stack), drawn in the same window and swapchain as the game. A custom theme (dark, rounded
panels, accent colour, large type for TV distance), an open-licence font (Inter or similar, OFL, pinned), icons as a font
(e.g. Material Symbols, Apache-2.0). Pad navigation through ImGui's gamepad nav, mouse and keyboard as usual. The same
toolkit draws the in-game overlay, which replaces the lcs text overlay (F10) and the F9 page (the cheat device of
`2026-11-11-cheat-device-design.md` is one of its screens).

### 4. Screens

- **Home:** game title art is the player's own (taken from the ISO's `ICON0.PNG`/`PIC1.PNG`, never shipped), Play,
  Mods, Cheats, Settings, About.
- **Mods:** installed mods (ON/OFF, version, ERROR with reason, settings from `mod.toml` as typed fields), the store
  (gt-mods releases, `host/mod_store.cpp`: install, update, "restart to load").
- **Cheats:** the cheat device's categories (outside a race, only the ones that apply).
- **Settings:** Graphics (GE path, internal resolution, fullscreen/windowed, vsync, frame-rate cap, draw distance),
  Audio (volume, output device), Controls (pad and keyboard mapping of the PSP buttons, plus the cheat device's hotkeys:
  one bindings screen), Paths (ISO, data folder), Advanced (`--decomp on|off`, perf log).
- **About / legal:** what the program is, that it carries no game data, the licences of what it bundles.

### 5. Phasing *(Claude)*

1. **ImGui in the host** (in-game overlay only): theme, font, pad nav; the cheat device's model drawn with it; F9/F10
   rerouted. Gate: frames with the overlay closed byte-identical to today.
2. **Launcher mode + settings screen**, still started from a build tree (paths resolved from the data folder, falling
   back to `extracted/`).
3. **ISO import pipeline** (§2) with its tests; `extracted/` becomes optional.
4. **Distribution** per the user's answer to question 1 (option A: the bundled toolchain and first-start build).

Each phase is its own PR series with the usual gates (differential gate, boot check, tests); phase 1 also needs the
software renderer path (no ImGui there: headless runs stay as they are).

## Testing

- **Unit:** ISO9660 reader on a synthetic image built in the test (no game data); hash check and error paths; data-folder
  resolution per platform; settings and bindings round trip; launcher state machine (no ISO → pick → verify → ready;
  moved ISO; wrong dump).
- **UI logic** kept out of draw code (view-models), tested without a window; a smoke test opens each screen headless
  (SDL dummy video driver + ImGui with a null renderer) and checks no assertion fires.
- **In game:** the boot check started through the launcher with a scripted "Play"; frames with the overlay closed
  identical to a build without ImGui.

## Non-goals

Bundling any game data or art; online features; a console port (Android follows the same UI later, PSP needs none).

## Open questions (the user's)

Answered 2026-11-12: distribution **A** (build on the player's machine); name **Turismo PC**.

1. Should Play return to the launcher when the game quits (needs a restartable runtime; proposed: later)?
