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
