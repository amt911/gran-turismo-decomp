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
  - **Claude drafts the same way** (user, 2026-11-20: "con Claude también la misma dinámica… sin mirar, que decompile
    rápido"). When the delegate is stopped or out of credit, the same loops draft with Claude:
    `GT_DELEGATE_ENGINE=claude` (model `GT_DELEGATE_CLAUDE_MODEL`, default `sonnet`) makes `tools/delegate.sh` run
    `claude -p` in the worktree with the same draft-only task. It is draft-only with no review, in batches of 60–80,
    and the scripts verify. Claude's usage limit stops it like an outage (exit 2).
  - **Measured speed, same draft-only prompt (2026-11-21):** Claude Sonnet via `claude -p` took 16 s, DeepSeek V4.1
    Flash via OpenRouter 31–33 s, and via opencode-go 94–165 s. Yesterday's "DeepSeek is faster" came from comparing
    draft-only DeepSeek with Claude agents that also ran gates. In draft-only mode Claude is faster (71 functions in
    4 min), so the user chose Claude as the drafter. DeepSeek (`GT_DELEGATE_MODEL`) stays as a fallback when the
    Claude quota is short.
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
- **Pipeline architecture: scripts, not agents** (user, 2026-11-20: "maximizar velocidad y minimizar tokens y uso de
  Claude"). Mechanical decomp work runs as shell scripts in `~/repos/gt-ops/`, not as Claude subagents. The agents cost
  600k–900k tokens each and waited between turns; the scripts cost no Claude tokens and run in parallel.
  - **Drafting:** `draftloop.sh WORKTREE` (`draftloop2.sh` with `POOL=` and `BATCH=`) runs one delegate session after
    another. Each session drafts a batch of free candidates, with input shapes, and commits it locally. A loop stops on
    exit 2 or when the pool is empty.
  - **Verifying:** `vloop.sh` (main-session batches) and `vloopg.sh` (`gt-bN` batches) build, run the synthetic
    compare with the batch's shapes, and run `sabotage --synth`. They keep only functions that compared with 0
    mismatches and were pinned, and drop the rest without trying to fix them. `mprune.py` prunes the file;
    `commitbatch.sh` and the loops commit it, push it, open the PR and queue its verdict.
  - **Landing:** `lqueue2.sh` (helper laptop) and `dqueue.sh` (desktop) run the verdicts one at a time and merge the
    clean ones.
  - **Keeping it moving:** `prio.sh` gives verdicts CPU priority, `stalls.sh` finds stuck waits, `wait_verdicts.sh`
    bounds waits, and `mkwt.sh` creates prebuilt-library worktrees.
  - **Claude's role:** the main session supervises and handles exceptions only: stalls, merge conflicts, broken
    scripts, the pool running out, and reports to the user. It starts subagents only for work that is not mechanical
    (structured rewrites that need judgement, mods, investigating a fault), and only when the user asks for them.
  - **Grouped PRs** (user, 2026-11-20 night: "aplica los cambios"). Verified single batches are grouped into PRs of
    about 80 functions, so one verdict covers several batches. `groupprs.sh NAME PR...` builds the combined branch from
    main, unions `config/synth_shapes.txt`, refuses duplicate registrations, builds, and opens the group PR. It then
    closes the absorbed PRs with a pointer and swaps them in `queue.txt`. `grouploop.sh` does this every 20 min with
    the singles waiting in the queue.
  - **Reachability probe before drafting** (same decision). `probe.sh` registers an empty stub for each free candidate
    and runs the synthetic scenes. A stub that gets compared means the gate can verify a real draft of that function.
    The draft loops (`draftloop3.sh`, `POOL=drafts/reach_cands.txt` built by `mkreach.sh`) take only reachable
    candidates; the unreachable ones wait in `drafts/unreach_pool.txt` for new scenes or Phase B. This cuts the
    drafting spent on functions the gate can never compare (about two thirds before).
  - **Nothing a stopped producer leaves is lost.** `vauto.sh` verifies every finished draft branch. Draft commits that
    a prune rewrote are pinned under `refs/drafts/*`, and `drafts/all_drafts_index.txt` maps each drafted function to a
    commit. A failed or timed-out gate run is retried, never counted as "nothing compared".
  - **The bottleneck is the verdict:** about 1.5–2 h per PR, one at a time per machine. More functions per PR, up to
    the decomp workflow's ~80, or another machine, raise throughput; more drafting does not.
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

## Modes (user, 2026-11-21: "pon otra entrada que sea para decir todos los modos existentes")

Said by the user in the chat; each stays on until another mode or "normal mode" is said. The definitions of the first
four live in `AGENTS.md` § Mode switch; "modo eficiente" is defined below.

| Mode | What changes |
| --- | --- |
| **normal mode** (default) | superpowers on; at most 1 implementation agent at a time, never above Sonnet |
| **lite mode** | superpowers off |
| **modo desatendido** | user away: decide alone; push own feature branches and open PRs; merges stay as allowed in the user decisions above |
| **modo pocos tokens** | same quality and gates, fewer Claude tokens: no subagents, terse, batched calls |
| **modo eficiente** | maximum decompilation for the fewest Claude tokens, speed second (below) |

## "modo eficiente" (user, 2026-11-21)

The user asked for "la decompilación a tope", unattended, with Claude tokens kept to a minimum and speed second. The
architecture is:
- **Drafting:** at most 5 opencode-go DeepSeek V4.1 Flash sessions write the code, never Claude.
  - Each session is a `~/repos/gt-ops/draftloop5.sh WORKTREE` loop in its own built worktree (`gt-c1..c3`, `gt-d1`,
    `gt-d2`). `tools/delegate.sh` defaults to `opencode-go/deepseek-v4.1-flash`.
  - A batch is at most 30 functions and about 6 KB, smallest first, from `drafts/res_cands.txt`. That pool is every
    emitted function not on main, not in an open PR and not waiting for verification.
  - When an old draft exists (`drafts/old_draft_commit.txt`), the session starts from it.
  - Input shapes are required, because most dropped functions were never compared for lack of valid inputs.
  - Sessions skip setjmp-like functions.
  - Measured speed: about 0.4 functions per minute per session, against Claude's 1–2.4. That is accepted to save the
    Claude quota.
- **Everything after drafting runs as scripts, with no Claude tokens:**
  - `vauto.sh` verifies, `retryloop.sh` retries failed gate runs, and `unclaim.sh` gives back what a session skipped
    (a function skipped twice is parked in `drafts/hard_pool.txt`).
  - `grouploop.sh` groups singles into PRs of about 80 functions.
  - `dqueue.sh` runs verdicts on the desktop and `lqueue2.sh` on the helper laptop when it is lent. `pauser.sh`
    freezes pipeline work while a verdict runs its real-time routes.
  - `requeue.sh` re-queues infrastructure failures, and `landpr.sh` merges only clean verdicts (no route exit 3 or more,
    no `FLOOR:`, only the known synthetic mismatches).
- **Claude:**
  - The main session only handles exceptions, with long monitor intervals.
  - It runs at most 1 subagent (as `AGENTS.md` says), and only for work that is not mechanical, such as bisecting a
    verdict failure. Usually it runs none.
  - Claude never drafts in this mode.
- **Faster verdicts without lowering quality** (user, 2026-11-22: "que no bajen la calidad, muy importante").
  - Verified batches are grouped into PRs of about 250 functions (`grouploop.sh`, which also regroups the earlier
    groups). This is an exception to the decomp workflow's ~80; a failing group is bisected by hand.
  - The verdict build uses a pinned ccache (`tools/setup.sh`, PR #775). Units whose content did not change come from
    the cache, so the objects are those of a clean build. A prebuilt recompiled library was not used: the boundaries
    from `src/` change the generated code.
  - The synthetic groups run beside the full gate on their own CPUs (#775).
