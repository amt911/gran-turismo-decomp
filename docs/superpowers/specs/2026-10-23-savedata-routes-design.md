# Routes from a finished save — design

**Status:** proposed 2026-10-23 (the user agreed to the approach in chat the same day: "me parece bien … todo sea
para la decompilación"; this document is what they approve before code).

## Why

The goal is 100 % decompiled, and the differential gate only proves functions a route runs. Today's twelve routes
start from an empty save and reach ~7,900 of ~20,800 functions; every reached function under 300 instructions is
decompiled (2026-10-23 probe), and none of the 233 pending 300-3000-instruction functions is reached. A new game only
sees the first menus and races: championships, licences, special events, most tracks and cars, later pages of records
and the garage stay locked. The user supplied a finished European save (Day 25, 999,999,999 Cr) in
`game_files/UCES01245-GAMEDAT/`. Routes that start from it can reach that code.

## What the spike showed (throwaway, scratch only)

- The save is a real PSP save: `PARAM.SFO` `SAVEDATA_PARAMS` starts with `0x41` (encrypted, hashed), `GAME.DAT` is
  ciphertext. Our `sceUtilitySavedata` HLE hands files over as they are, so GT crashes on it (guest access at 0).
- GT passes a 16-byte key in its savedata parameter block (offset `0x5DC`, `secureVersion` 0 at `0x5EC`) and was
  built with SDK 5.x, so the firmware uses **mode 5**: no console-bound (fuse) step, so any PSP's save decrypts.
- Mode 5, implemented from the algorithm alone (PPSSPP's GPL `sceChnnlsv.cpp` read as a reference, nothing copied):
  a KIRK keystream (AES-128 with KIRK slot keys 18 and 100) seeded by the file's first 16 bytes XOR the game key,
  and a CMAC-style hash (slot 16) that must equal the 16 bytes after the file's name in `SAVEDATA_FILE_LIST`. On the
  user's save the computed hash matched on the first try, and the decrypted `GAME.DAT` loads: the main menu shows Day
  25 and the credits.

## Design

### 1. The savedata HLE decrypts like the firmware (host/)

- On a load of a file listed in the save directory's `PARAM.SFO` with an encrypted `SAVEDATA_PARAMS` mode, the HLE
  decrypts it with the key the game passed (mode from `secureVersion` and the SDK version, as the firmware does;
  only mode 5 and mode 1/3 non-fuse paths are needed, fuse modes 2/4/6 are refused with the firmware's error),
  checks the hash, and gives the game the plain bytes. A wrong hash returns the firmware's "broken data" error, never
  garbage.
- Saves the HLE writes stay as today (plain, `SFOPARAM.BIN`): nothing changes for an empty-save run.
- Code: a small `host/psp_savedata_crypto.{hpp,cpp}` (our own code) on OpenSSL's libcrypto AES (system library,
  Apache-2.0; `pkg-config libcrypto`, already present on this machine; named in the PR and in `docs/COMPILAR.md`).
- KIRK slot keys 16, 18, 100: never committed. `tools/setup.sh` reads them as data from the pinned pspdecrypt
  checkout it already fetches (`libkirk/kirk_engine.c`, its `keyvault` table) into a gitignored generated header
  (`build/`-side), checked against a SHA-256 in `config/`. The game key is never stored: it comes from the game at
  run time.

### 2. The save as an input, verified like the ISO

- `config/savedata.sha1` holds the hashes of the four files of `game_files/UCES01245-GAMEDAT/`.
- The gate copies the folder into each route's own fresh savedata directory before the route starts (routes save
  too; the original is never written).
- Without the save, or with other hashes, the gate refuses to run the save routes with a clear message and the run
  fails: from now on the save is a required input of this project, like the ISO. (The user has it; another dump
  would be another target.)

### 3. Save routes

- New pad routes `config/pad_routes/save_*.pad`, in `ROUTES` like the others (fast and full), each with a header
  saying what it covers. They start with their own first wait: with a save the main menu loads a different page
  than `0x1FA48900` (spike: that seek never came), so the first step is measuring the save's page offsets
  (`GT_IO_LOG` + frame dumps, FACTS § Page loads).
- Which routes: chosen by measurement, not guessed. For each candidate area (championships, licences, special
  events, all tracks/cars, records/garage pages, the Theatre), a probe run (`tools.route_reach`) says how many
  functions it adds; a route is kept when it adds code, in the order of most new functions. The first one is a
  wide menu tour; the rest follow its numbers.
- The floor rises with them (`--raise-floor`), as today.

## Testing

- **Crypto (CTest, no game data):** known-answer vectors made from synthetic plaintext and a synthetic key with the
  slot keys from the generated header: decrypt reproduces the plaintext, the hash matches; one flipped ciphertext
  byte fails the hash (sabotage). The vectors are computed once by an independent implementation (the spike's
  Python, kept in `tools/tests/` as the reference) and pinned.
- **Real save (pytest, skipped without the save):** decrypting `GAME.DAT` gives a hash equal to `PARAM.SFO`'s.
- **HLE:** booting with the save reaches the main menu (frame check in the route's own run); a corrupted copy of the
  save gets the error, not a crash.
- **Gate:** the save routes pass the fast and full gates with 0 mismatches; `tools.route_reach` numbers in the PR.

## Not in scope

- Encrypting the saves we write (only needed to move a save back to a real PSP).
- Fuse-bound modes (2, 4, 6): GT does not use them.
- The US save the user also had: another region's key; not needed.

## Legal

The save, its decrypted form and the game key are the user's data and stay out of git (`game_files/`, scratch,
`build/`). The KIRK keys are read from the already-fetched pspdecrypt checkout, never committed. No GPL code is
copied: PPSSPP and libkirk are references for the algorithm only.
