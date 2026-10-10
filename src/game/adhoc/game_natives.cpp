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
constexpr std::uint32_t kCarParameterCarCode = 0x088A5EE0u;  // (engine object) -> 64-bit car code
constexpr std::uint32_t kRaceOperatorTargetSlot = 0x0885BBF0u;  // (operator engine object, index) -> slot
constexpr std::uint32_t kRaceOperatorIsOnline = 0x0885BE14u;    // (operator engine object) -> bool
constexpr std::uint32_t kSelfAsMusic = 0x08A3A9D0u;          // (holder *out) [a1 = self]
constexpr std::uint32_t kMusicIsPlaying = 0x08A3A788u;       // (music *) -> int
constexpr std::uint32_t kSelfAsCarCollection = 0x0881BD98u;  // (holder *out) [a1 = self]
constexpr std::uint32_t kCarCollectionCore = 0x0881B068u;    // (collection *) -> its engine object
constexpr std::uint32_t kCarCollectionFind = 0x0881BF5Cu;    // (core, argc, argv, u64 *code, int *index) -> kind
constexpr std::uint32_t kCarCollectionHasKind1 = 0x0881A8F0u;  // (core, -, u64 code) -> count
constexpr std::uint32_t kCarCollectionHasKind2 = 0x0881A8C0u;  // (core, -, u64 code, index) -> count
constexpr std::uint32_t kSelfAsCalendar = 0x08817FCCu;       // (holder *out) [a1 = self]
constexpr std::uint32_t kCalendarToday = 0x08817A54u;        // (calendar *) -> int * (the current day)
constexpr std::uint32_t kCalendarFirstDay = 0x08817924u;     // () -> int
constexpr std::uint32_t kOrganizerEntryMax = 0x0885EC1Cu;    // (organizer core) -> int
constexpr std::uint32_t kOrganizerCanEnterCourse = 0x0885E8B8u;  // (organizer core) -> bool
constexpr std::uint32_t kOrganizerSetChangeableDriver = 0x0885ED58u;  // (core, entry, slot, driver parameter core)
constexpr std::uint32_t kOrganizerEntriedDriver = 0x0885ECF4u;  // (core, entry, slot) -> driver parameter or 0
constexpr std::uint32_t kOrganizerCurrentRace = 0x0885EDF8u;    // (core) -> race state (race parameter at +4)
constexpr std::uint32_t kWrapDriverParameter = 0x0884F674u;  // (holder *out, driver parameter *)
constexpr std::uint32_t kWrapRaceParameter = 0x08848DB8u;    // (holder *out, race parameter *)
constexpr std::uint32_t kCarParameterHasTarget = 0x088A5928u;  // (core) -> int
constexpr std::uint32_t kCarParameterIsRemote = 0x088A5984u;   // (core) -> int
constexpr std::uint32_t kDriverParameterIsHuman = 0x08897788u; // (core) -> bool
constexpr std::uint32_t kDriverParameterSetAILevel = 0x08897290u;  // (core, level)
constexpr std::uint32_t kSelfAsRaceParameter = 0x08847008u;  // (holder *out) [a1 = self]
constexpr std::uint32_t kRaceParameterCore = 0x08846E50u;    // (race parameter *) -> engine object
constexpr std::uint32_t kRaceParameterSetLaunchPosition = 0x088D59A0u;  // (core, a, b)
constexpr std::uint32_t kRaceParameterSetLaunchSpeed = 0x088D4E3Cu;     // (core, a, (u8)b)
constexpr std::uint32_t kSoundPlay = 0x08A34DA8u;            // (sound *, std::string name)
constexpr std::uint32_t kSoundPlayWith = 0x08A34D98u;        // (sound *, std::string name, float in $f12)
constexpr std::uint32_t kSelfAsMenuObjectManager = 0x08A14610u;  // (holder *out) [a1 = self]
constexpr std::uint32_t kMenuObjectManagerStart = 0x08A142C8u;   // (manager *, const std::string *name)

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

// MCarParameter.getCarCode() -> the 64-bit code of the parameter's car.
void MCarParameter_getCarCode(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    std::uint32_t core = 0u;
    with_self(rt, ctx, kSelfAsCarParameter, [&](std::uint32_t object) { core = call_guest(rt, ctx, kCarParameterCore, {object}); });
    const GuestResult code = call_guest(rt, ctx, kCarParameterCarCode, {core}, {});
    GuestFrame frame(ctx, 0x20u);
    return_long(rt, ctx, result, frame.at(0u), code.v0, code.v1);
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

// MMusic.isPlaying() -> integer (the engine's state, not a boolean).
void MMusic_isPlaying(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    with_self(rt, ctx, kSelfAsMusic, [&](std::uint32_t music) {
        GuestFrame frame(ctx, 0x20u);
        return_integer(rt, ctx, result, frame.at(0u), call_guest(rt, ctx, kMusicIsPlaying, {music}));
    });
}

// MCarCollection.hasCar(car...) -> boolean: the arguments name a car (its code, plus an index for
// kind 2); no car (code -1) gives false, otherwise whether the collection holds at least one.
void MCarCollection_hasCar(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    auto &memory = rt.memory();
    std::uint32_t core = 0u;
    with_self(rt, ctx, kSelfAsCarCollection, [&](std::uint32_t collection) { core = call_guest(rt, ctx, kCarCollectionCore, {collection}); });
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t index = frame.at(0x0u);
    const std::uint32_t code = frame.at(0x8u);  // u64, 8-aligned
    const std::uint32_t kind =
        call_guest(rt, ctx, kCarCollectionFind, {core, static_cast<std::uint32_t>(argc), argv, code, index});
    const std::uint32_t low = memory.load32(code);
    const std::uint32_t high = memory.load32(code + 4u);
    std::uint32_t has = 0u;
    if (low != 0xFFFFFFFFu || high != 0xFFFFFFFFu) {
        std::int32_t count = 0;
        if (kind == 1u) count = static_cast<std::int32_t>(call_guest(rt, ctx, kCarCollectionHasKind1, {core, 0u, low, high}));
        else if (kind == 2u)
            count = static_cast<std::int32_t>(
                call_guest(rt, ctx, kCarCollectionHasKind2, {core, 0u, low, high, memory.load32(index)}));
        has = count > 0 ? 1u : 0u;
    }
    GuestFrame value(ctx, 0x20u);
    return_bool(rt, ctx, result, value.at(0u), has);
}

// self as `self_as`, then its engine object (`core_of`); the holder is dropped before returning.
std::uint32_t self_core(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t self_as, std::uint32_t core_of,
                        std::uint32_t self_holder_arg = 0u, bool explicit_self = false) {
    std::uint32_t core = 0u;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t holder = frame.at(0u);
    if (explicit_self) (void)call_guest(rt, ctx, self_as, {holder, self_holder_arg});
    else (void)call_guest(rt, ctx, self_as, {holder});
    core = call_guest(rt, ctx, core_of, {rt.memory().load32(holder)});
    destroy(rt, ctx, holder);
    return core;
}
std::uint32_t organizer_core(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    return self_core(rt, ctx, kAsOrganizer, kOrganizerCore);
}

// Argument `index` as an integer (its toInt()).
std::uint32_t int_argument(psprecomp::Runtime &rt, const psprecomp::AllegrexContext &ctx, std::uint32_t argv, std::uint32_t index) {
    return static_cast<std::uint32_t>(to_int(rt, ctx, rt.memory().load32(argv + 4u * index)));
}

// *result = the holder `wrap(out, object)` fills.
void return_wrapped(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t result, std::uint32_t wrap,
                    std::uint32_t object) {
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t out = frame.at(0u);
    (void)call_guest(rt, ctx, wrap, {out, object});
    if (result != out) (void)call_guest(rt, ctx, kHolderReset, {result, rt.memory().load32(out)});
    destroy(rt, ctx, out);
}

// MCalendar.getGameDay() -> integer: days since the calendar's first day.
void MCalendar_getGameDay(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t today = self_core(rt, ctx, kSelfAsCalendar, kCalendarToday);
    const std::uint32_t day = rt.memory().load32(today);
    const std::uint32_t first = call_guest(rt, ctx, kCalendarFirstDay, {});
    GuestFrame frame(ctx, 0x10u);
    return_integer(rt, ctx, result, frame.at(0u), day - first);
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

// MCarParameter.isTargetLocal() -> integer: it has a target and the target isn't remote.
void MCarParameter_isTargetLocal(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t core = self_core(rt, ctx, kSelfAsCarParameter, kCarParameterCore);
    std::uint32_t local = 0u;
    if (call_guest(rt, ctx, kCarParameterHasTarget, {core}) != 0u && call_guest(rt, ctx, kCarParameterIsRemote, {core}) == 0u) {
        local = 1u;
    }
    GuestFrame frame(ctx, 0x10u);
    return_integer(rt, ctx, result, frame.at(0u), local);
}

// MCarDriverParameter.isHuman() -> boolean; setAILevel(level), with at least one argument.
void MCarDriverParameter_isHuman(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t core = self_core(rt, ctx, kSelfAsCarDriverParameter, kCarDriverParameterCore);
    const std::uint32_t human = call_guest(rt, ctx, kDriverParameterIsHuman, {core});
    GuestFrame frame(ctx, 0x10u);
    return_bool(rt, ctx, result, frame.at(0u), human);
}
void MCarDriverParameter_setAILevel(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    const std::uint32_t core = self_core(rt, ctx, kSelfAsCarDriverParameter, kCarDriverParameterCore);
    (void)call_guest(rt, ctx, kDriverParameterSetAILevel, {core, int_argument(rt, ctx, argv, 0u)});
}

// MRaceParameter.setLaunchPosition(a, b) / setLaunchSpeed(a, b): with at least two arguments
// (setLaunchSpeed passes b as a byte).
void race_parameter_set(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t set, std::uint32_t mask) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 1) return;
    const std::uint32_t core = self_core(rt, ctx, kSelfAsRaceParameter, kRaceParameterCore);
    const std::uint32_t a = int_argument(rt, ctx, argv, 0u);
    const std::uint32_t b = int_argument(rt, ctx, argv, 1u) & mask;
    (void)call_guest(rt, ctx, set, {core, a, b});
}
void MRaceParameter_setLaunchPosition(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    race_parameter_set(rt, ctx, kRaceParameterSetLaunchPosition, 0xFFFFFFFFu);
}
void MRaceParameter_setLaunchSpeed(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    race_parameter_set(rt, ctx, kRaceParameterSetLaunchSpeed, 0xFFu);
}

// MSound.play(name[, value]): plays the named sound (with a float parameter when given).
void MSound_play(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[5];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    auto &memory = rt.memory();
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t name = frame.at(0x0u);
    const std::uint32_t sound = frame.at(0x4u);
    to_string(rt, ctx, memory.load32(argv), name);
    (void)call_guest(rt, ctx, kSelfAsSound, {sound, self});
    if (argc < 2) {
        (void)call_guest(rt, ctx, kSoundPlay, {memory.load32(sound), memory.load32(name)});
    } else {
        const float value = to_float(rt, ctx, memory.load32(argv + 4u));
        (void)call_guest(rt, ctx, kSoundPlayWith, {memory.load32(sound), memory.load32(name)}, {value});
    }
    destroy(rt, ctx, sound);
    string_release(rt, ctx, memory.load32(name));
}

// MMenuGameObjectManager.start(name), with at least one argument.
void MMenuGameObjectManager_start(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    auto &memory = rt.memory();
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t manager = frame.at(0x0u);
    const std::uint32_t name = frame.at(0x4u);
    (void)call_guest(rt, ctx, kSelfAsMenuObjectManager, {manager});
    to_string(rt, ctx, memory.load32(argv), name);
    (void)call_guest(rt, ctx, kMenuObjectManagerStart, {memory.load32(manager), name});
    string_release(rt, ctx, memory.load32(name));
    destroy(rt, ctx, manager);
}

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
GT_DECOMP(0x088558F8u, gt::adhoc::game::MCarParameter_getCarCode, gt::ReturnKind::Void);
GT_DECOMP(0x0882689Cu, gt::adhoc::game::MRaceOperator_getTargetSlot, gt::ReturnKind::Void);
GT_DECOMP(0x08826070u, gt::adhoc::game::MRaceOperator_isOnline, gt::ReturnKind::Void);
GT_DECOMP(0x08A3B220u, gt::adhoc::game::MMusic_isPlaying, gt::ReturnKind::Void);
GT_DECOMP(0x0881D538u, gt::adhoc::game::MCarCollection_hasCar, gt::ReturnKind::Void);
GT_DECOMP(0x08818560u, gt::adhoc::game::MCalendar_getGameDay, gt::ReturnKind::Void);
GT_DECOMP(0x0882FE1Cu, gt::adhoc::game::MOrganizer_getEntryMaxAtCurrentRace, gt::ReturnKind::Void);
GT_DECOMP(0x0882F498u, gt::adhoc::game::MOrganizer_canEnterCourse, gt::ReturnKind::Void);
GT_DECOMP(0x0882B9C4u, gt::adhoc::game::MOrganizer_setChangeableDriverParameterAtBeginningOfSession, gt::ReturnKind::Void);
GT_DECOMP(0x08830874u, gt::adhoc::game::MOrganizer_getEntriedDriverParameter, gt::ReturnKind::Void);
GT_DECOMP(0x0883332Cu, gt::adhoc::game::MOrganizer_getCurrentRaceParameter, gt::ReturnKind::Void);
GT_DECOMP(0x0885600Cu, gt::adhoc::game::MCarParameter_isTargetLocal, gt::ReturnKind::Void);
GT_DECOMP(0x0884D81Cu, gt::adhoc::game::MCarDriverParameter_isHuman, gt::ReturnKind::Void);
GT_DECOMP(0x0884D77Cu, gt::adhoc::game::MCarDriverParameter_setAILevel, gt::ReturnKind::Void);
GT_DECOMP(0x088472C0u, gt::adhoc::game::MRaceParameter_setLaunchPosition, gt::ReturnKind::Void);
GT_DECOMP(0x088471E4u, gt::adhoc::game::MRaceParameter_setLaunchSpeed, gt::ReturnKind::Void);
GT_DECOMP(0x08A36608u, gt::adhoc::game::MSound_play, gt::ReturnKind::Void);
// Not comparable yet (sleeps the thread: sceKernelSleepThread), so not swapped in: GT_DECOMP(0x08A1511Cu, gt::adhoc::game::MMenuGameObjectManager_start, gt::ReturnKind::Void);
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
