// Adhoc natives of GT's game-side classes (names from GT's registrations, config/symbol_addrs.txt).
// Same shape as the UI natives: fetch self as the engine class (helpers take the native's self in
// a1, left untouched here unless an argument is converted instead), call the engine, drop holders.

#include "decomp_registry.hpp"
#include "natives.hpp"

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
constexpr std::uint32_t kLoadContainer = 0x08808F2Cu;        // (holder *out, argc, argv, is_array, element factory)
constexpr std::uint32_t kCarArrayElement = 0x08808640u;      // element factory of loadCarArray
constexpr std::uint32_t kCourseMapElement = 0x088085B8u;     // element factory of loadCourseMap
constexpr std::uint32_t kSelfAsSceneFace = 0x0880B8F8u;      // (holder *out) [a1 = self]
constexpr std::uint32_t kSceneSetClearColor = 0x0880BBF4u;   // (scene *, bool)
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
constexpr std::uint32_t kRaceOperatorSlotCarParameter = 0x0885C188u;    // (engine, slot) -> car parameter or 0
constexpr std::uint32_t kRaceOperatorSlotDriverParameter = 0x0885C228u; // (engine, slot, index) -> driver parameter or 0
constexpr std::uint32_t kRaceOperatorIsTheater = 0x0885C35Cu;
constexpr std::uint32_t kRaceOperatorClear = 0x0885C3C8u;
constexpr std::uint32_t kRaceOperatorCheckIn = 0x0885D010u;
constexpr std::uint32_t kRaceOperatorAllocateResource = 0x0885BFE0u;
constexpr std::uint32_t kWrapCarParameter = 0x08857EE4u;      // (holder *out, car parameter *, 0)
constexpr std::uint32_t kOrganizerSetDrivingLineDisp = 0x0885E1F0u;   // (core, entry, bool)
constexpr std::uint32_t kOrganizerSetCameraTarget = 0x0885E6E0u;      // (core, a, b)
constexpr std::uint32_t kRaceStateEntryControlType = 0x088DC858u;     // (race state, entry) -> int
constexpr std::uint32_t kOrganizerHasLoadingCarDone = 0x0885E91Cu;
constexpr std::uint32_t kOrganizerCamera = 0x0885E710u;               // (core) -> camera
constexpr std::uint32_t kCameraType = 0x08917918u;                    // (camera, 1) -> int
constexpr std::uint32_t kOrganizerRaceDisplay = 0x0885EE58u;          // (core) -> race display
constexpr std::uint32_t kRaceDisplaySetTarget = 0x0889368Cu;          // (display, a, b)
constexpr std::uint32_t kOrganizerSetNoInputLimit = 0x0885E9F8u;      // (core, seconds)
constexpr std::uint32_t kOrganizerSetEndSessionSecond = 0x0885E9ECu;  // (core, seconds)
constexpr std::uint32_t kOrganizerGameCondition = 0x0885EE04u;        // (core) -> game condition
constexpr std::uint32_t kWrapGameCondition = 0x0884642Cu;             // (holder *out, game condition *)
constexpr std::uint32_t kOrganizerRaceSoundHolder = 0x08829314u;      // (organizer *) -> holder *
constexpr std::uint32_t kHolderCopyRaceSound = 0x08835AB8u;           // holder copy instance
constexpr std::uint32_t kRaceStateSetBackRace = 0x088D4824u;          // (race state + 0xC, race parameter core)
constexpr std::uint32_t kCarParameterPP = 0x088A5AE4u;                // (core) -> float
constexpr std::uint32_t kCarParameterTransmission = 0x088A6608u;      // (core) -> transmission info
constexpr std::uint32_t kTransmissionIsATOnly = 0x089325E0u;          // (info) -> bool
constexpr std::uint32_t kCarParameterPowerScratch = 0x088A7608u;
constexpr std::uint32_t kCarParameterSetRearTire = 0x088A6508u;       // (core, category) -> int
constexpr std::uint32_t kCarParameterSetFrontTire = 0x088A6494u;
constexpr std::uint32_t kDriverParameterSetPlayer = 0x08897554u;      // (core, player)
constexpr std::uint32_t kRaceParameterDelayStart = 0x088D4D14u;       // (core, index) -> int
constexpr std::uint32_t kRaceParameterUseLaunchData = 0x088D4E18u;    // (core, bool)
constexpr std::uint32_t kRaceParameterSetEventV = 0x088D5988u;        // (core, a, b)
constexpr std::uint32_t kRaceParameterSetBoostA = 0x088D4E68u;        // (core, side, value)
constexpr std::uint32_t kRaceParameterSetBoostB = 0x088D4EE4u;
constexpr std::uint32_t kRaceParameterSetBoostC = 0x088D4F48u;
constexpr std::uint32_t kSystemIsExitGame = 0x08B1A844u;              // () -> int
constexpr std::uint32_t kTemporalAntialias = 0x08CF6568u;             // u8 flag
constexpr std::uint32_t kEnterButtonQuery = 0x08C19BECu;              // (system parameter *) -> nonzero: circle
constexpr std::uint32_t kEnterButtonParameter = 0x08CF6540u;
constexpr std::uint32_t kTextCross = 0x08CABB64u;                     // "cross"
constexpr std::uint32_t kTextCircle = 0x08CABB5Cu;                    // "circle"
constexpr std::uint32_t kGameRegionCode = 0x08ABDC28u;                // () -> const char *
constexpr std::uint32_t kSelfAsController = 0x088378F0u;              // (holder *out, self holder *)
constexpr std::uint32_t kControllerCore = 0x08837498u;                // (controller *, 0) -> config owner
constexpr std::uint32_t kWrapControllerConfig = 0x08838380u;          // (holder *out, owner, index)
constexpr std::uint32_t kSoundFadeOutMusic = 0x08A35040u;   // (seconds in $f12, sound *)
constexpr std::uint32_t kMusicFadeIn = 0x08A3A3E4u;         // (seconds in $f12, music *, std::string name)
constexpr std::uint32_t kSoundPlay = 0x08A34DA8u;            // (sound *, std::string name)
constexpr std::uint32_t kSoundPlayWith = 0x08A34D98u;        // (sound *, std::string name, float in $f12)
constexpr std::uint32_t kSelfAsMenuObjectManager = 0x08A14610u;  // (holder *out) [a1 = self]
constexpr std::uint32_t kMenuObjectManagerStart = 0x08A142C8u;   // (manager *, const std::string *name)


// self as `cls` into a frame holder, `use(object)`, then drop the holder.

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


// loadCarArray(...) / loadCourseMap(...): the shared loader, building an array of cars or a map of
// courses from the arguments; its result goes to *result. Module functions: (result, argc, argv).
void load_container(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t is_array,
                    std::uint32_t element) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    const std::uint32_t loaded = frame.at(0u);
    (void)call_guest(rt, ctx, kLoadContainer, {loaded, ctx.gpr[5], ctx.gpr[6], is_array, element});
    if (result != loaded) (void)call_guest(rt, ctx, kHolderReset, {result, rt.memory().load32(loaded)});
    destroy(rt, ctx, loaded);
}
void loadCarArray(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_container(rt, ctx, 1u, kCarArrayElement); }
void loadCourseMap(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { load_container(rt, ctx, 0u, kCourseMapElement); }

// MSceneFace.setClearColorBuffer(flag) -> 1 after setting the scene's clear-colour flag to
// toInt(argv[0]) != 0; 0 (and nothing set) without an argument. Method: (result, self, argc, argv).
void MSceneFace_setClearColorBuffer(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    GuestFrame frame(ctx, 0x30u);
    std::uint32_t done = 0u;
    if (argc > 0) {
        const std::uint32_t self = frame.at(4u);
        (void)call_guest(rt, ctx, kSelfAsSceneFace, {self});
        const std::uint32_t flag = rt.memory().load32(argv);
        const std::uint32_t truth = call_guest(rt, ctx, virtual_function(rt, flag, kToIntSlot), {flag});
        const std::uint32_t scene = rt.memory().load32(rt.memory().load32(self) + 0xE8u);
        (void)call_guest(rt, ctx, kSceneSetClearColor, {scene, truth != 0u ? 1u : 0u});
        destroy(rt, ctx, self);
        done = 1u;
    }
    return_integer(rt, ctx, result, frame.at(0u), done);
}


// pdistd.IsDebugVersion() -> 0: this is the release build.
void pdistd_IsDebugVersion(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    GuestFrame frame(ctx, 0x20u);
    return_integer(rt, ctx, result, frame.at(0u), 0u);
}

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


std::uint32_t organizer_core(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    return self_core(rt, ctx, kAsOrganizer, kOrganizerCore);
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

// MSound.fadeOutMusic([seconds = 1]).
void MSound_fadeOutMusic(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[5];
    const std::uint32_t argv = ctx.gpr[7];
    float seconds = 1.0f;
    if (ctx.gpr[6] == 1u) seconds = to_float(rt, ctx, rt.memory().load32(argv));
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t sound = frame.at(0u);
    (void)call_guest(rt, ctx, kSelfAsSound, {sound, self});
    (void)call_guest(rt, ctx, kSoundFadeOutMusic, {rt.memory().load32(sound)}, {seconds});
    destroy(rt, ctx, sound);
}

// MMusic.fadein(name[, seconds = 3]).
void MMusic_fadein(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t self = ctx.gpr[5];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    auto &memory = rt.memory();
    float seconds = 3.0f;
    if (argc >= 2) seconds = to_float(rt, ctx, memory.load32(argv + 4u));
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t name = frame.at(0x0u);
    const std::uint32_t music = frame.at(0x4u);
    to_string(rt, ctx, memory.load32(argv), name);
    (void)call_guest(rt, ctx, kSelfAsMusic, {music, self});
    (void)call_guest(rt, ctx, kMusicFadeIn, {memory.load32(music), memory.load32(name)}, {seconds});
    destroy(rt, ctx, music);
    string_release(rt, ctx, memory.load32(name));
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

// --- MCarParameter / MCarDriverParameter / MRaceParameter ---
std::int32_t trunc_w_s(float value) {  // Allegrex trunc.w.s: toward zero, NaN/out of range -> 0x7FFFFFFF
    if (!(value > -2147483648.0f && value < 2147483648.0f)) return 0x7FFFFFFF;
    return static_cast<std::int32_t>(value);
}
void MCarParameter_getPP1000(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t core = self_core(rt, ctx, kSelfAsCarParameter, kCarParameterCore);
    const float pp = call_guest(rt, ctx, kCarParameterPP, {core}, {}).f0;
    return_int(rt, ctx, result, static_cast<std::uint32_t>(trunc_w_s(pp * 1000.0f)));
}
void MCarParameter_isATOnly(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t info = call_guest(rt, ctx, kCarParameterTransmission, {self_core(rt, ctx, kSelfAsCarParameter, kCarParameterCore)});
    return_boolean(rt, ctx, result, call_guest(rt, ctx, kTransmissionIsATOnly, {info}));
}
void MCarParameter_getPowerScratch(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t core = self_core(rt, ctx, kSelfAsCarParameter, kCarParameterCore);
    return_int(rt, ctx, result, call_guest(rt, ctx, kCarParameterPowerScratch, {core}));
}
void car_parameter_set_tire(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx, std::uint32_t set) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    const std::uint32_t core = self_core(rt, ctx, kSelfAsCarParameter, kCarParameterCore);
    return_int(rt, ctx, result, call_guest(rt, ctx, set, {core, int_argument(rt, ctx, argv, 0u)}));
}
void MCarParameter_setRearTireCategory(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_parameter_set_tire(rt, ctx, kCarParameterSetRearTire); }
void MCarParameter_setFrontTireCategory(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) { car_parameter_set_tire(rt, ctx, kCarParameterSetFrontTire); }
void MCarDriverParameter_setPlayer(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    const std::uint32_t core = self_core(rt, ctx, kSelfAsCarDriverParameter, kCarDriverParameterCore);
    (void)call_guest(rt, ctx, kDriverParameterSetPlayer, {core, int_argument(rt, ctx, argv, 0u)});
}
std::uint32_t race_parameter_core(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    return self_core(rt, ctx, kSelfAsRaceParameter, kRaceParameterCore);
}
void MRaceParameter_getDelayStartSecond(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    const std::uint32_t core = race_parameter_core(rt, ctx);
    return_int(rt, ctx, result, call_guest(rt, ctx, kRaceParameterDelayStart, {core, int_argument(rt, ctx, argv, 0u)}));
}
void MRaceParameter_useLaunchData(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 0) return;
    const std::uint32_t core = race_parameter_core(rt, ctx);
    const auto use = static_cast<std::int32_t>(int_argument(rt, ctx, argv, 0u));
    (void)call_guest(rt, ctx, kRaceParameterUseLaunchData, {core, use > 0 ? 1u : 0u});
}
void MRaceParameter_setEventV(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    race_parameter_set(rt, ctx, kRaceParameterSetEventV, 0xFFFFFFFFu);
}
// MRaceParameter.setBoostParams(a0, a1, b0, b1, c0, c1): three boost settings for both sides.
void MRaceParameter_setBoostParams(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    if (argc <= 5) return;
    const std::uint32_t core = race_parameter_core(rt, ctx);
    std::uint32_t value[6];
    for (std::uint32_t i = 0u; i < 6u; ++i) value[i] = int_argument(rt, ctx, argv, i);
    const std::uint32_t setters[3] = {kRaceParameterSetBoostA, kRaceParameterSetBoostB, kRaceParameterSetBoostC};
    for (std::uint32_t i = 0u; i < 3u; ++i) {
        (void)call_guest(rt, ctx, setters[i], {core, 0u, value[i * 2u]});
        (void)call_guest(rt, ctx, setters[i], {core, 1u, value[i * 2u + 1u]});
    }
}


// --- system natives (module functions: result, argc, argv) ---
void MSystemCondition_IsExitGame(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    return_int(rt, ctx, result, call_guest(rt, ctx, kSystemIsExitGame, {}));
}
void MSystemConfiguration_SetTemporalAntialias(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const auto argc = static_cast<std::int32_t>(ctx.gpr[5]);
    const std::uint32_t argv = ctx.gpr[6];
    if (argc <= 0) return;
    rt.memory().store8(kTemporalAntialias, static_cast<std::uint8_t>(int_argument(rt, ctx, argv, 0u)));
}
// MSystemConfiguration.GetEnterButtonAssign() -> "cross" or "circle" (the system's enter button).
void MSystemConfiguration_GetEnterButtonAssign(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const bool circle = call_guest(rt, ctx, kEnterButtonQuery, {kEnterButtonParameter}) != 0u;
    return_c_string(rt, ctx, result, circle ? kTextCircle : kTextCross);
}
void MProductInformation_GetGameRegionCode(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    return_c_string(rt, ctx, result, call_guest(rt, ctx, kGameRegionCode, {}));
}

// MController.getConfig([index]) -> the controller configuration.
void MController_getConfig(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {
    const std::uint32_t result = ctx.gpr[4];
    const std::uint32_t self = ctx.gpr[5];
    const auto argc = static_cast<std::int32_t>(ctx.gpr[6]);
    const std::uint32_t argv = ctx.gpr[7];
    const std::uint32_t index = argc > 0 ? int_argument(rt, ctx, argv, 0u) : 0u;
    GuestFrame frame(ctx, 0x10u);
    const std::uint32_t holder = frame.at(0x0u);
    const std::uint32_t out = frame.at(0x4u);
    (void)call_guest(rt, ctx, kSelfAsController, {holder, self});
    const std::uint32_t owner = call_guest(rt, ctx, kControllerCore, {rt.memory().load32(holder), 0u});
    destroy(rt, ctx, holder);
    (void)call_guest(rt, ctx, kWrapControllerConfig, {out, owner, index});
    if (result != out) (void)call_guest(rt, ctx, kHolderReset, {result, rt.memory().load32(out)});
    destroy(rt, ctx, out);
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
GT_DECOMP(0x08805FD0u, gt::adhoc::game::CreateGameRecordStructure, gt::ReturnKind::Void);
GT_DECOMP(0x08A4C848u, gt::adhoc::game::pdistd_IsDebugVersion, gt::ReturnKind::Void);
// Not comparable: the loader reads files (sceIo), which a harness case can't replay.
// GT_DECOMP(0x088094DCu, gt::adhoc::game::loadCarArray, gt::ReturnKind::Void);
// GT_DECOMP(0x08809528u, gt::adhoc::game::loadCourseMap, gt::ReturnKind::Void);
GT_DECOMP(0x0880C2D4u, gt::adhoc::game::MSceneFace_setClearColorBuffer, gt::ReturnKind::Void);
GT_DECOMP(0x08A27F20u, gt::adhoc::game::MProductInformation_GetParentalLevel, gt::ReturnKind::Void);
GT_DECOMP(0x08A35648u, gt::adhoc::game::MSound_stopMusic, gt::ReturnKind::Void);
GT_DECOMP(0x08806384u, gt::adhoc::game::InitializeGameRecord, gt::ReturnKind::Void);
GT_DECOMP(0x08826968u, gt::adhoc::game::MRaceOperator_getTargetCarParameter, gt::ReturnKind::Void);
GT_DECOMP(0x088270E8u, gt::adhoc::game::MRaceOperator_getTargetDriverParameter, gt::ReturnKind::Void);
GT_DECOMP(0x08825FE4u, gt::adhoc::game::MRaceOperator_isTheater, gt::ReturnKind::Void);
GT_DECOMP(0x08824CB0u, gt::adhoc::game::MRaceOperator_clear, gt::ReturnKind::Void);
// Not comparable yet (waits on the UMD drive: sceUmdWaitDriveStat), so not swapped in: GT_DECOMP(0x08825AE8u, gt::adhoc::game::MRaceOperator_checkIn, gt::ReturnKind::Void);
// Not comparable yet (creates threads: sceKernelCreateThread), so not swapped in: GT_DECOMP(0x08825B74u, gt::adhoc::game::MRaceOperator_allocateResource, gt::ReturnKind::Void);
GT_DECOMP(0x0882BFA0u, gt::adhoc::game::MOrganizer_setDrivingLineDispFlag, gt::ReturnKind::Void);
GT_DECOMP(0x0882AFB4u, gt::adhoc::game::MOrganizer_setCameraTarget, gt::ReturnKind::Void);
GT_DECOMP(0x0882FEB0u, gt::adhoc::game::MOrganizer_getEntryControlType, gt::ReturnKind::Void);
GT_DECOMP(0x0882F404u, gt::adhoc::game::MOrganizer_hasLoadingCarDone, gt::ReturnKind::Void);
GT_DECOMP(0x0882E98Cu, gt::adhoc::game::MOrganizer_getCamearType, gt::ReturnKind::Void);
GT_DECOMP(0x0882BAD4u, gt::adhoc::game::MOrganizer_setRaceDisplayTarget, gt::ReturnKind::Void);
GT_DECOMP(0x0882B50Cu, gt::adhoc::game::MOrganizer_setNoInputLimitSecond, gt::ReturnKind::Void);
GT_DECOMP(0x0882B3C0u, gt::adhoc::game::MOrganizer_setApplicationEndSessionSecond, gt::ReturnKind::Void);
GT_DECOMP(0x08832BB4u, gt::adhoc::game::MOrganizer_getGameCondition, gt::ReturnKind::Void);
GT_DECOMP(0x08833C24u, gt::adhoc::game::MOrganizer_getRaceSound, gt::ReturnKind::Void);
GT_DECOMP(0x08829A78u, gt::adhoc::game::MOrganizer_setBackRaceParameter, gt::ReturnKind::Void);
GT_DECOMP(0x08857A78u, gt::adhoc::game::MCarParameter_getPP1000, gt::ReturnKind::Void);
GT_DECOMP(0x0885794Cu, gt::adhoc::game::MCarParameter_isATOnly, gt::ReturnKind::Void);
GT_DECOMP(0x08857584u, gt::adhoc::game::MCarParameter_getPowerScratch, gt::ReturnKind::Void);
GT_DECOMP(0x08855E54u, gt::adhoc::game::MCarParameter_setRearTireCategory, gt::ReturnKind::Void);
GT_DECOMP(0x08855D78u, gt::adhoc::game::MCarParameter_setFrontTireCategory, gt::ReturnKind::Void);
GT_DECOMP(0x0884D950u, gt::adhoc::game::MCarDriverParameter_setPlayer, gt::ReturnKind::Void);
GT_DECOMP(0x0884BDA8u, gt::adhoc::game::MRaceParameter_getDelayStartSecond, gt::ReturnKind::Void);
GT_DECOMP(0x08847144u, gt::adhoc::game::MRaceParameter_useLaunchData, gt::ReturnKind::Void);
GT_DECOMP(0x088477A0u, gt::adhoc::game::MRaceParameter_setEventV, gt::ReturnKind::Void);
GT_DECOMP(0x0884739Cu, gt::adhoc::game::MRaceParameter_setBoostParams, gt::ReturnKind::Void);
GT_DECOMP(0x08A29540u, gt::adhoc::game::MSystemCondition_IsExitGame, gt::ReturnKind::Void);
GT_DECOMP(0x08A29730u, gt::adhoc::game::MSystemConfiguration_SetTemporalAntialias, gt::ReturnKind::Void);
GT_DECOMP(0x08A29FD8u, gt::adhoc::game::MSystemConfiguration_GetEnterButtonAssign, gt::ReturnKind::Void);
GT_DECOMP(0x08A28CE4u, gt::adhoc::game::MProductInformation_GetGameRegionCode, gt::ReturnKind::Void);
GT_DECOMP(0x088387DCu, gt::adhoc::game::MController_getConfig, gt::ReturnKind::Void);
GT_DECOMP(0x08A35B8Cu, gt::adhoc::game::MSound_fadeOutMusic, gt::ReturnKind::Void);
// Not comparable yet (wakes a thread: sceKernelWakeupThread), so not swapped in: GT_DECOMP(0x08A3B9E8u, gt::adhoc::game::MMusic_fadein, gt::ReturnKind::Void);
