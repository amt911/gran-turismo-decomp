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

typedef struct GtModCtx GtModCtx; /* the guest CPU/memory state a hook runs against */
typedef struct GtMod GtMod;       /* one loaded mod (defined by the loader) */

typedef void (*GtHookFn)(GtModCtx *ctx);

/* v2. A vblank tick: runs once per game vblank on the guest thread that reaches the vblank wait first, with every other guest
 * thread suspended at a call boundary (cooperative scheduling). Only here may a mod use `call_guest`. */
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
     * runtime is stopped, or when the arguments are bad. The callee must not block (a thread switch stops the runtime) and runs
     * while other guest threads are suspended mid-flow: call only setters that are safe between any two game calls. */
    int (*call_guest)(GtModCtx *ctx, uint32_t address, const uint32_t *args, uint32_t nargs, uint32_t *v0_out);
} GtModApi;

/* Entry point every mod exports. Returns 0 on success; non-zero makes the loader skip the mod. */
int gt_mod_init(const GtModApi *api, GtMod *self);

#ifdef __cplusplus
}
#endif

#endif /* GT_MOD_API_H */
