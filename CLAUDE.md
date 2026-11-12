# Claude Code instructions

@AGENTS.md

<!-- Canonical instructions live in AGENTS.md: Codex reads it directly, Claude Code through the
import above. Add below only what applies to Claude Code alone. -->

## Claude Code only

- **REA does not open the PSP EBOOT** (MIPS ELFs are rejected by REA 6.2.0 before any provider runs —
  `docs/FINDINGS.md`). The registration below is kept for other artifacts and for when REA adds MIPS;
  the game binary is analysed with the repo-local Ghidra (`AGENTS.md` § Commands).

- **REA MCP for this repo comes from `.mcp.json`** — same `rea-agents@latest` server as the user-scope
  one, plus `GHIDRA_INSTALL_DIR` (default: the repo-local `tools/ghidra/ghidra_12.1.3_PUBLIC`) and `JAVA_HOME`
  (default: `java-27-openjdk`; Ghidra 12.1 needs ≥ 21) so the Ghidra provider is available —
  `rea doctor --provider ghidra` with those values reports it healthy (2026-10-09).
  The first session asks to approve the project server; after approving, restart so the tools
  reconnect. Check with `binary_session` (no arguments): the Ghidra candidate must not say
  `not_configured`. Its output is ~200 KB — read it with `jq`, never inline.
  **Unverified (2026-10-09):** that the approved project server shadows the user-scope `rea` of the same
  name — before approval, `claude mcp get rea` shows only the user one. If Ghidra is still
  `not_configured` after approving, the user-scope registration needs the variable instead
  (`claude mcp remove rea -s user` + `claude mcp add -s user -e GHIDRA_INSTALL_DIR=<dir> rea -- npx -y rea-agents@latest mcp`)
  — that edits user config, so the user runs or approves it. Record the outcome in `docs/FACTS.md`.
- **Load REA tools in one batch** with `ToolSearch` (`select:mcp__rea__open_binary,…`) before using them;
  the `reverse-engineer-anything` skill says which tool fits the question.
- **REA's Ghidra JVM is a child of the MCP server, not of your shell** — a `systemd-run` scope around a
  Bash command does not cap it. Keep one binary open at a time and `close_binary` when done.
- **`/graphify` is a Claude Code slash command** — scope it as `AGENTS.md` § graphify says.
- **"modo pocos tokens"** — low-token mode, defined in `AGENTS.md` § Mode switch (same quality, fewer tokens).

## User decisions, 2026-11-10 (asked to be kept in this file)

- **Decompiled bodies are structured C** — ifs/loops/switch, named typed locals; `G[n]` only at the call boundary.
  No register-level / emitter output (`tools/batch_emit.py` is a reading aid only). The ~1,100 emitter-style functions
  in `src/game/unsorted/hot_batch_*.cpp` are being rewritten as structured C (same address, same gate); the README tracks
  "structured rewrites: N of ~1,100".
- **Readable ≠ named.** Three levels, reported apart: verified correct → structured (readable control flow) → named and
  typed (structs, real names, with evidence). "Decompiled" never means "understood".
- **Mods go through a named SDK, never raw guest addresses** (spec `docs/superpowers/specs/2026-11-10-named-mod-sdk-design.md`,
  PR #533): accessors like `money_get/set`, `player_car`, `car_state`, named hook points, implemented in the host with the
  researched addresses and their evidence. When the emulated memory goes away only the host changes; mods stay.
- **Verdicts:** the synthetic gate is skipped when the PR cannot reach it (`tools/verdict_scope.py`); when it runs, its two
  route groups go one after the other (six CPU-bound synthetic runs at once all timed out). The build is already
  incremental; the time is the two gates.
- **Software renderer is the exact reference, not the play path**: it runs below 60 fps (GE-bound) even on the desktop;
  play with `NativeGE=true`. Not optimised for now.
- **Rendering at the no-emulation phase: a readable API with backends (path B)** — the renderer calls named functions
  (`draw_mesh`, `set_blend`, `set_stencil`…), not GE command words. Requirement for that phase's spec: every call maps
  one-to-one (or to a few commands) onto the PSP GE, so a PSP backend is a direct translation and a gate can compare the
  generated GE lists with the original's; Vulkan (PC, Android) is one backend. No API feature the GE cannot express.
- **Ports:** Android is near (Vulkan + SDL2 already; arm64 build, touch controls, user data on device) and needs no
  finished decomp; PSP needs the full decomp without emulated memory (32 MB RAM, 333 MHz) plus the GE backend above.

## User decisions, 2026-11-11 (asked to be kept in this file)

- **Cheats as a game-style cheat device** (like the GTA Vice City Stories Cheat Device): its own menu overlay opened by a
  button/key combination, with **configurable hotkeys for both keyboard and pad**, rebindable from the menu itself
  (spec `docs/superpowers/specs/2026-11-11-cheat-device-design.md`).
- **Standalone app, Dusklight-style:** a program that runs on its own, asks for the user's game ISO for everything else
  (verifies it, extracts what it needs), and has a polished user interface (launcher, settings, mods)
  (spec `docs/superpowers/specs/2026-11-11-standalone-app-design.md`). **2026-11-12:** distribution **A** — the released
  app carries only our code and recompiles the player's own EBOOT on their machine at first start (nothing game-derived is
  shipped); name **Turismo PC**. The user wants it Dusklight-like (compiled, only asks for the ISO); **every release must carry
  the decompiled code compiled in** (first start recompiles only what is still undecompiled), and at 100 % the first-start
  build goes away.
- **Mods must also act in replays** (rollover, cheats such as nitro): a replay that drops a mod's effect is a bug.

## User decisions, 2026-11-12 (asked to be kept in this file)

- **Helper laptops over SSH use the password, never keys.** The user gives the host and a temporary password in the chat;
  log in with that password every time (an `SSH_ASKPASS` script deleted right after use, or one `ControlMaster` connection
  per session) and **never install an SSH key** on the laptop. The password is read from the conversation when needed and
  **never written to the repo, memory or any file that outlives the command** (the repo is public).
- **Current helper: `andres@192.168.0.133`** (MSI GE63: i7-8750H, 12 threads, 32 GB, Arch), used to run PR verdicts in
  parallel with the desktop: repo in `~/repos/gran-turismo-decomp`, verdict worktree `~/repos/gt-verify` (with the user's
  `game_files/`, hash-checked). Jobs there run detached (`setsid nohup`; the wifi drops) and the `gt-heavy.slice` limits are
  set with `--runtime` (gone after a reboot: set them again). Installing the repo, its tools and builds there is allowed.
