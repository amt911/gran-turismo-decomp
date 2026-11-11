/* Gran Turismo PSP decomp - native mod SDK (API versions 1 and 2).
 *
 * A mod is a shared library that exports `gt_mod_init`. The loader calls it once with the host's function table;
 * the mod registers hooks on guest (PSP) addresses through `api->hook` and reads/writes guest state through the
 * ctx-taking accessors. Valid C99 and C++.
 *
 * Caveat: a hook fires only where the call goes through the runtime's function table. A call that stays inside one
 * recompiled unit (an in-unit goto/direct jump, e.g. 0x08952B68 from 0x0894DD9C in the default --decomp off build)
 * never reaches it. Check a hook point with GT_PROBE=<addr> (hits > 0) in both --decomp off and --decomp on. */
#ifndef GT_MOD_API_H
#define GT_MOD_API_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* GT_MOD_API_VERSION is the BASE version (1): a mod written for v1 keeps comparing `api->version` with it and keeps loading.
 * The host hands each mod a table whose `version` is the `api =` its mod.toml declares (1 or 2), so an unchanged v1 mod
 * (binary or rebuilt) sees 1. A mod that uses the v2 functions declares `api = 2` and checks `api->version >= 2`
 * (GT_MOD_API_VERSION_2); the v2 functions refuse a mod that declared api 1. */
#define GT_MOD_API_VERSION 1
#define GT_MOD_API_VERSION_2 2
/* v3 (docs/superpowers/specs/2026-11-10-named-mod-sdk-design.md): named game state, so mods never hard-code guest addresses.
 * The v3 functions refuse a mod that declared api < 3. */
#define GT_MOD_API_VERSION_3 3

typedef struct GtModCtx GtModCtx; /* the guest CPU/memory state a hook runs against */
typedef struct GtMod GtMod;       /* one loaded mod (defined by the loader) */

typedef void (*GtHookFn)(GtModCtx *ctx);

/* v3: a car of the running race (host/game_state.hpp). `handle` is opaque: pass it back to car_state / car_set_state. */
typedef struct GtCar {
    uint32_t handle;
    uint32_t index; /* the car's slot in the race */
    int32_t role;   /* 1 the live human, 2 an AI car, 3 the human's car in its replay (the replay re-simulates the race; a mod that
                     * changes physics acts there as it did live, or the replayed car leaves the recorded line) */
} GtCar;

/* v3: the body fields the physics step integrates, plus the pedals (FACTS "Car body state"). */
typedef struct GtCarState {
    float position[3];
    float basis[9]; /* forward, right, up rows (left-handed) */
    float speed;    /* m/s */
    float velocity[3];
    float rates[5]; /* angular rates; rates[3] is the roll rate (FACTS "Rollovers") */
    float euler[3];
    float throttle; /* 0..1 */
    float brake;    /* 0..1 */
} GtCarState;

/* v2. A vblank tick: runs once per game vblank on whichever guest thread reaches the vblank wait first (not a fixed "main" thread).
 * Guest threads are scheduled cooperatively on one OS thread, so no other guest thread runs during the tick; but one that is parked
 * (at a thread-switch point, an HLE call) may be in the MIDDLE of a function, a decompiled one included (its fiber is suspended
 * there), and the GE worker is a real OS thread that keeps reading guest memory. Only here may a mod use `call_guest`. */
typedef void (*GtVblankFn)(GtModCtx *ctx);
/* v2. Menu row on the F9 MODS page. `label` writes the row text (NUL-terminated, at most capacity-1 chars) and `activate` is
 * called for Enter (direction 0), Left (-1) and Right (+1). The host calls both from threads other than the game's (the window
 * thread for the keyboard, a guest thread for the pad) and from several threads at once: keep the state in atomics and never touch
 * guest memory or call the game from them. Rows of a mod that is switched off show "<id> mod OFF" and ignore activation. */
typedef void (*GtMenuLabelFn)(GtMod *self, char *out, uint32_t capacity);
typedef void (*GtMenuActivateFn)(GtMod *self, int direction);

typedef struct GtModApi {
    uint32_t version; /* GT_MOD_API_VERSION the host implements */
    /* Several mods may hook one address: the hooks chain in load order, and call_original runs the next active mod's hook
     * (the game's function after the last one). A hook must not assume it is the only one: read the state it needs after
     * call_original returns, never cache what another mod may change. */
    int (*hook)(GtMod *self, uint32_t address, GtHookFn fn);
    void (*call_original)(GtModCtx *ctx);
    uint32_t (*gpr)(GtModCtx *ctx, unsigned reg);
    void (*set_gpr)(GtModCtx *ctx, unsigned reg, uint32_t value);
    uint8_t (*read8)(GtModCtx *ctx, uint32_t address);
    uint16_t (*read16)(GtModCtx *ctx, uint32_t address);
    uint32_t (*read32)(GtModCtx *ctx, uint32_t address);
    float (*readf)(GtModCtx *ctx, uint32_t address);
    void (*write8)(GtModCtx *ctx, uint32_t address, uint8_t value);
    void (*write16)(GtModCtx *ctx, uint32_t address, uint16_t value);
    void (*write32)(GtModCtx *ctx, uint32_t address, uint32_t value);
    void (*writef)(GtModCtx *ctx, uint32_t address, float value);
    const char *(*config)(GtMod *self, const char *key);
    void (*log)(GtMod *self, const char *message);
    /* ---- v2 (appended: the v1 prefix is unchanged). Only valid for a mod that declared `api = 2`; a v1 mod gets a refusal. ---- */
    /* Adds a row to the MODS page (during gt_mod_init only). Returns 0, or non-zero: after init, a null function, an empty or
     * over-long (> 31 chars) id, or the same `id` registered twice by this mod. Rows of several mods coexist, in load order. */
    int (*add_menu_row)(GtMod *self, const char *id, GtMenuLabelFn label, GtMenuActivateFn activate);
    /* Registers this mod's vblank tick (during gt_mod_init only; one per mod). 0 = ok. A switched-off mod's tick is skipped. */
    int (*on_vblank)(GtMod *self, GtVblankFn fn);
    /* Calls the guest function at `address` with `nargs` (<= 8) integer arguments (a0-a3, t0-t3); *v0_out gets its v0 (may be
     * NULL). Allowed ONLY from a vblank tick: returns non-zero and runs nothing from a hook (or any other place), when the
     * runtime is stopped, or when the arguments are bad. The callee must not block (a thread switch stops the runtime), runs on the
     * stack and context copy of the tick's thread, and runs while the other guest threads are parked, possibly in the middle of a
     * function: call only getters, or setters that are safe between any two instructions of the game's own code. */
    int (*call_guest)(GtModCtx *ctx, uint32_t address, const uint32_t *args, uint32_t nargs, uint32_t *v0_out);
    /* Non-zero while the host's F9/F10 overlay is open. gt_play hides the pad from the game then (when v2 mods are installed); a mod
     * that injects input of its own (analog pedals) should stop doing so. Callable from hooks and ticks; any api version. */
    int (*is_menu_open)(GtMod *self);
    /* The game's vblank count as of the latest vblank tick (the tick runs for every installed v2 mod, switched on or off), 0 before
     * the first. A mod that was switched off for a while sees it jump: use it, not the wall clock, to tell a stale state. */
    uint64_t (*vblank_index)(GtMod *self);
    /* ---- v3 (appended): named game state. Only valid for a mod that declared `api = 3`. ---- */
    /* The live balance (Cr). Returns 0 and sets *value, or non-zero when no game is loaded (the host checks the cell). */
    int (*money_get)(GtMod *self, GtModCtx *ctx, uint32_t *value);
    /* Sets the balance (0..999,999,999). Returns 0, or non-zero when no game is loaded or the value is out of range. The game
     * keeps it once it saves. */
    int (*money_set)(GtMod *self, GtModCtx *ctx, uint32_t value);
    /* The cars of the race whose world step is `world` (the a0 of the world-step hook): fills up to `capacity` entries and returns
     * how many there are (the live human, the AI cars and, in a replay, the replayed human as role 3; empty slots are left out). */
    uint32_t (*race_cars)(GtMod *self, GtModCtx *ctx, uint32_t world, GtCar *out, uint32_t capacity);
    /* The live human car of `world`: 0 and *car set when there is exactly one, non-zero otherwise (menus, replays, two humans). */
    int (*player_car)(GtMod *self, GtModCtx *ctx, uint32_t world, uint32_t *car);
    /* Reads / writes a car's state (0 = ok). Write only from a world-step hook or the vblank tick. */
    int (*car_state)(GtMod *self, GtModCtx *ctx, uint32_t car, GtCarState *out);
    int (*car_set_state)(GtMod *self, GtModCtx *ctx, uint32_t car, const GtCarState *state);
} GtModApi;

/* Entry point every mod exports. Returns 0 on success; non-zero makes the loader skip the mod. */
int gt_mod_init(const GtModApi *api, GtMod *self);

#ifdef __cplusplus
}
#endif

#endif /* GT_MOD_API_H */
