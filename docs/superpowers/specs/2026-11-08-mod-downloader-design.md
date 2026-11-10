# Mod downloader — get mods from inside the game (Dusklight style)

**Status:** the user approved the direction on 2026-11-08 and delegated the choices: "pon el repo público, no me importa, o si
quieres repo público con todo compilado, o el mismo incluso en releases, lo que veas mejor". Decisions marked *(Claude)*.

## Intent

Today a mod is installed by building `gt-mods` and copying each `.so` + `mod.toml` into `<build>/mods/`. The user wants what
Dusklight does: browse the available mods from inside the game, install or update one with a button, no manual copying.

## Decisions *(Claude)*

- **Source: GitHub Releases of `amt911/gt-mods`** (public since 2026-11-08), not a second repo. One release per tag `vYYYY.MM.DD[.N]`.
  Assets: one archive per mod and platform, `<id>-<version>-linux-x86_64.tar.gz` (folder `<id>/` with `mod.toml` + the `.so`),
  and `index.json` listing every mod: `id`, `name`, `version`, `api`, `description`, per-platform `url`, `sha256`, `size`.
  Why: no token, no second repo to keep in sync, and the release is the same commit the source shows.
- **Built by CI**: a GitHub Actions workflow in `gt-mods` builds every mod against the public `gran-turismo-decomp` headers
  (`include/gt_mod/`, pinned commit) on a tag push, writes `index.json`, and uploads the assets. Linux x86_64 first; Windows,
  Android and arm64 are added as the host runs there (the index already has a per-platform map).
- **Mod version**: `mod.toml` gains `version = "x.y.z"` (missing → `0.0.0`). An installed mod is outdated when the index has
  a newer version for the host's platform.
- **In-game UI**: a second F9 page, **GET MODS**, next to MODS (keyboard and pad, same navigation as MODS). Rows: name,
  installed version / available version, state (`INSTALL`, `UPDATE`, `UP TO DATE`, `DOWNLOADING 45 %`, `ERROR: <reason>`).
  Enter installs or updates. The index is fetched when the page opens (and on Enter on a `REFRESH` row), never at boot.
- **Install**: download to `<mods>/.download/`, check size and `sha256` against the index, unpack to `<mods>/.staging/<id>/`,
  then rename into place. On update, the user's `enabled` and any key in the old `mod.toml` that the new one also has are
  kept; new keys take the new defaults. A mod installed or updated mid-run **takes effect at the next start** (hooks are
  installed at load time only — `docs/MODS.md`); the row says `RESTART TO LOAD`. Uninstall is not in v1 (delete the folder).
- **Networking**: libcurl (system package, already common on Linux; pinned/fetched per `docs/agents/toolchain.md` if not), on a
  worker thread; nothing blocks the game or the renderer. HTTPS only, to `api.github.com` / `github.com` /
  `objects.githubusercontent.com`. Offline or rate-limited → the page says so; the game runs on.
- **Trust**: a downloaded `.so` is native code that runs in the game. v1 trusts the `amt911/gt-mods` releases over HTTPS plus
  the index hash; the index URL is a setting (`GT_MODS_INDEX_URL`) so a fork can point elsewhere. Signing (minisign) is
  noted for later, not in v1.

## Non-goals (v1)

Third-party mod repositories in the UI, uninstall, hot loading without restart, mod dependencies, ratings/screenshots.

## Testing

- Unit (host, ctest): index parsing (good, missing fields, unknown platform), version compare, hash mismatch rejected,
  `mod.toml` merge on update keeps `enabled` and shared keys, staging → rename is atomic (a failed unpack leaves the old mod).
- Integration: a local HTTP server (pytest fixture) serving a fake index + archive: install, update, hash-mismatch refused,
  offline message — driven by `GT_MOD_MENU_TEST_*`-style hooks, no keyboard needed.
- In game: install the real `analog-pedals` release into an empty mods dir, restart, `[mods] loaded: analog-pedals`;
  `mods_together_check.sh` with mods installed only through the downloader. Sabotage: corrupt archive must be refused.
- CI: the workflow's build must produce byte-for-byte the same `index.json` entries as a local `tools/` run of the same
  script (the index generator is a tested Python tool in `gt-mods`).

## Order

1. `gt-mods`: `version` in every `mod.toml`, index generator + CI release workflow, first release.
2. Host: downloader core (fetch, verify, stage, install) + tests.
3. Host: GET MODS page (keyboard + pad) + in-game check.
