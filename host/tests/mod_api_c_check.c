/* Compile-only check: the mod SDK header must be valid C99 (built as a C object into mod_manifest_tests). */
#include "gt_mod/gt_mod_api.h"

int gt_mod_c_check(const GtModApi *api, GtMod *self) {
    (void)api;
    (void)self;
    return GT_MOD_API_VERSION == 1 ? 0 : 1;
}
