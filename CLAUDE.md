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

## User decisions, 2026-11-20 (asked to be kept in this file)

- **Decompilation is delegated to another provider's LLM**, which is faster: DeepSeek V4.1 Flash through OpenRouter,
  called with `tools/delegate.sh [-C DIR] [-t TITLE] "<task>"` (opencode, model `openrouter/deepseek/deepseek-v4.1-flash-1017`;
  override with `GT_DELEGATE_MODEL`). This applies to new functions and to structured rewrites, at any time.
  - **The delegate works as Claude would: full access**, like Claude. It runs in the caller's worktree (`-C`), may read
    and edit the repo and run the tools, gates and builds (`opencode run --auto`), and reads `AGENTS.md` like any agent.
    The hard limits are prepended to every task: never merge, never push main, never force-push, the memory cgroup,
    the gate locks, kill only its own processes, no game data in git.
  - **Its result is still checked like Claude's own work:** the caller reads the diff against the asm and makes sure every
    gate passed (fast or synthetic compare, `sabotage` pinned on each function, `frame_audit`, `literal_check`, `sp_loads`,
    `arg_reads`) before committing or opening a PR. A function is decompiled only when its gate says so.
  - **Fast models only write code; Claude verifies (user, 2026-11-20), for the most throughput.** This applies to every
    model the user names as fast; so far that is DeepSeek V4.1 Flash. Its tasks are draft-only: write the functions,
    build, run the static audits (`frame_audit`, `literal_check`, `sp_loads`, `arg_reads`), commit locally, and take the
    next batch at once.
    - It runs **no gates of any kind**: no `tools.decomp_gate` fast, full, synth, coverage or corpus search, no
      sabotage, no in-game checks and no verdicts. It never waits on a lock.
    - Claude (the agents and the main session) reviews the diff against the asm and runs the corpus search, the
      synthetic or fast compare and `sabotage`. Claude drops what fails and opens the PR. Verdicts run on the desktop
      and on the helper laptop.
    - `tools/delegate.sh` prepends this limit to every task. Slow models (Claude itself) keep doing both drafting and
      verification.
  - **Drafting never stops** (user, 2026-11-20: "que se acumule el trabajo si hace falta, pero que la decompilación siga").
    Fast drafters keep writing even while verification lags behind, and the backlog of drafts is fine.
    - `~/repos/gt-ops/draftloop.sh WORKTREE` keeps one delegate session drafting batches of 80 in that worktree, back to
      back. Each batch takes the next free candidates under a lock on the claims files and ships its input shapes.
    - Verification checks the batch branches out in other worktrees, never in a draft loop's worktree.
    - Keep about 5 drafting sessions alive in total across the team.
  - **Batch size depends on the drafter's speed.** The user says which providers are fast and which are slow. Fast
    drafters get **60–80 functions per PR**: the verdict costs about the same hour whatever the batch size, and it is
    the bottleneck. DeepSeek V4.1 Flash is fast (user, 2026-11-20). Slow drafters keep the old 15–25: Claude writing by
    hand is slow. Either way a batch stays within the decomp workflow's ~80.
  - **Tell the user whenever an opencode session is open.** Every session is logged with its start and end in
    `~/repos/gt-ops/out/opencode-sessions.log`; the orchestrating session watches that file and says so in the chat.
  - **If the delegate stops working**, ask the user, and meanwhile do the work with Claude. That covers money running out
    (it is billed per API call), auth, rate limits, network or outage. `tools/delegate.sh` exits 2 with the reason in all
    these cases. When the user says it is back, delegate again.
    Running out of credit also leaves the flag `/tmp/claude-1000/delegate-no-credit`. Every later call then stops at
    once, until the user tops up and the flag is removed. The main session tells the user as soon as the flag appears.
- **How the work is organised (the setup to reuse in future sessions).**
  - **Team: the main session plus three agents.** One agent works on mods, one on structured rewrites and one on
    decompiling new functions (user, 2026-11-19: "tiene que haber 3"). Each agent has its own worktree, and claim
    files in `~/repos/gt-ops/claims/` keep two agents off the same functions. The main session orchestrates: verdicts,
    merges, the README and reports to the user. It may also decompile, and delegate to DeepSeek in its own worktree
    (user, 2026-11-20).
  - **Delegation:** every decomp agent, and the main session, hands drafting to DeepSeek with `tools/delegate.sh`, as above.
  - **Report progress as the `tools.progress` percentage** after each merge, not only as rewrite counts.
  - **Verdicts are serialised, one at a time.** Two at once made every route time out. The ops scripts live outside the
    repo, in `~/repos/gt-ops/`:
    - **Where verdicts run:** while the helper laptop is lent, every verdict runs there (`lqueue2.sh`), and the desktop
      only verifies the agents' and the delegate's batches. Desktop verdicts kept timing out: agents' synthetic runs
      sit in systemd scopes with the same CPU weight as a verdict's routes, so niceness does not protect them.
      Without the laptop, `dqueue.sh` on the desktop takes over.
    - `lqueue2.sh` is the same for the helper laptop: it reads `queue_laptop.txt` (`PR BRANCH`), runs the verdict
      there and lands the PR from the desktop.
    - `dqueue.sh` reads `queue.txt` (`PR BRANCH TREE` per line, editable while it runs). Put PRs that raise the
      percentage first.
    - `vland_locked.sh` runs `origin/main`'s `tools/verdict.sh` under `verdict.lock`, then `landpr.sh`.
    - `landpr.sh` checks pytest and ctest, the gate, the boot check, route timeouts and the known synthetic mismatches
      (FINDINGS #645, issue #682). It then posts the verdict comment, runs `gh pr merge --merge` and deletes the remote
      branch.
    - When `landpr.sh` stops on "real conflict", merge `origin/main` into the branch by hand. Regenerate the README
      counts with `tools.progress` and the map with `tools.progress_map`, check that no source differs from the verdict
      head except what main brought, then comment and merge.
  - **Batch PRs of new functions do not touch the README or the progress map.** Every batch conflicted on them; the main
    session updates them after merges.
  - **When the user takes a resource back, Claude carries on alone** (user, 2026-11-20).
    - **The delegate:** `tools/delegate.sh` exits 2. Claude drafts and verifies itself, in hand-written batches of
      15–25, and tells the user. It goes back to delegating when the user says so.
    - **The helper laptop:** `~/repos/gt-ops/lqueue2.sh` notices after 10 min without an answer. It moves the current
      PR and the rest of `queue_laptop.txt` to the front of the desktop queue and exits. Every verdict then runs on the
      desktop.
    - **Never** wait for either one to come back.
  - **Shared locks.** Agents never wrap `tools.decomp_gate` in `tools/cpu_lock.sh`: that took the locks in the opposite
    order to the verdict and deadlocked it. They set `GT_GATE_CPU_LOCK="<desc>"` instead, and only on the runs that
    are real time (the route gates: `fast`, `full`, route `sabotage`). Synthetic runs (`synth`, `coverage --synth`,
    `sabotage --synth`) are CPU-bound, not paced, and run at nice 19 under `prio.sh`. They take no CPU lock and no
    verdict wait, and run in parallel (user, 2026-11-20: verification was the bottleneck). Before each lock-taking or gate
    job they run `~/repos/gt-ops/wait_verdicts.sh`, so a waiting verdict goes first. It goes ahead anyway after 45 min.
    Keep at most one queued lock job per agent.
  - **Verdicts get CPU priority:** `~/repos/gt-ops/prio.sh` runs all the time and every 30 s renices to 19 every heavy
    process of this project that is not in a `gt-verify*` worktree. That covers builds, gates and delegate sessions.
    Without it, verdict routes timed out under the load of five delegate sessions and three agents.
  - **Stopping a delegate session:** `tools/delegate.sh` runs each session on its own server (`--standalone`), so
    killing the script ends the session. Sessions started on the shared `opencode serve` service kept running after
    their client was killed. List them with `opencode session list` and stop them with `opencode session delete <id>`.
  - **Never edit a running script in place.** Bash reads a script while it runs, so an in-place edit broke
    `tools/delegate.sh` at the end of live sessions. Write a new file and `mv` it over the old one.
  - **Waiting.** Use `~/repos/gt-ops/job.sh NAME CMD…` and `waitjob.sh NAME`, never `pgrep -f` loops. Kill only your own
    PIDs. The Bash tool is zsh, so put loops inside `bash -c`.
  - **No wait may be unbounded** (user, 2026-11-20), whether it is Claude's, an agent's or the delegate's. Every wait
    loop has a limit: `wait_verdicts.sh` 45 min, `cpu_lock.sh` 30 min, `tools/delegate.sh` 4 h. The main session runs
    `~/repos/gt-ops/stalls.sh` every 30 min or so and acts on each line it prints:
    - a wait loop or lock wait older than 45 min;
    - two `decomp_gate` runs with the same tag (the delegate did this, and their logs collide);
    - an opencode session whose worktree has not changed in 45 min.

    Kill orphaned waits (a wait for a `.done` of a job that was killed). Restart or finish stuck work. Tell the user what
    was stuck and why.
  - **Stalls.** When load is near 0 and a verdict does not move, read `/proc/<pid>/wchan`
    (`locks_lock_inode_wait`) and the holders of `/tmp/claude-1000/gt-heavy*.lock` and `cpu-heavy.lock`. When the load is
    high, find which session's processes cause it and ask that session to cap them.
