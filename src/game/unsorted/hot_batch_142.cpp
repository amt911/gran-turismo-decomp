// Hot batch 142: shapes no route reaches, decompiled once as a parameterised wrapper and instanced for every member of
// their family (rows from tools.family_rows), verified by synthetic cases; written by hand from `tools.disasm`; role
// words describe access shapes, none is a game name.
#include "decomp_registry.hpp"
#include "gt/game/hot_common.hpp"
#include "guest_call.hpp"

#include <cstdint>
#include <initializer_list>

namespace gt::game {
namespace hot_batch_142 {
namespace {

using namespace gt::game::hot;

// A guest call with the argument registers set like the original sets them before the jal (a0-a3 in order): what the
// callee does not take stays as the caller had it; v0 / v1 / f0 of the callee stay in the registers.
inline GuestResult call_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t address,
                            std::initializer_list<std::uint32_t> arguments) {
    unsigned reg = 4u;
    for (const std::uint32_t argument : arguments) {
        if (reg <= 11u) ctx.gpr[reg++] = argument;
    }
    const GuestResult result = call_guest(rt, ctx, address, arguments, {});
    ctx.gpr[2] = result.v0;
    ctx.gpr[3] = result.v1;
    ctx.fpr[0] = result.f0;
    return result;
}

// make_native_method(owner, name..., pair *a2, pair *a3) with the member's base constructor and vtable: a holder h from
// func_08A69B3C(&h) (the name arguments stay in a1-a3), a 0x24-byte object (func_08A5BC20(0x24, 4)) built by
// base(object, &h) and given the vtable, the method pair a2[0] / a2[1] at +0x14 / +0x18 and a3[0] / a3[1] at +0x1C / +0x20
// (the original tests a2[0] against 0x08B867AC and a2[1] against 0, but every path stores the same two words), h
// destroyed (func_08A69D68), the object wrapped (func_08A5D414(&w, &object)) and added to the owner
// (func_08A7C5C8(owner, &w)), w destroyed.
void make_native_method(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t base, std::uint32_t vtable) {
    const std::uint32_t owner = ctx.gpr[4];
    const std::uint32_t method = word(rt, ctx.gpr[6]), adjust = word(rt, ctx.gpr[6] + 4u);
    const std::uint32_t data = word(rt, ctx.gpr[7]), data_adjust = word(rt, ctx.gpr[7] + 4u);
    GuestFrame frame(ctx, 0x40u);
    const std::uint32_t holder = frame.at(8u), wrapped = frame.at(4u), slot = frame.at(0u);
    call_set(rt, ctx, 0x08A69B3Cu, {holder});
    const std::uint32_t object = call_set(rt, ctx, 0x08A5BC20u, {0x24u, 4u}).v0;
    call_set(rt, ctx, base, {object, holder});
    put(rt, object, vtable);
    put(rt, object + 0x18u, adjust);
    put(rt, object + 0x14u, method);
    put(rt, object + 0x20u, data_adjust);
    put(rt, object + 0x1Cu, data);
    call_set(rt, ctx, 0x08A69D68u, {holder, method, 0x08B80000u});  // a1 / a2 as the original leaves them (the method,
                                                                      // the `lui` of 0x08B867AC)
    put(rt, slot, object);
    call_set(rt, ctx, 0x08A5D414u, {wrapped, slot});
    call_set(rt, ctx, 0x08A7C5C8u, {owner, wrapped});
    call_set(rt, ctx, 0x08A5B9ECu, {wrapped});
}

// reset_list(a0-a3 as they came) with the member's getter: s = get(...); func_08BE9B60(s, s->+8); then the list at s is
// empty: +0x10 and +0xC = s + 4, +0x14 and +8 = 0. Returns s + 4.
void reset_list(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t get) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t list = call_set(rt, ctx, get, {}).v0;
    call_set(rt, ctx, 0x08BE9B60u, {list, word(rt, list + 8u)});
    put(rt, list + 0x10u, list + 4u);
    put(rt, list + 0x14u, 0u);
    put(rt, list + 0xCu, list + 4u);
    put(rt, list + 8u, 0u);
    ctx.gpr[2] = list + 4u;
}

// reset_member(this) with the member's constants: *this = first; destroy(this + offset); *this = second (left in v0).
void reset_member(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t first, std::uint32_t destroy,
                  std::uint32_t offset, std::uint32_t second) {
    const std::uint32_t self = ctx.gpr[4];
    put(rt, self, first);
    {
        GuestFrame frame(ctx, 0x10u);
        call_set(rt, ctx, destroy, {self + offset});
    }
    put(rt, self, second);
    ctx.gpr[2] = second;
}

// tag(this, value) with the member's kind and vtable: byte +4 = kind, +8 = value, the rest of the +4 word cleared,
// *this = vtable (v0); v1 keeps the +4 word (the kind).
void tag(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t kind, std::uint32_t vtable) {
    const std::uint32_t self = ctx.gpr[4];
    rt.memory().store8(self + 4u, static_cast<std::uint8_t>(kind));
    const std::uint32_t low = word(rt, self + 4u) & 0xFFu;
    put(rt, self + 8u, ctx.gpr[5]);
    put(rt, self + 4u, low);
    put(rt, self, vtable);
    ctx.gpr[2] = vtable;
    ctx.gpr[3] = low;
}

} // namespace

// Template of its family (77 more members, batch 142 rows below).
void func_08BE9F78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCDD20u); }

void func_08BEA6B8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A98220u, 0x08CCDA50u); }
void func_08BEAC80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCDD68u); }
void func_08BECC54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCDE50u); }
void func_08BED1F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCDE98u); }
void func_08BEE3F8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCE068u); }
void func_08BF218C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCE2A0u); }
void func_08BF2FFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCE3C0u); }
void func_08BF3198(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCE378u); }
void func_08BF4A18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCE730u); }
void func_08BF4F84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCE778u); }
void func_08BF7E0C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A98220u, 0x08CCE9E8u); }
void func_08BFB53C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCEAE0u); }
void func_08BFCB60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCED20u); }
void func_08BFCCFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCECD8u); }
void func_08BFCE98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCEC48u); }
void func_08BFD034(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCEB28u); }
void func_08BFE1E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCEEC8u); }
void func_08BFE330(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A98220u, 0x08CCEE80u); }
void func_08BFF920(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A98220u, 0x08CCEFB0u); }
void func_08C03234(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A98220u, 0x08CCF290u); }
void func_08C033D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCF320u); }
void func_08C04814(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCF5C8u); }
void func_08C0524C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCF610u); }
void func_08C06970(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCF730u); }
void func_08C0860C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCF790u); }
void func_08C09F08(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCF9E8u); }
void func_08C0C650(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCFCF8u); }
void func_08C0C8F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCFD40u); }
void func_08C0FDFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCFEB0u); }
void func_08C0FF98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCFE68u); }
void func_08C107D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A98220u, 0x08CCFF40u); }
void func_08C10C90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CCFFD8u); }
void func_08C11030(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD0020u); }
void func_08C14234(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A98220u, 0x08CD0230u); }
void func_08C14388(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A98220u, 0x08CD01E8u); }
void func_08C144DC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A98220u, 0x08CD01A0u); }
void func_08C14F9C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD0278u); }
void func_08C1E268(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD2120u); }
void func_08C264F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD3D68u); }
void func_08C26CD4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD4038u); }
void func_08C26E70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD3FF0u); }
void func_08C2700C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD3FA8u); }
void func_08C271A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD3F60u); }
void func_08C27344(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD3F18u); }
void func_08C274E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD3ED0u); }
void func_08C2767C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD3E88u); }
void func_08C27818(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD3E40u); }
void func_08C279B4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD3DF8u); }
void func_08C27B50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD3DB0u); }
void func_08C2D27C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD44A8u); }
void func_08C2D418(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD4460u); }
void func_08C2F3B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD4848u); }
void func_08C2F6A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD47B8u); }
void func_08C2F840(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD4770u); }
void func_08C387F4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD5AD0u); }
void func_08C38990(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD5A88u); }
void func_08C38B2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD5A40u); }
void func_08C38CC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD59F8u); }
void func_08C38E64(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD59B0u); }
void func_08C39000(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD5968u); }
void func_08C3919C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD5920u); }
void func_08C39338(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD58D8u); }
void func_08C394D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD5890u); }
void func_08C39670(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD5848u); }
void func_08C3980C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD5800u); }
void func_08C399A8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD57B8u); }
void func_08C3A928(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD5DE8u); }
void func_08C3AAC4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD5DA0u); }
void func_08C3AC60(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD5D58u); }
void func_08C3ADFC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD5D10u); }
void func_08C3AF98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD5CC8u); }
void func_08C3B134(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD5C80u); }
void func_08C3B2D0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD5C38u); }
void func_08C3B46C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD5BF0u); }
void func_08C3B608(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD5BA8u); }
void func_08C3B7A4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD5B60u); }
void func_08C3B940(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { make_native_method(rt, ctx, 0x08A80214u, 0x08CD5B18u); }

void func_0896BA78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08BE9C08u); }
void func_08898F78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08CC2020u, 0x088978B4u, 32u, 0x08CC3E60u); }
void func_08A65288(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag(rt, ctx, 6u, 0x08CD2980u); }

void func_0897520C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08BECDA4u); }
void func_089752B0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08BECE24u); }
void func_0897FC88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08BEEFA0u); }
void func_08984314(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08BEF7E8u); }
void func_0898AACC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08BF0630u); }
void func_0898FFC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08BF08A8u); }
void func_08994230(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08BF09B0u); }
void func_08995A14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08BF0A98u); }
void func_08997908(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08BF19D4u); }
void func_0899A7D4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08BF3384u); }
void func_0899B1C0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08BF37C0u); }
void func_0899E1E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08BF4318u); }
void func_089A15C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08BF4B68u); }
void func_089A1668(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08BF4BE8u); }
void func_089A4C58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08BF6DC0u); }
void func_089B3A58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08BFD278u); }
void func_089B7334(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08BFE51Cu); }
void func_089B9CBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08BFE5F4u); }
void func_089BCC98(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08BFF624u); }
void func_089C067C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08BFFCE4u); }
void func_089C2CE8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C00754u); }
void func_089CB100(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C021D0u); }
void func_089D0398(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C03524u); }
void func_089D7D54(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C044BCu); }
void func_089D8EF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C04F64u); }
void func_089DB78C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C07064u); }
void func_089E2F24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C0919Cu); }
void func_089E6798(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C098CCu); }
void func_089EF234(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C0B2D8u); }
void func_089F4680(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C0B478u); }
void func_089F6F30(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C0C010u); }
void func_089F8A24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C0C7A0u); }
void func_089F8AC8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C0C820u); }
void func_089FD434(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C0ECFCu); }
void func_089FEC58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C0EEA8u); }
void func_089FFFAC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C0F084u); }
void func_08A02808(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C0FAF0u); }
void func_08A05C34(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C10DE0u); }
void func_08A05CD8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C10E60u); }
void func_08A08F38(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C12FF8u); }
void func_08A0E090(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C14764u); }
void func_08A10194(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C149ACu); }
void func_08A19F24(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_list(rt, ctx, 0x08C18ECCu); }
void func_08898FB8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08CC2020u, 0x088978B4u, 32u, 0x08CC3E60u); }
void func_08A64554(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08CD2868u, 0x08A69D68u, 8u, 0x08CD3638u); }
void func_08A64BF8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08CD2868u, 0x08A69D68u, 8u, 0x08CD3638u); }
void func_08A64F10(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08CD2908u, 0x08A75A88u, 8u, 0x08CD3638u); }
void func_08A65074(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08CD2930u, 0x08A69D68u, 8u, 0x08CD3638u); }
void func_08A651D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08CD2958u, 0x08A69D68u, 8u, 0x08CD3638u); }
void func_08A65F90(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08CD2BD8u, 0x08A75A88u, 8u, 0x08CD3638u); }
void func_08A66A1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08CD2DB8u, 0x08A69D68u, 8u, 0x08CD3638u); }
void func_08A66B80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08CD2DE0u, 0x08A69D68u, 8u, 0x08CD3638u); }
void func_08A66CE4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08CD2E08u, 0x08A75A88u, 8u, 0x08CD3638u); }
void func_08A66E88(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08CD2E30u, 0x08A75A88u, 8u, 0x08CD3638u); }
void func_08A66FEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08CD2E58u, 0x08A75A88u, 8u, 0x08CD3638u); }
void func_08A67190(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08CD2E80u, 0x08A75A88u, 8u, 0x08CD3638u); }
void func_08A67504(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08CD2ED0u, 0x08A5B9ECu, 8u, 0x08CD3638u); }
void func_08A67668(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08CD2EF8u, 0x08A5B9ECu, 8u, 0x08CD3638u); }
void func_08A67D04(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08CD3010u, 0x08A69D68u, 8u, 0x08CD3638u); }
void func_08AC64FC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08CD6690u, 0x08B0C808u, 8u, 0x08CD6718u); }
void func_08B9C038(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08CBFF08u, 0x0887C8E8u, 4u, 0x08CD7118u); }
void func_08BABF40(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { reset_member(rt, ctx, 0x08CC2020u, 0x088978B4u, 32u, 0x08CC3E60u); }
void func_08A65964(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag(rt, ctx, 12u, 0x08CD2A70u); }
void func_08A65A2C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag(rt, ctx, 13u, 0x08CD2A98u); }
void func_08A65AF4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag(rt, ctx, 14u, 0x08CD2AC0u); }
void func_08A65BBC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag(rt, ctx, 15u, 0x08CD2AE8u); }
void func_08A65C84(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag(rt, ctx, 16u, 0x08CD2B10u); }
void func_08A65D4C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag(rt, ctx, 18u, 0x08CD2B60u); }
void func_08A65E14(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag(rt, ctx, 64u, 0x08CD2B88u); }
void func_08A662D8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag(rt, ctx, 25u, 0x08CD2C78u); }
void func_08A66450(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag(rt, ctx, 27u, 0x08CD2CC8u); }
void func_08A66728(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag(rt, ctx, 30u, 0x08CD2D40u); }
void func_08A668A0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag(rt, ctx, 32u, 0x08CD2D90u); }
void func_08A67718(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag(rt, ctx, 42u, 0x08CD2F20u); }
void func_08A67A94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag(rt, ctx, 46u, 0x08CD2FC0u); }
void func_08A67E94(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag(rt, ctx, 50u, 0x08CD3060u); }
void func_08A682E8(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag(rt, ctx, 55u, 0x08CD3128u); }
void func_08A68554(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag(rt, ctx, 58u, 0x08CD32B0u); }
void func_08A689C4(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag(rt, ctx, 63u, 0x08CD3598u); }
void func_08A68A8C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tag(rt, ctx, 64u, 0x08CD35C0u); }

} // namespace hot_batch_142
} // namespace gt::game

#define HOT142(address, name, kind) GT_DECOMP(address, gt::game::hot_batch_142::name, gt::ReturnKind::kind)
// HOT142(0x08BE9F78u, func_08BE9F78, Word);  // no comparable synthetic case yet
HOT142(0x08BEA6B8u, func_08BEA6B8, Word);
HOT142(0x08BEAC80u, func_08BEAC80, Word);
HOT142(0x08BECC54u, func_08BECC54, Word);
// HOT142(0x08BED1F4u, func_08BED1F4, Word);  // no comparable synthetic case yet
HOT142(0x08BEE3F8u, func_08BEE3F8, Word);
// HOT142(0x08BF218Cu, func_08BF218C, Word);  // no comparable synthetic case yet
HOT142(0x08BF2FFCu, func_08BF2FFC, Word);
// HOT142(0x08BF3198u, func_08BF3198, Word);  // no comparable synthetic case yet
// HOT142(0x08BF4A18u, func_08BF4A18, Word);  // no comparable synthetic case yet
HOT142(0x08BF4F84u, func_08BF4F84, Word);
HOT142(0x08BF7E0Cu, func_08BF7E0C, Word);
// HOT142(0x08BFB53Cu, func_08BFB53C, Word);  // no comparable synthetic case yet
// HOT142(0x08BFCB60u, func_08BFCB60, Word);  // no comparable synthetic case yet
// HOT142(0x08BFCCFCu, func_08BFCCFC, Word);  // no comparable synthetic case yet
HOT142(0x08BFCE98u, func_08BFCE98, Word);
// HOT142(0x08BFD034u, func_08BFD034, Word);  // no comparable synthetic case yet
HOT142(0x08BFE1E0u, func_08BFE1E0, Word);
HOT142(0x08BFE330u, func_08BFE330, Word);
// HOT142(0x08BFF920u, func_08BFF920, Word);  // no comparable synthetic case yet
// HOT142(0x08C03234u, func_08C03234, Word);  // no comparable synthetic case yet
HOT142(0x08C033D4u, func_08C033D4, Word);
HOT142(0x08C04814u, func_08C04814, Word);
// HOT142(0x08C0524Cu, func_08C0524C, Word);  // no comparable synthetic case yet
// HOT142(0x08C06970u, func_08C06970, Word);  // no comparable synthetic case yet
HOT142(0x08C0860Cu, func_08C0860C, Word);
// HOT142(0x08C09F08u, func_08C09F08, Word);  // no comparable synthetic case yet
HOT142(0x08C0C650u, func_08C0C650, Word);
// HOT142(0x08C0C8F4u, func_08C0C8F4, Word);  // no comparable synthetic case yet
// HOT142(0x08C0FDFCu, func_08C0FDFC, Word);  // no comparable synthetic case yet
// HOT142(0x08C0FF98u, func_08C0FF98, Word);  // no comparable synthetic case yet
// HOT142(0x08C107D4u, func_08C107D4, Word);  // no comparable synthetic case yet
HOT142(0x08C10C90u, func_08C10C90, Word);
HOT142(0x08C11030u, func_08C11030, Word);
// HOT142(0x08C14234u, func_08C14234, Word);  // no comparable synthetic case yet
// HOT142(0x08C14388u, func_08C14388, Word);  // no comparable synthetic case yet
// HOT142(0x08C144DCu, func_08C144DC, Word);  // no comparable synthetic case yet
HOT142(0x08C14F9Cu, func_08C14F9C, Word);
// HOT142(0x08C1E268u, func_08C1E268, Word);  // no comparable synthetic case yet
// HOT142(0x08C264F4u, func_08C264F4, Word);  // no comparable synthetic case yet
// HOT142(0x08C26CD4u, func_08C26CD4, Word);  // no comparable synthetic case yet
// HOT142(0x08C26E70u, func_08C26E70, Word);  // no comparable synthetic case yet
HOT142(0x08C2700Cu, func_08C2700C, Word);
HOT142(0x08C271A8u, func_08C271A8, Word);
// HOT142(0x08C27344u, func_08C27344, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT142(0x08C274E0u, func_08C274E0, Word);  // no comparable synthetic case yet
// HOT142(0x08C2767Cu, func_08C2767C, Word);  // no comparable synthetic case yet
// HOT142(0x08C27818u, func_08C27818, Word);  // no comparable synthetic case yet
// HOT142(0x08C279B4u, func_08C279B4, Word);  // no comparable synthetic case yet
// HOT142(0x08C27B50u, func_08C27B50, Word);  // no comparable synthetic case yet
// HOT142(0x08C2D27Cu, func_08C2D27C, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT142(0x08C2D418u, func_08C2D418, Word);  // no comparable synthetic case yet
// HOT142(0x08C2F3B0u, func_08C2F3B0, Word);  // no comparable synthetic case yet
// HOT142(0x08C2F6A4u, func_08C2F6A4, Word);  // no comparable synthetic case yet
// HOT142(0x08C2F840u, func_08C2F840, Word);  // no comparable synthetic case yet
// HOT142(0x08C387F4u, func_08C387F4, Word);  // no comparable synthetic case yet
// HOT142(0x08C38990u, func_08C38990, Word);  // no comparable synthetic case yet
// HOT142(0x08C38B2Cu, func_08C38B2C, Word);  // no comparable synthetic case yet
// HOT142(0x08C38CC8u, func_08C38CC8, Word);  // no comparable synthetic case yet
// HOT142(0x08C38E64u, func_08C38E64, Word);  // no comparable synthetic case yet
// HOT142(0x08C39000u, func_08C39000, Word);  // no comparable synthetic case yet
// HOT142(0x08C3919Cu, func_08C3919C, Word);  // no comparable synthetic case yet
// HOT142(0x08C39338u, func_08C39338, Word);  // no comparable synthetic case yet
// HOT142(0x08C394D4u, func_08C394D4, Word);  // no comparable synthetic case yet
// HOT142(0x08C39670u, func_08C39670, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT142(0x08C3980Cu, func_08C3980C, Word);  // no comparable synthetic case yet
// HOT142(0x08C399A8u, func_08C399A8, Word);  // no comparable synthetic case yet
HOT142(0x08C3A928u, func_08C3A928, Word);
// HOT142(0x08C3AAC4u, func_08C3AAC4, Word);  // no comparable synthetic case yet
// HOT142(0x08C3AC60u, func_08C3AC60, Word);  // compared equal, but the synthetic sabotage went undetected: not verified
// HOT142(0x08C3ADFCu, func_08C3ADFC, Word);  // no comparable synthetic case yet
HOT142(0x08C3AF98u, func_08C3AF98, Word);
// HOT142(0x08C3B134u, func_08C3B134, Word);  // no comparable synthetic case yet
HOT142(0x08C3B2D0u, func_08C3B2D0, Word);
HOT142(0x08C3B46Cu, func_08C3B46C, Word);
// HOT142(0x08C3B608u, func_08C3B608, Word);  // no comparable synthetic case yet
// HOT142(0x08C3B7A4u, func_08C3B7A4, Word);  // no comparable synthetic case yet
// HOT142(0x08C3B940u, func_08C3B940, Word);  // no comparable synthetic case yet
HOT142(0x0896BA78u, func_0896BA78, Word);
HOT142(0x08898F78u, func_08898F78, Word);
HOT142(0x08A65288u, func_08A65288, Word);
HOT142(0x0897520Cu, func_0897520C, Word);
HOT142(0x089752B0u, func_089752B0, Word);
HOT142(0x0897FC88u, func_0897FC88, Word);
HOT142(0x08984314u, func_08984314, Word);
HOT142(0x0898AACCu, func_0898AACC, Word);
HOT142(0x0898FFC8u, func_0898FFC8, Word);
HOT142(0x08994230u, func_08994230, Word);
HOT142(0x08995A14u, func_08995A14, Word);
HOT142(0x08997908u, func_08997908, Word);
HOT142(0x0899A7D4u, func_0899A7D4, Word);
HOT142(0x0899B1C0u, func_0899B1C0, Word);
HOT142(0x0899E1E8u, func_0899E1E8, Word);
HOT142(0x089A15C4u, func_089A15C4, Word);
HOT142(0x089A1668u, func_089A1668, Word);
HOT142(0x089A4C58u, func_089A4C58, Word);
HOT142(0x089B3A58u, func_089B3A58, Word);
HOT142(0x089B7334u, func_089B7334, Word);
HOT142(0x089B9CBCu, func_089B9CBC, Word);
HOT142(0x089BCC98u, func_089BCC98, Word);
HOT142(0x089C067Cu, func_089C067C, Word);
HOT142(0x089C2CE8u, func_089C2CE8, Word);
HOT142(0x089CB100u, func_089CB100, Word);
HOT142(0x089D0398u, func_089D0398, Word);
HOT142(0x089D7D54u, func_089D7D54, Word);
HOT142(0x089D8EF8u, func_089D8EF8, Word);
HOT142(0x089DB78Cu, func_089DB78C, Word);
HOT142(0x089E2F24u, func_089E2F24, Word);
HOT142(0x089E6798u, func_089E6798, Word);
HOT142(0x089EF234u, func_089EF234, Word);
HOT142(0x089F4680u, func_089F4680, Word);
HOT142(0x089F6F30u, func_089F6F30, Word);
HOT142(0x089F8A24u, func_089F8A24, Word);
HOT142(0x089F8AC8u, func_089F8AC8, Word);
HOT142(0x089FD434u, func_089FD434, Word);
HOT142(0x089FEC58u, func_089FEC58, Word);
HOT142(0x089FFFACu, func_089FFFAC, Word);
HOT142(0x08A02808u, func_08A02808, Word);
HOT142(0x08A05C34u, func_08A05C34, Word);
HOT142(0x08A05CD8u, func_08A05CD8, Word);
HOT142(0x08A08F38u, func_08A08F38, Word);
HOT142(0x08A0E090u, func_08A0E090, Word);
HOT142(0x08A10194u, func_08A10194, Word);
HOT142(0x08A19F24u, func_08A19F24, Word);
HOT142(0x08898FB8u, func_08898FB8, Word);
HOT142(0x08A64554u, func_08A64554, Word);
HOT142(0x08A64BF8u, func_08A64BF8, Word);
// HOT142(0x08A64F10u, func_08A64F10, Word);  // no comparable synthetic case yet
HOT142(0x08A65074u, func_08A65074, Word);
HOT142(0x08A651D8u, func_08A651D8, Word);
HOT142(0x08A65F90u, func_08A65F90, Word);
HOT142(0x08A66A1Cu, func_08A66A1C, Word);
HOT142(0x08A66B80u, func_08A66B80, Word);
HOT142(0x08A66CE4u, func_08A66CE4, Word);
HOT142(0x08A66E88u, func_08A66E88, Word);
// HOT142(0x08A66FECu, func_08A66FEC, Word);  // no comparable synthetic case yet
// HOT142(0x08A67190u, func_08A67190, Word);  // no comparable synthetic case yet
HOT142(0x08A67504u, func_08A67504, Word);
HOT142(0x08A67668u, func_08A67668, Word);
HOT142(0x08A67D04u, func_08A67D04, Word);
HOT142(0x08AC64FCu, func_08AC64FC, Word);
HOT142(0x08B9C038u, func_08B9C038, Word);
HOT142(0x08BABF40u, func_08BABF40, Word);
HOT142(0x08A65964u, func_08A65964, Word);
HOT142(0x08A65A2Cu, func_08A65A2C, Word);
HOT142(0x08A65AF4u, func_08A65AF4, Word);
HOT142(0x08A65BBCu, func_08A65BBC, Word);
HOT142(0x08A65C84u, func_08A65C84, Word);
HOT142(0x08A65D4Cu, func_08A65D4C, Word);
HOT142(0x08A65E14u, func_08A65E14, Word);
HOT142(0x08A662D8u, func_08A662D8, Word);
HOT142(0x08A66450u, func_08A66450, Word);
HOT142(0x08A66728u, func_08A66728, Word);
// HOT142(0x08A668A0u, func_08A668A0, Word);  // no comparable synthetic case yet
HOT142(0x08A67718u, func_08A67718, Word);
// HOT142(0x08A67A94u, func_08A67A94, Word);  // no comparable synthetic case yet
HOT142(0x08A67E94u, func_08A67E94, Word);
HOT142(0x08A682E8u, func_08A682E8, Word);
HOT142(0x08A68554u, func_08A68554, Word);
HOT142(0x08A689C4u, func_08A689C4, Word);
// HOT142(0x08A68A8Cu, func_08A68A8C, Word);  // no comparable synthetic case yet
