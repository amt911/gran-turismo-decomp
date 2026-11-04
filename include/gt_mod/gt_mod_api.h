/* Gran Turismo PSP decomp - native mod SDK (API version 1).
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

#define GT_MOD_API_VERSION 1

typedef struct GtModCtx GtModCtx; /* the guest CPU/memory state a hook runs against */
typedef struct GtMod GtMod;       /* one loaded mod (defined by the loader) */

typedef void (*GtHookFn)(GtModCtx *ctx);

typedef struct GtModApi {
    uint32_t version; /* GT_MOD_API_VERSION the host implements */
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
} GtModApi;

/* Entry point every mod exports. Returns 0 on success; non-zero makes the loader skip the mod. */
int gt_mod_init(const GtModApi *api, GtMod *self);

#ifdef __cplusplus
}
#endif

#endif /* GT_MOD_API_H */
