// Organizer: the engine side and the natives of MOrganizer, the race/session organizer (layout and
// evidence: include/gt/game/organizer.hpp; names from GT's registration, config/symbol_addrs.txt).
//
// Engine functions take the OrganizerHandle in a0 (the script object's engine object) and work on the
// OrganizerData its first word points to. A function is named after the method or attribute whose native
// calls it when only that one does; the ones several natives share keep func_XXXXXXXX. Registered
// functions replace the recompiled originals; the ones commented out at the end were not compared in any
// fast, dense (GT_DECOMP_DENSE: every call of the function compared) or full run of the PR (or reach calls the harness can't replay), so the gate can't verify them.
//
// Modding: the organizer is where a session starts, runs and ends; a mod that wants, say, a different
// session setup changes the engine functions behind enterCourse and the entry setters. Keep the original
// behaviour for the differential gate and make such changes opt-in.

#include "decomp_registry.hpp"
#include "gt/game/organizer.hpp"
#include "guest_call.hpp"
#include "../adhoc/natives.hpp"

#include <cstdint>

namespace gt::game::organizer {
namespace {

using guest::View;
using namespace gt::adhoc;

constexpr std::uint32_t kArgumentAsRaceSound = 0x08835BD8u;  // (holder *out, argv)
constexpr std::uint32_t kArgumentAsSoundContext = 0x08A22050u;  // (holder *out, argv)
constexpr std::uint32_t kAsOrganizer = 0x088297A8u;          // (holder *out, self or argument)
constexpr std::uint32_t kCameraType = 0x08917918u;                    // (camera, 1) -> int
constexpr std::uint32_t kCarDriverParameterCore = 0x0884D3D8u;
constexpr std::uint32_t kHolderCopyRaceSound = 0x08835AB8u;           // holder copy instance
constexpr std::uint32_t kHolderReset = 0x08A5BAD8u;
constexpr std::uint32_t kOrganizerCamera = 0x0885E710u;               // (core) -> camera
constexpr std::uint32_t kOrganizerCanEnterCourse = 0x0885E8B8u;  // (organizer core) -> bool
constexpr std::uint32_t kOrganizerCore = 0x088294D0u;        // (organizer *) -> its engine object
constexpr std::uint32_t kOrganizerCurrentRace = 0x0885EDF8u;    // (core) -> race state (race parameter at +4)
constexpr std::uint32_t kOrganizerDisableRenderingRace = 0x0885E788u;
constexpr std::uint32_t kOrganizerDisableSoundingRace = 0x0885E7B8u;
constexpr std::uint32_t kOrganizerEnableRenderingRace = 0x0885E778u;
constexpr std::uint32_t kOrganizerEnableSoundingRace = 0x0885E794u;
constexpr std::uint32_t kOrganizerEndSession = 0x0885F46Cu;
constexpr std::uint32_t kOrganizerEnterCourse = 0x0885FC4Cu;
constexpr std::uint32_t kOrganizerEntriedDriver = 0x0885ECF4u;  // (core, entry, slot) -> driver parameter or 0
constexpr std::uint32_t kOrganizerEntryMax = 0x0885EC1Cu;    // (organizer core) -> int
constexpr std::uint32_t kOrganizerExitCourse = 0x0885E940u;
constexpr std::uint32_t kOrganizerGameCondition = 0x0885EE04u;        // (core) -> game condition
constexpr std::uint32_t kOrganizerHasLoadingCarDone = 0x0885E91Cu;
constexpr std::uint32_t kOrganizerRaceDisplay = 0x0885EE58u;          // (core) -> race display
constexpr std::uint32_t kOrganizerRaceSoundHolder = 0x08829314u;      // (organizer *) -> holder *
constexpr std::uint32_t kOrganizerRequestLoadingCourse = 0x0885E848u;
constexpr std::uint32_t kOrganizerSetCameraTarget = 0x0885E6E0u;      // (core, a, b)
constexpr std::uint32_t kOrganizerSetChangeableDriver = 0x0885ED58u;  // (core, entry, slot, driver parameter core)
constexpr std::uint32_t kOrganizerSetDrivingLineDisp = 0x0885E1F0u;   // (core, entry, bool)
constexpr std::uint32_t kOrganizerSetEndSessionSecond = 0x0885E9ECu;  // (core, seconds)
constexpr std::uint32_t kOrganizerSetNoInputLimit = 0x0885E9F8u;      // (core, seconds)
constexpr std::uint32_t kOrganizerSetRaceSound = 0x08829538u;   // (organizer *, holder *)
constexpr std::uint32_t kOrganizerSetSoundContext = 0x088294D8u; // (organizer *, holder *)
constexpr std::uint32_t kOrganizerStartLoop = 0x08860058u;   // (engine object)
constexpr std::uint32_t kRaceDisplaySetTarget = 0x0889368Cu;          // (display, a, b)
constexpr std::uint32_t kRaceParameterCore = 0x08846E50u;    // (race parameter *) -> engine object
constexpr std::uint32_t kRaceStateEntryControlType = 0x088DC858u;     // (race state, entry) -> int
constexpr std::uint32_t kRaceStateSetBackRace = 0x088D4824u;          // (race state + 0xC, race parameter core)
constexpr std::uint32_t kSelfAsCarDriverParameter = 0x0884D660u;  // (holder *out) [a1 = self]
constexpr std::uint32_t kSelfAsRaceParameter = 0x08847008u;  // (holder *out) [a1 = self]
constexpr std::uint32_t kWrapDriverParameter = 0x0884F674u;  // (holder *out, driver parameter *)
constexpr std::uint32_t kWrapGameCondition = 0x0884642Cu;             // (holder *out, game condition *)
constexpr std::uint32_t kWrapRaceParameter = 0x08848DB8u;    // (holder *out, race parameter *)

std::uint32_t word(psprecomp::Runtime &rt, std::uint32_t address) { return rt.memory().load32(address); }

// The data of the handle in a0.
View<OrganizerData> data_of(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    return *View<OrganizerHandle>(rt.memory(), ctx.gpr[4]).data();
}

} // namespace

// --- Engine -------------------------------------------------------------------------------------------

// MRaceOperator::setOrganizer hands the operator over: the word +0x3F50 = v.
void func_0885E424(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { data_of(rt, ctx).unk_3F50() = ctx.gpr[5]; }
// The word +0x3F98 / +0x3F9C = v (the sound target and its companion).
void func_0885EFE0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { data_of(rt, ctx).unk_3F98() = ctx.gpr[5]; }
void func_0885EFEC(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { data_of(rt, ctx).unk_3F9C() = ctx.gpr[5]; }

// enableRenderingRace / disableRenderingRace
void enable_rendering_race(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { data_of(rt, ctx).rendering_race() = 1u; }
void disable_rendering_race(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { data_of(rt, ctx).rendering_race() = 0u; }
// enableSoundingRace / disableSoundingRace: 0x08AB8858(sound target, 1 / 0) when there is a target.
void enable_sounding_race(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = data_of(rt, ctx).unk_3F98();
    ctx.gpr[2] = target;
    if (target == 0u) return;
    tail_call(ctx, 0x08AB8858u, {target, 1u});
}
void disable_sounding_race(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t target = data_of(rt, ctx).unk_3F98();
    if (target == 0u) {
        ctx.gpr[2] = 0u;
        return;
    }
    tail_call(ctx, 0x08AB8858u, {target, 0u});
}

// startGameObjectLoop: the virtual +0x1C of the object a0 points to (tail call).
void start_game_object_loop(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = word(rt, ctx.gpr[4]);
    tail_call(ctx, word(rt, word(rt, object) + 28u), {object});
}

// 0x0885EC1C (also called by getSlotIDListOnGrid, getEntriedCarParameters and RaceOperator::race_build_parameter):
// tail-calls 0x088D9F80 on the object at +0x5AC; v0 is first loaded with *a0, as the original does.
void func_0885EC1C(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    ctx.gpr[2] = word(rt, ctx.gpr[4]);
    tail_call(ctx, 0x088D9F80u, {data_of(rt, ctx).unk_5AC()});
}
void can_enter_course(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t object = data_of(rt, ctx).unk_5AC();
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x088D7FB4u, {object}) != 0u ? 1u : 0u;
}
void has_loading_car_done(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    GuestFrame frame(ctx, 0x10u);
    ctx.gpr[2] = call_guest(rt, ctx, 0x088D7FF8u, {data_of(rt, ctx).unk_5AC(), ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]}) != 0u ? 1u : 0u;
}
// setApplicationEndSessionSecond / setNoInputLimitSecond (tail calls on the object at +0x5AC)
void set_application_end_session_second(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x088DAD14u, {data_of(rt, ctx).unk_5AC()});
}
void set_no_input_limit_second(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x088DAD54u, {data_of(rt, ctx).unk_5AC()});
}
// requestLoadingCourse: 0x0885E7DC(this), then 0x088D8154(the object at +0x5AC, 0) (tail call).
void request_loading_course(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x0885E7DCu, {self, ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    }
    tail_call(ctx, 0x088D8154u, {word(rt, word(rt, self) + 0x5ACu), 0u});
}
// enterCourse
void enter_course(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[4];
    std::uint32_t value;
    {
        GuestFrame frame(ctx, 0x10u);
        (void)call_guest(rt, ctx, 0x08866918u, {word(rt, self)});
        (void)call_guest(rt, ctx, 0x088D6E58u, {data_of(rt, ctx).unk_5AC()});
        value = call_guest(rt, ctx, 0x088D5EACu, {data_of(rt, ctx).unk_5AC() + 4u});
    }
    tail_call(ctx, 0x088F27F8u, {data_of(rt, ctx).unk_45A4().address(), value});
}

// getGameCondition: the address of the game condition inside the data.
void get_game_condition(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data_of(rt, ctx).game_condition().address(); }
// The race display (+0x600) and the replay buffer (+0x3F80), shared by several natives.
void func_0885EE58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data_of(rt, ctx).race_display().address(); }
void func_0885EE70(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { ctx.gpr[2] = data_of(rt, ctx).replay_buffer().address(); }
// Forwards on the race display / the pause object / the camera, used by the attributes and the camera methods.
void func_0885E758(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x0889366Cu, {data_of(rt, ctx).race_display().address()}); }
void func_0885EF44(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08865F68u, {data_of(rt, ctx).unk_3F58().address()}); }
void func_0885EF50(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08865F70u, {data_of(rt, ctx).unk_3F58().address()}); }
void func_0885EF18(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08865F54u, {data_of(rt, ctx).unk_3F58().address()}); }
// attribute window_max: the setter forwards to 0x08917F54 on the camera, the getter to 0x08917840.
void set_window_max(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08917F54u, {data_of(rt, ctx).camera().address()}); }
void window_max(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { tail_call(ctx, 0x08917840u, {data_of(rt, ctx).camera().address()}); }
// 0x0885E6E0(a, b): 0x08917870(camera, b, a) (tail call); also called by RaceOperator (0x0885CA10, 0x0885D184, 0x0885D1BC,
// 0x0885CFD4) besides setCameraTarget, so it keeps its address.
void func_0885E6E0(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x08917870u, {data_of(rt, ctx).camera().address(), ctx.gpr[6], ctx.gpr[5]});
}
// 0x0885E498(this, x): 0x08891C54(the replay buffer, x or, when 0, the word +0x8C).
void func_0885E498(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<OrganizerData> data = data_of(rt, ctx);
    const std::uint32_t x = ctx.gpr[5] != 0u ? ctx.gpr[5] : data.unk_8C().value();
    tail_call(ctx, 0x08891C54u, {data.replay_buffer().address(), x});
}
// 0x0885EC80(a, b): 0x0885DFF4(data, a, b, 0) (tail call); called by setChangeableCarParameterAtBeginningOfSession,
// setChangeableCarParameterAtPitWork and others, so it keeps its address as name.
void func_0885EC80(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    tail_call(ctx, 0x0885DFF4u, {word(rt, ctx.gpr[4]), ctx.gpr[5], ctx.gpr[6], 0u});
}
// getEntriedDriverParameter(entry, slot): the entry list's (+0x5B4) record + 0x20, or 0.
void get_entried_driver_parameter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<OrganizerData> data = data_of(rt, ctx);
    const std::uint32_t a = ctx.gpr[5];
    std::uint32_t k = ctx.gpr[6];
    ctx.gpr[2] = 0u;
    GuestFrame frame(ctx, 0x10u);
    if (static_cast<std::int32_t>(k) < 0) {
        k = call_guest(rt, ctx, 0x088F8A14u, {data.unk_5B4(), a});
        if (static_cast<std::int32_t>(k) < 0) return;
    }
    const std::uint32_t found = call_guest(rt, ctx, 0x088F8D0Cu, {data.unk_5B4(), a, k});
    ctx.gpr[2] = found != 0u ? found + 32u : 0u;
}
// 0x0885ED58(a, b, c): shared by setChangeableDriverParameterAtBeginningOfSession and other callers (keeps its address).
void func_0885ED58(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t a = ctx.gpr[5], c = ctx.gpr[7];
    std::uint32_t b = ctx.gpr[6];
    std::uint32_t entry;
    {
        GuestFrame frame(ctx, 0x10u);
        const View<OrganizerData> data = data_of(rt, ctx);
        if (static_cast<std::int32_t>(b) < 0) b = call_guest(rt, ctx, 0x088F8A14u, {data.unk_5B4(), a});
        entry = call_guest(rt, ctx, 0x088F8CC0u, {data.unk_5B4(), a, b});
    }
    tail_call(ctx, 0x08896EFCu, {entry + 32u, c});
}
// pauseRace's engine function: 0x088DE9CC(the object at +0x5AC); when non-zero the pause object is told 0 (tail call),
// else 0x088D7744(the object, flag) (tail call).
void func_0885EF78(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const View<OrganizerData> data = data_of(rt, ctx);
    const std::uint32_t flag = ctx.gpr[5] & 255u;
    std::uint32_t result;
    {
        GuestFrame frame(ctx, 0x10u);
        result = call_guest(rt, ctx, 0x088DE9CCu, {data.unk_5AC(), ctx.gpr[5], ctx.gpr[6], ctx.gpr[7]});
    }
    if (result != 0u) {
        tail_call(ctx, 0x08865F7Cu, {data.unk_3F58().address(), 0u});
        return;
    }
    tail_call(ctx, 0x088D7744u, {data.unk_5AC(), flag});
}

// --- Natives --------------------------------------------------------------------------------------------

// MOrganizer: the race session. (initialize(), 0x0882A638, has the same shape but creates threads,
// which a harness case can't replay: it stays recompiled until it can be compared.)
void organizer_call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t engine) {
    core_call(rt, ctx, kAsOrganizer, kOrganizerCore, engine, DropFirst::yes);
}
void MOrganizer_startGameObjectLoop(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { organizer_call(rt, ctx, kOrganizerStartLoop); }
void MOrganizer_endSession(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { organizer_call(rt, ctx, kOrganizerEndSession); }
void MOrganizer_exitCourse(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { organizer_call(rt, ctx, kOrganizerExitCourse); }
void MOrganizer_enterCourse(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { organizer_call(rt, ctx, kOrganizerEnterCourse); }
void MOrganizer_requestLoadingCourse(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { organizer_call(rt, ctx, kOrganizerRequestLoadingCourse); }
void MOrganizer_enableSoundingRace(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { organizer_call(rt, ctx, kOrganizerEnableSoundingRace); }
void MOrganizer_disableSoundingRace(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { organizer_call(rt, ctx, kOrganizerDisableSoundingRace); }
void MOrganizer_enableRenderingRace(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { organizer_call(rt, ctx, kOrganizerEnableRenderingRace); }
void MOrganizer_disableRenderingRace(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { organizer_call(rt, ctx, kOrganizerDisableRenderingRace); }

// MOrganizer.setRaceSound(sound) / setSoundContext(context): with at least one argument.
void organizer_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t convert, std::uint32_t set) {
    const std::uint32_t self = ctx.gpr[5];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t organizer = frame.at(0x0u);
    const std::uint32_t value = frame.at(0x4u);
    (void)call_guest(rt, ctx, convert, {value, argv});
    (void)call_guest(rt, ctx, kAsOrganizer, {organizer, self});
    (void)call_guest(rt, ctx, set, {rt.memory().load32(organizer), value});
    destroy(rt, ctx, organizer);
    destroy(rt, ctx, value);
}
void MOrganizer_setRaceSound(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    organizer_set(rt, ctx, kArgumentAsRaceSound, kOrganizerSetRaceSound);
}
void MOrganizer_setSoundContext(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    organizer_set(rt, ctx, kArgumentAsSoundContext, kOrganizerSetSoundContext);
}
std::uint32_t organizer_core(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    return self_core(rt, ctx, kAsOrganizer, kOrganizerCore);
}

// MOrganizer.getEntryMaxAtCurrentRace() -> integer; canEnterCourse() -> boolean.
void MOrganizer_getEntryMaxAtCurrentRace(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t max = call_guest(rt, ctx, kOrganizerEntryMax, {organizer_core(rt, ctx)});
    GuestFrame frame(ctx, 0x10u);
    return_integer(rt, ctx, result, frame.at(0u), max);
}
void MOrganizer_canEnterCourse(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t can = call_guest(rt, ctx, kOrganizerCanEnterCourse, {organizer_core(rt, ctx)});
    GuestFrame frame(ctx, 0x10u);
    return_bool(rt, ctx, result, frame.at(0u), can);
}

// MOrganizer.setChangeableDriverParameterAtBeginningOfSession(entry, slot or nil, driver parameter),
// with at least three arguments.
void MOrganizer_setChangeableDriverParameterAtBeginningOfSession(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 2) return;
    const std::uint32_t core = organizer_core(rt, ctx);
    const std::uint32_t entry = int_argument(rt, ctx, argv, 0u);
    const std::uint32_t slot = rt.memory().load32(argv + 4u) != 0u ? int_argument(rt, ctx, argv, 1u) : 0xFFFFFFFFu;
    const std::uint32_t driver = self_core(rt, ctx, kSelfAsCarDriverParameter, kCarDriverParameterCore, argv + 8u, true);
    (void)call_guest(rt, ctx, kOrganizerSetChangeableDriver, {core, entry, slot, driver});
}

// MOrganizer.getEntriedDriverParameter(entry[, slot]) -> the driver parameter, if there is one.
void MOrganizer_getEntriedDriverParameter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    const std::uint32_t core = organizer_core(rt, ctx);
    const auto entry = static_cast<std::int32_t>(int_argument(rt, ctx, argv, 0u));
    if (entry < 0) return;
    const std::uint32_t slot = argc > 1 ? int_argument(rt, ctx, argv, 1u) : 0xFFFFFFFFu;
    const std::uint32_t driver = call_guest(rt, ctx, kOrganizerEntriedDriver, {core, static_cast<std::uint32_t>(entry), slot});
    if (driver != 0u) return_wrapped(rt, ctx, result, kWrapDriverParameter, driver);
}

// MOrganizer.getCurrentRaceParameter() -> the current race's parameter.
void MOrganizer_getCurrentRaceParameter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t race = call_guest(rt, ctx, kOrganizerCurrentRace, {organizer_core(rt, ctx)});
    return_wrapped(rt, ctx, result, kWrapRaceParameter, race + 4u);
}

// --- MOrganizer ---
void MOrganizer_setDrivingLineDispFlag(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 1) return;
    const std::uint32_t core = organizer_core(rt, ctx);
    const std::uint32_t entry = int_argument(rt, ctx, argv, 0u);
    const std::uint32_t flag_object = rt.memory().load32(argv + 4u);
    const std::uint32_t flag = call_guest(rt, ctx, virtual_function(rt, flag_object, kToBoolSlot), {flag_object});
    (void)call_guest(rt, ctx, kOrganizerSetDrivingLineDisp, {core, entry, flag});
}
void organizer_set_two(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t set) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 1) return;
    const std::uint32_t core = organizer_core(rt, ctx);
    const std::uint32_t a = int_argument(rt, ctx, argv, 0u);
    (void)call_guest(rt, ctx, set, {core, a, int_argument(rt, ctx, argv, 1u)});
}
void MOrganizer_setCameraTarget(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { organizer_set_two(rt, ctx, kOrganizerSetCameraTarget); }
void organizer_set_one(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t set) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    const std::uint32_t core = organizer_core(rt, ctx);
    (void)call_guest(rt, ctx, set, {core, int_argument(rt, ctx, argv, 0u)});
}
void MOrganizer_setNoInputLimitSecond(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { organizer_set_one(rt, ctx, kOrganizerSetNoInputLimit); }
void MOrganizer_setApplicationEndSessionSecond(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { organizer_set_one(rt, ctx, kOrganizerSetEndSessionSecond); }
void MOrganizer_getEntryControlType(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t self = ctx.gpr[5];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    const std::uint32_t entry = int_argument(rt, ctx, argv, 0u);  // before self, as the original
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t holder = frame.at(0u);
    (void)call_guest(rt, ctx, kAsOrganizer, {holder, self});
    const std::uint32_t core = call_guest(rt, ctx, kOrganizerCore, {rt.memory().load32(holder)});
    const std::uint32_t race = call_guest(rt, ctx, kOrganizerCurrentRace, {core});
    destroy(rt, ctx, holder);
    return_int(rt, ctx, result, call_guest(rt, ctx, kRaceStateEntryControlType, {race, entry}));
}
void MOrganizer_hasLoadingCarDone(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    return_boolean(rt, ctx, result, call_guest(rt, ctx, kOrganizerHasLoadingCarDone, {organizer_core(rt, ctx)}));
}
void MOrganizer_getCamearType(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t camera = call_guest(rt, ctx, kOrganizerCamera, {organizer_core(rt, ctx)});
    return_int(rt, ctx, result, call_guest(rt, ctx, kCameraType, {camera, 1u}));
}
void MOrganizer_setRaceDisplayTarget(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 1) return;
    const std::uint32_t display = call_guest(rt, ctx, kOrganizerRaceDisplay, {organizer_core(rt, ctx)});
    const std::uint32_t a = int_argument(rt, ctx, argv, 0u);
    (void)call_guest(rt, ctx, kRaceDisplaySetTarget, {display, a, int_argument(rt, ctx, argv, 1u)});
}
void MOrganizer_getGameCondition(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t condition = call_guest(rt, ctx, kOrganizerGameCondition, {organizer_core(rt, ctx)});
    return_wrapped(rt, ctx, result, kWrapGameCondition, condition);
}
void MOrganizer_getRaceSound(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t organizer = frame.at(0x0u);
    const std::uint32_t sound = frame.at(0x4u);
    (void)call_guest(rt, ctx, kAsOrganizer, {organizer});
    const std::uint32_t source = call_guest(rt, ctx, kOrganizerRaceSoundHolder, {rt.memory().load32(organizer)});
    (void)call_guest(rt, ctx, kHolderCopyRaceSound, {sound, source});
    destroy(rt, ctx, organizer);
    if (result != sound) (void)call_guest(rt, ctx, kHolderReset, {result, rt.memory().load32(sound)});
    destroy(rt, ctx, sound);
}
void MOrganizer_setBackRaceParameter(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    const std::uint32_t race = call_guest(rt, ctx, kOrganizerCurrentRace, {organizer_core(rt, ctx)});
    const std::uint32_t parameter = self_core(rt, ctx, kSelfAsRaceParameter, kRaceParameterCore, argv, true);
    (void)call_guest(rt, ctx, kRaceStateSetBackRace, {race + 0xCu, parameter});
}

} // namespace gt::game::organizer

namespace organizer_registrations {
using gt::ReturnKind;
#define ORGANIZER(address, name, kind) GT_DECOMP_NESTED(address, gt::game::organizer::name, ReturnKind::kind)

ORGANIZER(0x0885E424u, func_0885E424, Void);
ORGANIZER(0x0885EC1Cu, func_0885EC1C, Word);
ORGANIZER(0x0885E8B8u, can_enter_course, Word);
ORGANIZER(0x0885E848u, request_loading_course, Void);
ORGANIZER(0x0885EE58u, func_0885EE58, Word);
ORGANIZER(0x0885EE70u, func_0885EE70, Word);
ORGANIZER(0x0885E758u, func_0885E758, Void);
ORGANIZER(0x0885EF44u, func_0885EF44, Void);
ORGANIZER(0x0885EF50u, func_0885EF50, Void);
ORGANIZER(0x0885EF18u, func_0885EF18, Void);
ORGANIZER(0x0885E734u, set_window_max, Void);
ORGANIZER(0x0885E6E0u, func_0885E6E0, Void);
ORGANIZER(0x0885E498u, func_0885E498, Void);
ORGANIZER(0x0885EC80u, func_0885EC80, Void);
ORGANIZER(0x0885ED58u, func_0885ED58, Void);
ORGANIZER(0x0885EF78u, func_0885EF78, Void);
ORGANIZER(0x0882A530u, MOrganizer_startGameObjectLoop, Void);
// Not comparable yet (sleeps/wakes PSP threads inside a case): ORGANIZER(0x08829E58u, MOrganizer_endSession, Void);
// Not comparable yet (sleeps/wakes PSP threads inside a case): ORGANIZER(0x0882A068u, MOrganizer_exitCourse, Void);
ORGANIZER(0x0882A0C0u, MOrganizer_enterCourse, Void);
// Not comparable yet (sleeps/wakes PSP threads inside a case): ORGANIZER(0x0882A1C8u, MOrganizer_requestLoadingCourse, Void);
ORGANIZER(0x0882A378u, MOrganizer_enableSoundingRace, Void);
ORGANIZER(0x0882A320u, MOrganizer_disableSoundingRace, Void);
ORGANIZER(0x0882A428u, MOrganizer_enableRenderingRace, Void);
ORGANIZER(0x0882A3D0u, MOrganizer_disableRenderingRace, Void);
ORGANIZER(0x0882FE1Cu, MOrganizer_getEntryMaxAtCurrentRace, Void);
ORGANIZER(0x0882F498u, MOrganizer_canEnterCourse, Void);
ORGANIZER(0x0882B9C4u, MOrganizer_setChangeableDriverParameterAtBeginningOfSession, Void);
ORGANIZER(0x08830874u, MOrganizer_getEntriedDriverParameter, Void);
ORGANIZER(0x0883332Cu, MOrganizer_getCurrentRaceParameter, Void);
ORGANIZER(0x088297E8u, MOrganizer_setRaceSound, Void);
ORGANIZER(0x08829868u, MOrganizer_setSoundContext, Void);
ORGANIZER(0x0882BFA0u, MOrganizer_setDrivingLineDispFlag, Void);
ORGANIZER(0x0882AFB4u, MOrganizer_setCameraTarget, Void);
ORGANIZER(0x0882FEB0u, MOrganizer_getEntryControlType, Void);
ORGANIZER(0x0882F404u, MOrganizer_hasLoadingCarDone, Void);
ORGANIZER(0x0882BAD4u, MOrganizer_setRaceDisplayTarget, Void);
ORGANIZER(0x0882B50Cu, MOrganizer_setNoInputLimitSecond, Void);
ORGANIZER(0x0882B3C0u, MOrganizer_setApplicationEndSessionSecond, Void);
ORGANIZER(0x08832BB4u, MOrganizer_getGameCondition, Void);
ORGANIZER(0x08833C24u, MOrganizer_getRaceSound, Void);
ORGANIZER(0x08829A78u, MOrganizer_setBackRaceParameter, Void);
// Not registered: compared 0 in every fast, dense and full run of the PR (the pad routes never call them on their own,
// and no decompiled GT_DECOMP_NESTED caller reached them either), so nothing could verify them; they run as the recompiled originals.
// ORGANIZER(0x0885E778u, enable_rendering_race, Void);
// ORGANIZER(0x0885E788u, disable_rendering_race, Void);
// ORGANIZER(0x0885E794u, enable_sounding_race, Void);
// ORGANIZER(0x0885E7B8u, disable_sounding_race, Void);
// ORGANIZER(0x08860058u, start_game_object_loop, Void);
// ORGANIZER(0x0885E91Cu, has_loading_car_done, Word);
// ORGANIZER(0x0885E9ECu, set_application_end_session_second, Void);
// ORGANIZER(0x0885E9F8u, set_no_input_limit_second, Void);
// ORGANIZER(0x0885FC4Cu, enter_course, Void);
// ORGANIZER(0x0885E728u, window_max, Word);
// ORGANIZER(0x0885ECF4u, get_entried_driver_parameter, Word);
// ORGANIZER(0x0882E98Cu, MOrganizer_getCamearType, Void);
// 0x0885EFE0, 0x0885EFEC (setters) and 0x0885EE04 (get_game_condition, the address of +0x3F88) are registered as
// template instances (PTR_SET_FIELD, PTR_MEMBER_ADDRESS) in src/core/chained.cpp, reached by the routes there.
#undef ORGANIZER
} // namespace organizer_registrations
