# Routes from a finished save — implementation plan

**Spec:** `docs/superpowers/specs/2026-10-23-savedata-routes-design.md` (approved by the user 2026-10-23: "me parece
bien el spec, continúa con todo" — also their go-ahead to execute this plan inline).

## Global constraints

- No game data, game key or KIRK key in git; the save lives in `game_files/`, generated keys under `build/`.
- No GPL code copied (PPSSPP, libkirk are references only).
- `tools/` is TDD with pytest (coverage >= 80 %); host code gets a CTest.
- Heavy runs inside `gt-heavy.slice`; real-time gates under `tools/cpu_lock.sh`.

## Tasks

1. **`tools/savedata.py` — PARAM.SFO reader, KIRK slot keys, reference cipher (pytest).**
   `read_sfo(bytes) -> dict`, `file_hash(sfo, name) -> bytes`, `crypt_mode(sfo) -> int`,
   `kirk_slot_keys(kirk_engine_c_text) -> dict[int, bytes]` (slots 16, 18, 100),
   `decrypt(data, game_key, keys, mode=5) -> bytes`, `mac(data, game_key, keys, mode=5) -> bytes` (AES through the
   `openssl` CLI), CLI `python -m tools.savedata keys OUT.hpp` (C++ header with the three slot keys, checked against
   `config/kirk_slot_keys.sha256`). Tests: synthetic SFO; key parsing on a synthetic keyvault; decrypt/mac
   round-trip properties on synthetic data (encrypt implemented in the test helper from the same primitives);
   real save test skipped without the save (hash equals PARAM.SFO's; game key taken from the env var
   `GT_SAVEDATA_TEST_KEY`, never written in the repo).
2. **`host/psp_savedata_crypto.{hpp,cpp}` + CTest `savedata_crypto`.** Same algorithm in C++ on libcrypto; the
   test runs the known-answer vectors produced by task 1's reference on synthetic data (pinned in the test),
   including a flipped byte failing the hash. CMake generates the key header at configure with task 1's CLI.
3. **HLE: decrypt on load.** `execute_savedata_operation` loads: if the save directory has `PARAM.SFO` with an
   encrypted mode and the file is in `SAVEDATA_FILE_LIST`, decrypt with the game's key (param + 0x5DC, secure
   version at + 0x5EC), check the hash, give the game the plain bytes; hash failure → the firmware's broken-data
   error. Boot check: the save reaches the main menu (Day 25); a corrupted copy gets the error, no crash.
4. **The save as a gate input.** `config/savedata.sha1`; `tools.decomp_gate` copies `game_files/UCES01245-GAMEDAT`
   into each `save_*` route's savedata dir, refuses without it or with other hashes (pytest).
5. **Save routes.** Measure the save's page offsets (`GT_IO_LOG` + frames), write a wide menu-tour route, measure
   with `tools.route_reach`, add routes by gain; fast + full gate, raise the floor. Docs: FACTS, FINDINGS, ROADMAP,
   AGENTS (save as an input), `docs/COMPILAR.md` (libcrypto).
