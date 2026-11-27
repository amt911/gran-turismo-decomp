// New batch main 34 (draft): of the 80 addresses this batch was given, 21 are real two-instruction tail thunks and 59 are
// entries of `.sceStub.text`. All are written from `tools.disasm`; the thunks as structured C (one helper per shape).
//
// The 59 stubs (0x08C7A7F8 .. 0x08C7A9E0, 8 bytes apart) are NOT decompiled here. The ELF places them in `.sceStub.text`
// (section vaddr 0x4767F8, after `.text` ends at 0x4767DC), their words are `jr $ra; nop`, and
// recomp/generated/generated_registry.cpp registers every one of them as an HLE import (`import_142` .. `import_328`,
// `IoFileMgrForUser`, `ThreadManForUser`, ...). A decompiled body at those addresses would replace the import implementation
// instead of the two instructions, so it could never match the original run (which calls host code): the stubs stay out,
// like every other `sce*` stub.
//
// 0x08BB1534 is not written either: its whole code is `j 0x08BB1534; nop` (`tools.disasm 08BB1534:2`), a jump to itself, so
// a differential case could only hang. It stays recompiled and unregistered.
//
// The 20 thunks below end in one `j target` whose delay slot runs before the jump. Each has no frame, touches no
// callee-saved register and leaves v0 exactly as the original (the target's value), so nothing is written to it here.
// No names: none has evidence.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace new_batch_main_34 {
namespace {

using namespace gt::game::hot;
using psprecomp::AllegrexContext;
using psprecomp::Runtime;
using u32 = std::uint32_t;

// `j target; nop`: continue in the target with every argument register as it is.
void forward_all(Runtime &, AllegrexContext &ctx, u32 target) { tail_call(ctx, target, {}); }

// `j target; lw a0, offset(a0)`: the member at `offset` replaces `this` (the load is the delay slot, so it happens before
// the target runs); a1-a3 and t0-t3 are untouched.
void forward_member(Runtime &rt, AllegrexContext &ctx, u32 target, u32 offset) {
    tail_call(ctx, target, {word(rt, ctx.gpr[4] + offset)});
}

// `j target; addiu a0, a0, addend`: a0 gets the sub-object at `this + addend`, no load.
void forward_this_plus(Runtime &, AllegrexContext &ctx, u32 target, u32 addend) {
    tail_call(ctx, target, {ctx.gpr[4] + addend});
}

// `j target; addiu a0, zero, value`.
void forward_first_constant(Runtime &, AllegrexContext &ctx, u32 target, u32 value) {
    tail_call(ctx, target, {value});
}

// `j target; addu a1, zero, value` (a0 and a2-a3 as they came in).
void forward_second_constant(Runtime &, AllegrexContext &ctx, u32 target, u32 value) {
    tail_call(ctx, target, {ctx.gpr[4], value});
}

// `j target; addiu a2, zero, value` (a0-a1 as they came in).
void forward_third_constant(Runtime &, AllegrexContext &ctx, u32 target, u32 value) {
    tail_call(ctx, target, {ctx.gpr[4], ctx.gpr[5], value});
}

// 0x088057A0: j 0x08A4D37C; nop
// 0x0885C0D4: j 0x08864A74; lw a0, 4(a0)
// 0x0885F428: j 0x0885F3FC; lw a0, 0(a0)
// 0x08891560: j 0x088910E4; lw a0, 0(a0)
// 0x088A3710: j 0x088A5E5C; addiu a0, a0, 8
// 0x089F0AF4: j 0x08B0B8CC; addiu a0, a0, 0xC
// 0x08A3D710: j 0x08B0B8CC; nop
void func_08A3D710(Runtime &rt, AllegrexContext &ctx) { forward_all(rt, ctx, 0x08B0B8CCu); }
// 0x08A8DB8C: j 0x08A8DAB4; addiu a2, zero, 5
// 0x08ACC0EC: j 0x08ACBC10; lw a0, 0(a0)
// 0x08AD096C: j 0x08B2E9F8; nop
// 0x08B19FEC: j 0x08B19FB0; lw a0, 0(a0)
// 0x08B1A0B4: j 0x08B1A088; lw a0, 0(a0)
void func_08B1A0B4(Runtime &rt, AllegrexContext &ctx) { forward_member(rt, ctx, 0x08B1A088u, 0x0u); }
// 0x08B2E2B8: j func_08C7A988; nop  (0x08C7A988 is a `.sceStub.text` entry: ThreadManForUser import 0xCEADEB47)
// 0x08B2EFF0: j 0x08B599E0; addiu a0, zero, 1
// 0x08B3027C: j 0x08B301C4; nop
// 0x08B35444: j func_08C7A928; nop  (0x08C7A928 is a `.sceStub.text` entry: SysMemUserForUser import 0xF919F628)
// 0x08BBFF7C: j 0x08BBFE68; addiu a0, a0, 0x98
// 0x08C4FD3C: j 0x08C4FC50; nop
void func_08C4FD3C(Runtime &rt, AllegrexContext &ctx) { forward_all(rt, ctx, 0x08C4FC50u); }
// 0x08C501D8: j 0x08C50034; nop
// 0x08C723C0: j 0x08C72194; addu a1, zero, zero
void func_08C723C0(Runtime &rt, AllegrexContext &ctx) { forward_second_constant(rt, ctx, 0x08C72194u, 0x0u); }

} // namespace
} // namespace new_batch_main_34
} // namespace gt::game

#define NEW34(address, name, kind) GT_DECOMP(address, gt::game::new_batch_main_34::name, gt::ReturnKind::kind)
NEW34(0x08A3D710u, func_08A3D710, Word);
NEW34(0x08B1A0B4u, func_08B1A0B4, Word);
NEW34(0x08C4FD3Cu, func_08C4FD3C, Word);
NEW34(0x08C723C0u, func_08C723C0, Word);
