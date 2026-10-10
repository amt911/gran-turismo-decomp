// Adhoc natives of GT's game-side classes (names from GT's registrations, config/symbol_addrs.txt).
// Same shape as the UI natives: fetch self as the engine class (helpers take the native's self in
// a1, left untouched here unless an argument is converted instead), call the engine, drop holders.

#include "decomp_registry.hpp"
#include "value.hpp"

namespace gt::adhoc::game {
namespace {

constexpr std::uint32_t kHolderReset = 0x08A5BAD8u;
constexpr std::uint32_t kHolderDestroy = 0x08A5B9ECu;

constexpr std::uint32_t kSelfAsWatcher = 0x08973EF8u;        // (holder *out) [a1 = self]
constexpr std::uint32_t kWatcherAppend = 0x08973DD0u;        // (watcher *)
constexpr std::uint32_t kWatcherRemoveSlot = 66u;            // watcher->remove()
constexpr std::uint32_t kAsOrganizer = 0x088297A8u;          // (holder *out, self or argument)
constexpr std::uint32_t kOrganizerCore = 0x088294D0u;        // (organizer *) -> its engine object
constexpr std::uint32_t kOrganizerStartLoop = 0x08860058u;   // (engine object)
constexpr std::uint32_t kOrganizerEndSession = 0x0885F46Cu;
constexpr std::uint32_t kOrganizerExitCourse = 0x0885E940u;
constexpr std::uint32_t kOrganizerEnterCourse = 0x0885FC4Cu;
constexpr std::uint32_t kOrganizerRequestLoadingCourse = 0x0885E848u;
constexpr std::uint32_t kOrganizerEnableSoundingRace = 0x0885E794u;
constexpr std::uint32_t kOrganizerDisableSoundingRace = 0x0885E7B8u;
constexpr std::uint32_t kOrganizerEnableRenderingRace = 0x0885E778u;
constexpr std::uint32_t kOrganizerDisableRenderingRace = 0x0885E788u;
constexpr std::uint32_t kSelfAsCarParameter = 0x088547C4u;   // (holder *out) [a1 = self]
constexpr std::uint32_t kCarParameterCore = 0x08854678u;     // (car parameter *) -> engine object
constexpr std::uint32_t kCarParameterBeginPartsChange = 0x088A5E54u;
constexpr std::uint32_t kCarParameterEndPartsChange = 0x088A5CD0u;
constexpr std::uint32_t kCarParameterBeginInfo = 0x088A61A4u;
constexpr std::uint32_t kCarParameterEndInfo = 0x088A5A0Cu;
constexpr std::uint32_t kCarParameterUpdateScratch = 0x088A6FB4u;
constexpr std::uint32_t kCarSettingsBegin = 0x088A657Cu;
constexpr std::uint32_t kCarSettingsEnd = 0x088A5CD8u;
constexpr std::uint32_t kSelfAsCarDriverParameter = 0x0884D660u;  // (holder *out) [a1 = self]
constexpr std::uint32_t kCarDriverParameterCore = 0x0884D3D8u;
constexpr std::uint32_t kCarDriverParameterSetDefault = 0x0889793Cu;
constexpr std::uint32_t kArgumentAsRaceSound = 0x08835BD8u;  // (holder *out, argv)
constexpr std::uint32_t kOrganizerSetRaceSound = 0x08829538u;   // (organizer *, holder *)
constexpr std::uint32_t kArgumentAsSoundContext = 0x08A22050u;  // (holder *out, argv)
constexpr std::uint32_t kOrganizerSetSoundContext = 0x088294D8u; // (organizer *, holder *)
constexpr std::uint32_t kSelfAsRaceOperator = 0x088248E4u;   // (holder *out) [a1 = self]
constexpr std::uint32_t kRaceOperatorSetOrganizer = 0x0885BB9Cu; // (operator engine object, organizer object)
constexpr std::uint32_t kCarLabelCount = 0x08931090u;        // () -> int
constexpr std::uint32_t kCarCode = 0x0883B848u;              // (argv) -> 64-bit car code
constexpr std::uint32_t kCourseCode = 0x0883B950u;           // (argv) -> 64-bit course code
constexpr std::uint32_t kCarCategory = 0x08930D98u;          // (u64 car code) -> category
constexpr std::uint32_t kCourseCondition = 0x0893069Cu;      // (u64 course code) -> condition
constexpr std::uint32_t kParentalLevel = 0x08ABDA84u;        // () -> int
constexpr std::uint32_t kSelfAsSound = 0x08A354ECu;          // (holder *out) [a1 = self]
constexpr std::uint32_t kSoundStopMusic = 0x08A35088u;       // (sound *)
constexpr std::uint32_t kGameRecordStructure = 0x08CE28E4u;  // global: the game-record structure object
constexpr std::uint32_t kHolderCopyRecord = 0x08A53224u;     // holder copy (an instance used here)
constexpr std::uint32_t kArgumentAsRecord = 0x08A57854u;     // (holder *out, argv): argument as STRecord
constexpr std::uint32_t kInitializeGameRecord = 0x08805D9Cu; // (record data)
constexpr std::uint32_t kEngineObject = 0x14u;               // MRaceOperator: its engine object

void destroy(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t holder) {
    (void)call_guest(rt, ctx, kHolderDestroy, {holder});
}

// self as `cls` into a frame holder, `use(object)`, then drop the holder.
template <typename Use>
void with_self(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t self_as, Use use) {
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t holder = frame.at(0u);
    (void)call_guest(rt, ctx, self_as, {holder});
    use(rt.memory().load32(holder));  // NULL is not checked (the originals only report it)
    destroy(rt, ctx, holder);
}

} // namespace

void MWatcher_append(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    with_self(rt, ctx, kSelfAsWatcher, [&](std::uint32_t watcher) { (void)call_guest(rt, ctx, kWatcherAppend, {watcher}); });
}

void MWatcher_remove(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    with_self(rt, ctx, kSelfAsWatcher, [&](std::uint32_t watcher) {
        (void)call_guest(rt, ctx, virtual_function(rt, watcher, kWatcherRemoveSlot), {watcher});
    });
}

// The common shape of argument-less natives: self as the engine class, its engine object
// (`core_of`), then `engine(core)` — called after the holder is dropped (`DropFirst::yes`) or before
// (`DropFirst::no`), as each original does.
enum class DropFirst { no, yes };
void core_call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t self_as, std::uint32_t core_of,
               std::uint32_t engine, DropFirst drop_first) {
    std::uint32_t core = 0u;
    with_self(rt, ctx, self_as, [&](std::uint32_t object) {
        core = call_guest(rt, ctx, core_of, {object});
        if (drop_first == DropFirst::no) (void)call_guest(rt, ctx, engine, {core});
    });
    if (drop_first == DropFirst::yes) (void)call_guest(rt, ctx, engine, {core});
}

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

// MCarParameter: a car's parameter set (parts, info). begin*/end* bracket changes to it.
void car_parameter_call(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t engine, DropFirst drop_first) {
    core_call(rt, ctx, kSelfAsCarParameter, kCarParameterCore, engine, drop_first);
}
void MCarParameter_beginPartsChange(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_parameter_call(rt, ctx, kCarParameterBeginPartsChange, DropFirst::no); }
void MCarParameter_endPartsChange(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_parameter_call(rt, ctx, kCarParameterEndPartsChange, DropFirst::no); }
void MCarParameter_beginInfo(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_parameter_call(rt, ctx, kCarParameterBeginInfo, DropFirst::no); }
void MCarParameter_endInfo(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_parameter_call(rt, ctx, kCarParameterEndInfo, DropFirst::no); }
void MCarParameter_updateScratch(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_parameter_call(rt, ctx, kCarParameterUpdateScratch, DropFirst::yes); }

// The car-settings class registered by initializer 0x04B808 (suspension, gears, wing, ... — its
// script name isn't known yet). It takes self as MCarParameter, like the class above.
void CarSettings_beginSetting(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_parameter_call(rt, ctx, kCarSettingsBegin, DropFirst::no); }
void CarSettings_endSetting(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_parameter_call(rt, ctx, kCarSettingsEnd, DropFirst::no); }

// MCarDriverParameter.setDefault().
void MCarDriverParameter_setDefault(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    core_call(rt, ctx, kSelfAsCarDriverParameter, kCarDriverParameterCore, kCarDriverParameterSetDefault, DropFirst::yes);
}

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

// MSpecDB.getCarLabelCount() -> integer.
void MSpecDB_getCarLabelCount(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    return_integer(rt, ctx, result, frame.at(0u), call_guest(rt, ctx, kCarLabelCount, {}));
}

// MSpecDB.getCarCode(label) -> 64-bit car code (module function: result, argc, argv).
void MSpecDB_getCarCode(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    const std::uint32_t argv = ctx.gpr[6];
    if (argc <= 0) return;
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t value = frame.at(0u);
    const GuestResult code = call_guest(rt, ctx, kCarCode, {argv}, {});
    return_long(rt, ctx, result, value, code.v0, code.v1);
}

// MSpecDB.getCarCategory(car) / getCourseCondition(course) / isDirt(course): look the code up and
// return the integer (isDirt: whether the course condition is 2 or 3). Module functions:
// (result, argc, argv); nothing happens without an argument.
void spec_lookup(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t code_of, std::uint32_t lookup,
                 bool dirt) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    const std::uint32_t argv = ctx.gpr[6];
    if (argc <= 0) return;
    GuestFrame frame(ctx, 0x20u);
    const GuestResult code = call_guest(rt, ctx, code_of, {argv}, {});
    const std::uint32_t value = call_guest(rt, ctx, lookup, {code.v0, code.v1});
    if (dirt) return_bool(rt, ctx, result, frame.at(0u), value - 2u < 2u ? 1u : 0u);
    else return_integer(rt, ctx, result, frame.at(0u), value);
}
void MSpecDB_getCarCategory(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { spec_lookup(rt, ctx, kCarCode, kCarCategory, false); }
void MSpecDB_getCourseCondition(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { spec_lookup(rt, ctx, kCourseCode, kCourseCondition, false); }
void MSpecDB_isDirt(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { spec_lookup(rt, ctx, kCourseCode, kCourseCondition, true); }

// MProductInformation.GetParentalLevel() -> integer.
void MProductInformation_GetParentalLevel(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    return_integer(rt, ctx, result, frame.at(0u), call_guest(rt, ctx, kParentalLevel, {}));
}

// MSound.stopMusic().
void MSound_stopMusic(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    with_self(rt, ctx, kSelfAsSound, [&](std::uint32_t sound) { (void)call_guest(rt, ctx, kSoundStopMusic, {sound}); });
}

// GameRecord.CreateGameRecordStructure() -> the global game-record structure.
void CreateGameRecordStructure(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t raw = frame.at(0x0u);
    const std::uint32_t value = frame.at(0x4u);
    rt.memory().store32(raw, rt.memory().load32(kGameRecordStructure));
    (void)call_guest(rt, ctx, kHolderCopyRecord, {value, raw});
    if (result != value) (void)call_guest(rt, ctx, kHolderReset, {result, rt.memory().load32(value)});
    destroy(rt, ctx, value);
}

// GameRecord.InitializeGameRecord(record): initializes the record's data (its own, or its STData's).
void InitializeGameRecord(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    const std::uint32_t argv = ctx.gpr[6];
    if (argc <= 0) return;
    auto &memory = rt.memory();
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t holder = frame.at(0u);
    (void)call_guest(rt, ctx, kArgumentAsRecord, {holder, argv});
    const std::uint32_t record = memory.load32(holder);
    std::uint32_t data = memory.load32(record + 0x1Cu);
    if (data == 0u) data = memory.load32(memory.load32(record + 0x18u) + 0xCu);  // via its STData
    destroy(rt, ctx, holder);
    (void)call_guest(rt, ctx, kInitializeGameRecord, {data});
}

} // namespace gt::adhoc::game

GT_DECOMP(0x08974058u, gt::adhoc::game::MWatcher_append, gt::ReturnKind::Void);
GT_DECOMP(0x08974004u, gt::adhoc::game::MWatcher_remove, gt::ReturnKind::Void);
GT_DECOMP(0x0882A530u, gt::adhoc::game::MOrganizer_startGameObjectLoop, gt::ReturnKind::Void);
// Not comparable yet (sleeps/wakes PSP threads inside a case), so not swapped in: GT_DECOMP(0x08829E58u, gt::adhoc::game::MOrganizer_endSession, gt::ReturnKind::Void);
// Not comparable yet (sleeps/wakes PSP threads inside a case), so not swapped in: GT_DECOMP(0x0882A068u, gt::adhoc::game::MOrganizer_exitCourse, gt::ReturnKind::Void);
GT_DECOMP(0x0882A0C0u, gt::adhoc::game::MOrganizer_enterCourse, gt::ReturnKind::Void);
// Not comparable yet (sleeps/wakes PSP threads inside a case), so not swapped in: GT_DECOMP(0x0882A1C8u, gt::adhoc::game::MOrganizer_requestLoadingCourse, gt::ReturnKind::Void);
GT_DECOMP(0x0882A378u, gt::adhoc::game::MOrganizer_enableSoundingRace, gt::ReturnKind::Void);
GT_DECOMP(0x0882A320u, gt::adhoc::game::MOrganizer_disableSoundingRace, gt::ReturnKind::Void);
GT_DECOMP(0x0882A428u, gt::adhoc::game::MOrganizer_enableRenderingRace, gt::ReturnKind::Void);
GT_DECOMP(0x0882A3D0u, gt::adhoc::game::MOrganizer_disableRenderingRace, gt::ReturnKind::Void);
// Not comparable yet (reads files inside a case), so not swapped in: GT_DECOMP(0x08854A78u, gt::adhoc::game::MCarParameter_beginPartsChange, gt::ReturnKind::Void);
GT_DECOMP(0x08854ACCu, gt::adhoc::game::MCarParameter_endPartsChange, gt::ReturnKind::Void);
GT_DECOMP(0x08854B74u, gt::adhoc::game::MCarParameter_beginInfo, gt::ReturnKind::Void);
GT_DECOMP(0x08854BC8u, gt::adhoc::game::MCarParameter_endInfo, gt::ReturnKind::Void);
GT_DECOMP(0x08854C1Cu, gt::adhoc::game::MCarParameter_updateScratch, gt::ReturnKind::Void);
GT_DECOMP(0x0884F760u, gt::adhoc::game::CarSettings_beginSetting, gt::ReturnKind::Void);
GT_DECOMP(0x0884F7B4u, gt::adhoc::game::CarSettings_endSetting, gt::ReturnKind::Void);
GT_DECOMP(0x0884D9F0u, gt::adhoc::game::MCarDriverParameter_setDefault, gt::ReturnKind::Void);
GT_DECOMP(0x088297E8u, gt::adhoc::game::MOrganizer_setRaceSound, gt::ReturnKind::Void);
GT_DECOMP(0x08829868u, gt::adhoc::game::MOrganizer_setSoundContext, gt::ReturnKind::Void);
GT_DECOMP(0x08824E88u, gt::adhoc::game::MRaceOperator_setOrganizer, gt::ReturnKind::Void);
GT_DECOMP(0x0883BD04u, gt::adhoc::game::MSpecDB_getCarLabelCount, gt::ReturnKind::Void);
GT_DECOMP(0x0883BF90u, gt::adhoc::game::MSpecDB_getCarCode, gt::ReturnKind::Void);
GT_DECOMP(0x08805FD0u, gt::adhoc::game::CreateGameRecordStructure, gt::ReturnKind::Void);
GT_DECOMP(0x0883D9E8u, gt::adhoc::game::MSpecDB_getCarCategory, gt::ReturnKind::Void);
GT_DECOMP(0x0883F948u, gt::adhoc::game::MSpecDB_getCourseCondition, gt::ReturnKind::Void);
GT_DECOMP(0x08840FA8u, gt::adhoc::game::MSpecDB_isDirt, gt::ReturnKind::Void);
GT_DECOMP(0x08A27F20u, gt::adhoc::game::MProductInformation_GetParentalLevel, gt::ReturnKind::Void);
GT_DECOMP(0x08A35648u, gt::adhoc::game::MSound_stopMusic, gt::ReturnKind::Void);
GT_DECOMP(0x08806384u, gt::adhoc::game::InitializeGameRecord, gt::ReturnKind::Void);
