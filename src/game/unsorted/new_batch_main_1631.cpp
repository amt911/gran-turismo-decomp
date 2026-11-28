// New batch main 1631 (draft, unverified). Structured C from tools.disasm; no GuestFrame (the function has no stack frame).
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>

namespace gt::game {
namespace new_batch_main_1631 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

// 0x08B72A58: no arguments. The 306 byte stores (one `sb` per byte, the value 4 loaded once into v0) fill the global block
// 0x08D72BB4 .. 0x08D72CE5 with 4; the last delay slot store is the final instruction, so v0 = 4 on the only exit.
void func_08B72A58(Runtime &rt, AllegrexContext &ctx) {
    const u32 value = 4u;
    for (u32 i = 0; i < 306u; ++i) {
        rt.memory().store8(0x08D72BB4u + i, static_cast<std::uint8_t>(value));
    }
    ctx.gpr[2] = value;
}

} // namespace
} // namespace new_batch_main_1631
} // namespace gt::game

#define NEWM(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_1631::name, gt::ReturnKind::kind)
NEWM(0x08B72A58u, func_08B72A58, Word);
