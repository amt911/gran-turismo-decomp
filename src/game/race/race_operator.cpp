// RaceOperator: the engine side and the natives of MRaceOperator, the object that runs the race session on
// the script side (layout and evidence: include/gt/game/race_operator.hpp; names from GT's registration,
// config/symbol_addrs.txt).
//
// Engine functions take the RaceOperatorHandle in a0 (the script object's engine object); the natives are
// `MRaceOperator_<method>`. Engine functions with no script name keep func_XXXXXXXX. Registered functions
// replace the recompiled originals; the ones commented out at the end are never compared on their own by
// the pad routes (nested in decompiled callers they ran as originals until nested verification; see their registrations), so the gate can't verify them.
//
// Modding: window_max, the memory plan and the series course codes live in RaceOperatorData; most of their
// readers here aren't verified by the gate yet (see the unregistered list at the end). Keep the original
// behaviour for the differential gate and make changes opt-in.

#include "decomp_registry.hpp"
#include "gt/game/race_operator.hpp"
#include "guest_call.hpp"
#include "../adhoc/natives.hpp"

#include <cstdint>

namespace gt::game::race_operator {
namespace {

using guest::View;
using namespace gt::adhoc;

constexpr std::uint32_t kSelfAsRaceOperator = 0x088248E4u;   // (holder *out) [a1 = self]
constexpr std::uint32_t kRaceOperatorSetOrganizer = 0x0885BB9Cu; // (operator engine object, organizer object)
constexpr std::uint32_t kEngineObject = 0x14u;               // MRaceOperator: its engine object
constexpr std::uint32_t kRaceOperatorTargetSlot = 0x0885BBF0u;  // (operator engine object, index) -> slot
constexpr std::uint32_t kRaceOperatorIsOnline = 0x0885BE14u;    // (operator engine object) -> bool
constexpr std::uint32_t kRaceOperatorSlotCarParameter = 0x0885C188u;    // (engine, slot) -> car parameter or 0
constexpr std::uint32_t kRaceOperatorSlotDriverParameter = 0x0885C228u; // (engine, slot, index) -> driver parameter or 0
constexpr std::uint32_t kRaceOperatorIsTheater = 0x0885C35Cu;
constexpr std::uint32_t kRaceOperatorClear = 0x0885C3C8u;
constexpr std::uint32_t kRaceOperatorCheckIn = 0x0885D010u;
constexpr std::uint32_t kRaceOperatorAllocateResource = 0x0885BFE0u;
constexpr std::uint32_t kWrapDriverParameter = 0x0884F674u;  // (holder *out, driver parameter *)
constexpr std::uint32_t kWrapCarParameter = 0x08857EE4u;      // (holder *out, car parameter *, 0)
constexpr std::uint32_t kAsOrganizer = 0x088297A8u;          // (holder *out, self or argument)
constexpr std::uint32_t kOrganizerCore = 0x088294D0u;        // (organizer *) -> its engine object
constexpr std::uint32_t kHolderReset = 0x08A5BAD8u;

std::uint32_t word(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load32(address); }

// The data of the operator handle in a0.
View<RaceOperatorData> data_of(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    return *View<RaceOperatorHandle>(rt.memory(), ctx.gpr[4]).data();
}

// The slot with the index in a1 (392 bytes each).
View<RaceOperatorSlot> slot_of(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    return View<RaceOperatorSlot>(rt.memory(), data_of(rt, ctx).unk_00() + ctx.gpr[5] * 392u);
}

void memory_assign(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t index) {
    ctx.gpr[2] = data_of(rt, ctx).memory_assign(index).address();
}

void table_entry_tail(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t next) {
    const std::uint32_t list = word(rt, ctx.gpr[4]);
    const std::uint32_t i = ctx.gpr[5];
    tail_call(ctx, next, {word(rt, list) + i * 392u, i << 3, list});
}

} // namespace

// --- Engine --------------------------------------------------------------------------------------------

// set_organizer(this, x): +0x4 = x, then func_0885E424(x, the word +0x0) (tail call).
void set_organizer(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4], x = ctx.gpr[5];
    const std::uint32_t head = word(rt, self);
    rt.memory().store32(self + 4u, x);
    tail_call(ctx, 0x0885E424u, {x, head});
}

// personal_id(this) -> the 64-bit value +0x28 of the object (+0x0).
void personal_id(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = word(rt, object + 40u);
    ctx.gpr[3] = word(rt, object + 44u);
}

// get_target_slot(this, n) -> the index of the n-th entry (from 0) of the 392-byte entries (this's +0x0: data,
// count) func_0885D4CC accepts, or -1.
void get_target_slot(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t remaining = ctx.gpr[5];
    GuestFrame frame(ctx, 0x20u);
    for (std::int32_t i = 0; i < static_cast<std::int32_t>(word(rt, word(rt, self) + 4u)); ++i) {
        const std::uint32_t entry = word(rt, word(rt, self)) + static_cast<std::uint32_t>(i) * 392u;
        if (call_guest(rt, ctx, 0x0885D4CCu, {entry}) == 0u) continue;
        if (--remaining == 0xFFFFFFFFu) {
            ctx.gpr[2] = static_cast<std::uint32_t>(i);
            return;
        }
    }
    ctx.gpr[2] = 0xFFFFFFFFu;
}

// set_with_ghost(this, v): the byte with_ghost (+0x18) of the data = v.
void set_with_ghost(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { rt.memory().store8(word(rt, ctx.gpr[4]) + 24u, ctx.gpr[5] & 255u); }

// build_race(this) -> func_08861B20(the word +0x4, the record +0x7C, +0x80, +0x84, +0x88 when the byte +0x19 is set) of the
// record at +0x0, under the lock this + 8.
void build_race(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    (void)call_guest(rt, ctx, 0x08B344E0u, {self + 8u});
    const std::uint32_t record = word(rt, self);
    const std::uint32_t extra = rt.memory().load8(record + 25u) == 0u ? 0u : word(rt, record + 136u);
    const std::uint32_t result = call_guest(rt, ctx, 0x08861B20u,
                                            {word(rt, self + 4u), word(rt, record + 124u), word(rt, record + 128u), word(rt, record + 132u), extra});
    (void)call_guest(rt, ctx, 0x08B3457Cu, {self + 8u});
    ctx.gpr[2] = result;
}

// Calls on a field of *this: is_online -> race_parameter::online_on(the object at +0x7C); func_0885EC1C ->
// func_088D9F80(the object at +0x5AC) as a tail call; Organizer::can_enter_course -> whether func_088D7FB4(the
// object at +0x5AC) is nonzero.
void is_online(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, word(rt, ctx.gpr[4]) + 0x7Cu);
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x088D4C9Cu, {object});
}

// set_race_parameter(this, index): func_088D4824(the member +0x7C of the object +0x0) unless index is 0 (tail call;
// the attribute handler func_08828494 passes 0 or the race parameter's index from 0x08846E50).
void set_race_parameter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    if (ctx.gpr[5] == 0u) return;
    tail_call(ctx, 0x088D4824u, {word(rt, word(rt, ctx.gpr[4]) + 124u)});
}

// set_game_condition / set_gadget_condition(this, ...): func_08894098 / func_08893C40(the member +0x80 / +0x84 of the
// object +0x0, ...) (tail calls).
void set_game_condition(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08894098u, {word(rt, word(rt, ctx.gpr[4]) + 128u)}); }
void set_gadget_condition(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08893C40u, {word(rt, word(rt, ctx.gpr[4]) + 132u)}); }

// get_account_car_parameter(this, i): table_entry_tail into func_0885D54C.
void get_account_car_parameter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { table_entry_tail(rt, ctx, 0x0885D54Cu); }

// get_account_fixed_car_parameter(this, i): table_entry_tail into func_0885D55C.
void get_account_fixed_car_parameter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { table_entry_tail(rt, ctx, 0x0885D55Cu); }

// get_account_driver_parameter(this, i, x): func_0885D56C(entry i (392 bytes) of the table the list (+0x0) holds, x) (tail call).
void get_account_driver_parameter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x0885D56Cu, {word(rt, word(rt, ctx.gpr[4])) + ctx.gpr[5] * 392u, ctx.gpr[6]});
}

// is_theater(this) -> the byte +0x17 of the object +0x0.
void is_theater(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[3] = word(rt, ctx.gpr[4]);
    ctx.gpr[2] = rt.memory().load8(ctx.gpr[3] + 23u);
}

// func_0885D4CC(this) -> CarParameter::target(this + 0x50) when the byte at +0x16 is set and the one at +0x17
// is clear, else 0.
void func_0885D4CC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t result = 0u;
    if (rt.memory().load8(self + 0x16u) != 0u) {
        ctx.gpr[4] = self + 0x50u;
        if (rt.memory().load8(self + 0x17u) == 0u) {
            GuestFrame frame(ctx, 0x10u);
            result = call_guest(rt, ctx, 0x088A5928u, {self + 0x50u});
        }
    }
    ctx.gpr[2] = result;
}


// attribute window_max
void set_window_max(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { data_of(rt, ctx).window_max() = static_cast<std::int32_t>(ctx.gpr[5]); }
void window_max(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = static_cast<std::uint32_t>(data_of(rt, ctx).window_max().value()); }
// attribute personal_id (the getter is personal_id above)
void set_personal_id(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<RaceOperatorData> data = data_of(rt, ctx);
    data.personal_id_low() = ctx.gpr[6];
    data.personal_id_high() = ctx.gpr[7];
}
// attribute with_ghost (the setter is set_with_ghost above)
void with_ghost(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data_of(rt, ctx).with_ghost(); }
// attribute expand_memory
void set_expand_memory(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { data_of(rt, ctx).expand_memory() = static_cast<std::uint8_t>(ctx.gpr[5]); }
void expand_memory(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data_of(rt, ctx).expand_memory(); }
// attribute representative_slot_id (read only)
void representative_slot_id(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = static_cast<std::uint32_t>(data_of(rt, ctx).representative_slot_id().value());
}
// attributes race_parameter, game_condition, gadget_condition, race_build_parameter (the getters)
void race_parameter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data_of(rt, ctx).race_parameter(); }
void game_condition(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data_of(rt, ctx).game_condition(); }
void gadget_condition(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data_of(rt, ctx).gadget_condition(); }
void race_build_parameter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data_of(rt, ctx).race_build_parameter(); }

// getCurrentSeriesRaceNumber / getMaxSeriesRace
void get_current_series_race_number(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = static_cast<std::uint32_t>(data_of(rt, ctx).current_series_race_number().value());
}
void get_max_series_race(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = static_cast<std::uint32_t>(data_of(rt, ctx).max_series_race().value());
}
// getSeriesCourseCode(i): the 64-bit course code (low in v0, high in v1).
void get_series_course_code(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<RaceOperatorData> data = data_of(rt, ctx);
    ctx.gpr[2] = data.series_course_code_low(ctx.gpr[5]);
    ctx.gpr[3] = data.series_course_code_high(ctx.gpr[5]);
}

// setAccountPoint(slot, v) / getAccountPoint(slot)
void set_account_point(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { slot_of(rt, ctx).account_point() = static_cast<std::int32_t>(ctx.gpr[6]); }
void get_account_point(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = static_cast<std::uint32_t>(slot_of(rt, ctx).account_point().value()); }
// setAccountPersonalID(slot, low, high) / getAccountPersonalID(slot)
void set_account_personal_id(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<RaceOperatorSlot> slot = slot_of(rt, ctx);
    slot.account_personal_id_low() = ctx.gpr[6];
    slot.account_personal_id_high() = ctx.gpr[7];
}
void get_account_personal_id(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<RaceOperatorSlot> slot = slot_of(rt, ctx);
    ctx.gpr[2] = slot.account_personal_id_low();
    ctx.gpr[3] = slot.account_personal_id_high();
}
// checkValidDriver(slot, i): the byte at +0x40 + i of the slot.
void check_valid_driver(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = slot_of(rt, ctx).unk_40(ctx.gpr[6]);
}

// getMemoryAssign_*: the address of one of the six blocks of the memory plan.
void get_memory_assign_standard(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { memory_assign(rt, ctx, 0u); }
void get_memory_assign_expand_memory(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { memory_assign(rt, ctx, 1u); }
void get_memory_assign_without_replay(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { memory_assign(rt, ctx, 2u); }
void get_memory_assign_for_replay(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { memory_assign(rt, ctx, 3u); }
void get_memory_assign_with_ghost(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { memory_assign(rt, ctx, 4u); }
void get_memory_assign_many_players(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { memory_assign(rt, ctx, 5u); }

// --- Natives --------------------------------------------------------------------------------------------

// MRaceOperator.setOrganizer(organizer): with at least one argument.
void MRaceOperator_setOrganizer(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    std::uint32_t engine = 0u;
    with_self(rt, ctx, kSelfAsRaceOperator, [&](std::uint32_t op) { engine = rt.memory().load32(op + kEngineObject); });
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t organizer = frame.at(0x0u);
    (void)call_guest(rt, ctx, kAsOrganizer, {organizer, argv});
    const std::uint32_t core = call_guest(rt, ctx, kOrganizerCore, {rt.memory().load32(organizer)});
    (void)call_guest(rt, ctx, kRaceOperatorSetOrganizer, {engine, core});
    destroy(rt, ctx, organizer);
}

// MRaceOperator's engine object (self as MRaceOperator, +0x14; the holder is dropped first).
std::uint32_t race_operator(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    std::uint32_t engine = 0u;
    with_self(rt, ctx, kSelfAsRaceOperator, [&](std::uint32_t op) { engine = rt.memory().load32(op + kEngineObject); });
    return engine;
}

// MRaceOperator.getTargetSlot(index) -> integer, with at least one argument.
void MRaceOperator_getTargetSlot(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    const std::uint32_t engine = race_operator(rt, ctx);
    const auto index = static_cast<std::uint32_t>(to_int(rt, ctx, rt.memory().load32(argv)));
    const std::uint32_t slot = call_guest(rt, ctx, kRaceOperatorTargetSlot, {engine, index});
    GuestFrame frame(ctx, 0x20u);
    return_integer(rt, ctx, result, frame.at(0u), slot);
}

// MRaceOperator.isOnline() -> boolean.
void MRaceOperator_isOnline(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t online = call_guest(rt, ctx, kRaceOperatorIsOnline, {race_operator(rt, ctx)});
    GuestFrame frame(ctx, 0x20u);
    return_bool(rt, ctx, result, frame.at(0u), online);
}


// --- MRaceOperator: the race's operator (self as MRaceOperator, engine object at +0x14) ---
void MRaceOperator_getTargetCarParameter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    const std::uint32_t engine = race_operator(rt, ctx);
    const auto slot = static_cast<std::int32_t>(call_guest(rt, ctx, kRaceOperatorTargetSlot, {engine, int_argument(rt, ctx, argv, 0u)}));
    if (slot < 0) return;
    const std::uint32_t car = call_guest(rt, ctx, kRaceOperatorSlotCarParameter, {engine, static_cast<std::uint32_t>(slot)});
    if (car == 0u) return;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t out = frame.at(0u);
    (void)call_guest(rt, ctx, kWrapCarParameter, {out, car, 0u});
    if (result != out) (void)call_guest(rt, ctx, kHolderReset, {result, rt.memory().load32(out)});
    destroy(rt, ctx, out);
}

void MRaceOperator_getTargetDriverParameter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 1) return;
    const std::uint32_t engine = race_operator(rt, ctx);
    const auto slot = static_cast<std::int32_t>(call_guest(rt, ctx, kRaceOperatorTargetSlot, {engine, int_argument(rt, ctx, argv, 0u)}));
    if (slot < 0) return;
    const std::uint32_t driver = call_guest(rt, ctx, kRaceOperatorSlotDriverParameter,
                                            {engine, static_cast<std::uint32_t>(slot), int_argument(rt, ctx, argv, 1u)});
    if (driver != 0u) return_wrapped(rt, ctx, result, kWrapDriverParameter, driver);
}

void race_operator_bool(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t query) {
    const std::uint32_t result = ctx.gpr[4];
    return_boolean(rt, ctx, result, call_guest(rt, ctx, query, {race_operator(rt, ctx)}));
}

void MRaceOperator_isTheater(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { race_operator_bool(rt, ctx, kRaceOperatorIsTheater); }

void MRaceOperator_checkIn(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { race_operator_bool(rt, ctx, kRaceOperatorCheckIn); }

void MRaceOperator_allocateResource(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { race_operator_bool(rt, ctx, kRaceOperatorAllocateResource); }

void MRaceOperator_clear(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    (void)call_guest(rt, ctx, kRaceOperatorClear, {race_operator(rt, ctx)});
}

} // namespace gt::game::race_operator

namespace race_operator_registrations {
using gt::ReturnKind;
#define RACE_OPERATOR(address, name, kind) GT_DECOMP_NESTED(address, gt::game::race_operator::name, ReturnKind::kind)

RACE_OPERATOR(0x0885BBD8u, personal_id, DoubleWord);
RACE_OPERATOR(0x0885BC90u, set_with_ghost, Void);
RACE_OPERATOR(0x0885BE98u, set_game_condition, Void);
RACE_OPERATOR(0x0885BEBCu, set_gadget_condition, Void);
RACE_OPERATOR(0x08824E88u, MRaceOperator_setOrganizer, Void);
RACE_OPERATOR(0x08824CB0u, MRaceOperator_clear, Void);
// Not comparable yet (waits on the UMD drive: sceUmdWaitDriveStat): MRaceOperator_checkIn (0x08825AE8).
// Not comparable yet (creates threads: sceKernelCreateThread): MRaceOperator_allocateResource (0x08825B74).
// Reached only inside their decompiled callers' cases: verified there since nested callees run decompiled in
// a case's decompiled run (2026-10-13; nested in 2-4 equal cases, 0 mismatches, sabotage pinned on each).
RACE_OPERATOR(0x0885BB9Cu, set_organizer, Void);
RACE_OPERATOR(0x0885C188u, get_account_car_parameter, Word);
RACE_OPERATOR(0x0885C228u, get_account_driver_parameter, Word);
RACE_OPERATOR(0x0885C35Cu, is_theater, Word);
RACE_OPERATOR(0x0885D4CCu, func_0885D4CC, Word);
// RACE_OPERATOR(0x0885BBB0u, set_window_max, Void);
RACE_OPERATOR(0x0885BBBCu, window_max, Word);
// RACE_OPERATOR(0x0885BBC8u, set_personal_id, Void);
// RACE_OPERATOR(0x0885BC9Cu, with_ghost, Word);
// RACE_OPERATOR(0x0885C368u, set_expand_memory, Void);
// RACE_OPERATOR(0x0885C374u, expand_memory, Word);
// RACE_OPERATOR(0x0885C0DCu, representative_slot_id, Word);
RACE_OPERATOR(0x0885BE8Cu, race_parameter, Word);
// RACE_OPERATOR(0x0885BEA4u, game_condition, Word);
// RACE_OPERATOR(0x0885BEC8u, gadget_condition, Word);
// RACE_OPERATOR(0x0885BF50u, race_build_parameter, Word);
// RACE_OPERATOR(0x0885BD24u, get_current_series_race_number, Word);
// RACE_OPERATOR(0x0885BD30u, get_max_series_race, Word);
// RACE_OPERATOR(0x0885BCA8u, get_series_course_code, DoubleWord);
// RACE_OPERATOR(0x0885C0E8u, set_account_point, Void);
// RACE_OPERATOR(0x0885C110u, get_account_point, Word);
// RACE_OPERATOR(0x0885D29Cu, set_account_personal_id, Void);
// RACE_OPERATOR(0x0885C138u, get_account_personal_id, DoubleWord);
// RACE_OPERATOR(0x0885C1FCu, check_valid_driver, Word);
RACE_OPERATOR(0x0885C380u, get_memory_assign_standard, Word);
// RACE_OPERATOR(0x0885C3A4u, get_memory_assign_expand_memory, Word);
// RACE_OPERATOR(0x0885C3B0u, get_memory_assign_without_replay, Word);
// RACE_OPERATOR(0x0885C3BCu, get_memory_assign_for_replay, Word);
RACE_OPERATOR(0x0885C38Cu, get_memory_assign_with_ghost, Word);
// RACE_OPERATOR(0x0885C398u, get_memory_assign_many_players, Word);
RACE_OPERATOR(0x0885BBF0u, get_target_slot, Word);
RACE_OPERATOR(0x0885BD3Cu, build_race, Word);
RACE_OPERATOR(0x0885BE14u, is_online, Word);
RACE_OPERATOR(0x0885BE70u, set_race_parameter, Word);
RACE_OPERATOR(0x0885C1ACu, get_account_fixed_car_parameter, Word);
RACE_OPERATOR(0x0882689Cu, MRaceOperator_getTargetSlot, Void);
RACE_OPERATOR(0x08826070u, MRaceOperator_isOnline, Void);
RACE_OPERATOR(0x08826968u, MRaceOperator_getTargetCarParameter, Void);
RACE_OPERATOR(0x088270E8u, MRaceOperator_getTargetDriverParameter, Void);
RACE_OPERATOR(0x08825FE4u, MRaceOperator_isTheater, Void);
#undef RACE_OPERATOR
} // namespace race_operator_registrations
