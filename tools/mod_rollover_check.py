"""Verdict of tools/mod_rollover_check.sh: the rollover mod (gt-mods repo) in a headless gt_play run.

The mod logs the player car's state (GT_MOD_ROLLOVER_LOG=1): `[mod:rollover] st=<player-car step> imp=<impulses so far> since=<steps since
the last> pos=x,y,z vel=x,y,z fwd=x,y,z lat=x,y,z up=x,y,z tilt=<+0x6FC>,<+0x700> rate=<+0x6F4> flag=<+0x74B hex>/<+0x74C> held=<steps
left>(<held roll rate>) body_calls=<n>`, plus one `IMPULSE st=<step> held=<signed rate>` line per hit it answered (positive rate
= toward the car's lateral axis `lat`) and `FLAGREAD ra=<addr> lifted=<0|1>` once per caller of the flag reader and value it got (`ra` is the guest return address in --decomp off only). The route (config/pad_routes/rollover.pad)
drives the car into the barrier at the end of the straight at a slant.
Judged here:
  roll run      normal driving first (>= MIN_QUIET_LINES lines, flag byte 0, upright, tilt under the game's own 35 deg clamp, no impulse before
                MIN_CRASH_STEP); an impulse at the crash; the car then tilts past the clamp (up.z under MAX_ROLLED_UP_Z or a tilt angle over
                MIN_ROLLED_TILT), toward the side it hit; the dormant flip state machine never wakes (flag byte stays 0, the car never jumps in
                height: it would sink through the road and be pushed up); no nan; the car ends upright or upside down at rest;
                the flag is lifted only for the clamp's and the wrap's readers (judge_flag_scope);
  box judge     (body_box, 0.2.0; every roll run) the tumbling car's body stays above the ground: the deepest of the box's 8 corners
                (`BOX` lines, the log measures even with the box off) never below MAX_BODY_DEPTH while tilted, the origin never rises more than
                MAX_TUMBLE_RISE over the ground (the box rolls over its edge at 1.17 m; it must not hop beyond that), the box never touched the car while upright (BOXSTAT);
  box compare   the box run and a run with body_box = false: every state line before the first impulse is byte-identical (the box is
                invisible until the car touches the ground with its body), and the box-off run does sink (the premise);
  identity run  the mod installed but disabled: frames byte-identical to the run without it; the toggle run (switched off from the F9 menu
                before the crash): the mod ran before the toggle and logged nothing after, and EVERY frame, the mod's active part and the
                crash after the toggle included, is byte-identical to the run without the mod (no hit before the crash: the mod changes nothing);
  recover run   (rollover + reset-to-track) the car rolled before the reset's RESTORE line; the reset judge (tools/mod_reset_check.py) says the rest.
"""
import math
import os
import re
import sys

TAG = r"\[mod:rollover\] "
NUM = r"-?(?:[\d.]+|nan|inf)"
VEC = rf"({NUM}),({NUM}),({NUM})"
STATE = re.compile(TAG + rf"st=(\d+) imp=(\d+) since=(-?\d+) pos={VEC} vel={VEC} fwd={VEC} lat={VEC} up={VEC} tilt=({NUM}),({NUM}) "
                   rf"rate=({NUM}) flag=([0-9A-Fa-f]{{2}})/(-?\d+) held=(\d+)\(({NUM})\)(?: (?:body|flipmachine)_calls=(\d+))?")
IMPULSE = re.compile(TAG + rf"IMPULSE st=(\d+) held=({NUM})(?: lat={VEC})?")
FLAGREAD = re.compile(TAG + r"FLAGREAD ra=([0-9A-Fa-f]{8}) lifted=([01])")
LIFTED_OK = {0x0893E5AC, 0x08956BF0}  # the tilt clamp's and the angle wrap's call sites (+ 8): the only readers that may get 1 (--decomp off)
BOX = re.compile(TAG + rf"BOX st=(\d+) box=([01]) upn=({NUM}) z-g=({NUM}) clear=({NUM}) roof=({NUM}) contacts=(\d+) depth=({NUM}) push=({NUM}) dv=({NUM}) vz=({NUM})")
BOXSTAT = re.compile(TAG + rf"BOXSTAT st=(\d+) steps=(\d+) touch=(\d+) touch_upright=(\d+) tilted=(\d+) min_clear=({NUM}) min_roof_tilted=({NUM})")
RESTORE = re.compile(r"\[mod:reset-to-track\] RESTORE step=(\d+)")
SCRIPT_DONE = re.compile(r"script done at vblank (\d+)")
TOGGLE_LINE = re.compile(r"^\[mods\] \S+ -> OFF$", re.M)
FRAME = re.compile(r"frame_(\d+)\.ppm$")

CLAMP_TILT = 0.611        # rad: the game's own tilt clamp (35 deg), which the mod lifts
MAX_QUIET_TILT = 0.35     # rad: normal driving stays well inside it
MIN_QUIET_LINES = 40      # state lines before the first impulse (one per 30 steps: the hit is at step ~1860 = ~60 lines; margin for a different timing)
MIN_CRASH_STEP = 1500     # no impulse before this player-car step: the race starts at ~750 and the route hits the barrier at ~1850
MAX_INVERTED_UP_Z = 0.0   # upside down: past 90 degrees (the recover run resets the car while it is tumbling on its roof)
MAX_ROLLED_UP_Z = 0.6     # past the clamp for sure: the clamp alone leaves up.z above ~0.67 even with both angles at the limit
MIN_ROLLED_TILT = CLAMP_TILT + 0.15
ROLL_WINDOW = 150         # steps after the impulse in which the roll must happen
DIRECTION_STEPS = 15      # the side it tips toward is read in the first steps after the hit
MAX_Z_STEP = 0.5          # m of height change per physics step (~35 m/s * a 5 % slope is 0.03, a free fall at 9 m/s 0.15, the push back up 2 or more)
SETTLE_STEPS = 400        # the car's final state is read this long after the impulse
MIN_UPRIGHT_Z = 0.9
MAX_REST_SPEED = 3.0      # m/s: "upside down at rest"
FLIP_FLAG_BITS = 0x06     # +0x74B bits the flip state machine sets
MIN_PRE_TOGGLE_LINES = 10
MAX_BODY_DEPTH = 0.25     # m: the deepest box corner of the tumbling car below the ground (the box off: 1.3 m; the first tilted step is already ~0.2 m low)
MAX_TUMBLE_RISE = 1.6     # m: highest the origin gets above the ground plane while tumbling. Not a "hop" measure: a box resting on a roof edge or corner puts the origin
                          # at the half-diagonal of its cross-section (sqrt(0.84^2 + 0.8^2) = 1.16 m) or higher when it stands on a nose corner (measured 1.43 m with the roof
                          # 0.03 m inside the ground), and the plane is the one fitted while upright (the terrain falls away); the stock 0.64 m only happens because the roof sinks
                          # into the road. What it still catches: a box that is launched (a rigid box without the cap reached 1.5-2.0 m with its roof 0.07 m inside)
MIN_PREMISE_DEPTH = 0.5   # m: the box-off run must sink at least this deep, or the depth assertion proves nothing
SETTLE_AFTER_RESET = 20   # steps after a reset out of a tumble in which the springs settle (0.9 rad/s of wobble at 10, 0.1 at 17)
MAX_SETTLED_RATE = 0.3
MAX_REPLAY_DRIFT = 0.5    # m: largest distance between the live car and its replay at the same player-car step (measured 0.06 m on circuit_analog, a car at rest)
ROLLED_UP_Z = 0.5         # up.z below this: the car is past ~60 deg of tilt


def parse_states(text):
    out = []
    for m in STATE.finditer(text):
        g = m.groups()
        f = [float(x) for x in g[3:20]] + [float(g[20])]
        out.append({"st": int(g[0]), "imp": int(g[1]), "since": int(g[2]), "pos": tuple(f[0:3]), "vel": tuple(f[3:6]), "fwd": tuple(f[6:9]),
                    "lat": tuple(f[9:12]), "up": tuple(f[12:15]), "tilt": (f[15], f[16]), "rate": f[17],
                    "flag": int(g[21], 16), "count": int(g[22]), "held": int(g[23]), "flipcalls": int(g[25]) if g[25] is not None else None})
    return out


def parse_impulses(text):
    return [{"st": int(m.group(1)), "held": float(m.group(2)), "lat": tuple(float(x) for x in m.groups()[2:5]) if m.group(3) is not None else None}
            for m in IMPULSE.finditer(text)]


def parse_flagreads(text):
    """[(ra, lifted)] for the `FLAGREAD` lines with a real guest address (--decomp on logs site numbers, not addresses)."""
    return [(int(m.group(1), 16), m.group(2) == "1") for m in FLAGREAD.finditer(text) if int(m.group(1), 16) >= 0x08000000]


def finite(s):
    return all(math.isfinite(v) for v in (*s["pos"], *s["vel"], *s["up"], *s["tilt"], s["rate"]))


def speed(s):
    return math.sqrt(sum(v * v for v in s["vel"]))


def judge_flag_scope(text):
    """The flag is lifted only for the readers that need it: no real-address reader but the clamp's and the wrap's ever got 1, and (when the log
    names readers at all, i.e. --decomp off) the clamp's did. Returns (ok, message)."""
    reads = parse_flagreads(text)
    wrong = sorted({ra for ra, lifted in reads if lifted and ra not in LIFTED_OK})
    if wrong:
        return False, "the flag was lifted for a reader that does not need it: " + " ".join(f"{ra:08X}" for ra in wrong)
    if reads and not any(lifted and ra == 0x0893E5AC for ra, lifted in reads):
        return False, "the tilt clamp's reader (ra 0893E5AC) never got the flag: the clamp is not lifted"
    return True, f"flag lifted only for the clamp/wrap readers ({len(reads)} reader logs)"


def judge_roll(text, expected_flipcalls=True):
    """The roll run's log. Returns (ok, message)."""
    states, impulses = parse_states(text), parse_impulses(text)
    if not states:
        return False, "no `[mod:rollover]` state lines: the mod did not run"
    bad = [s for s in states if not finite(s)]
    if bad:
        return False, f"nan/inf in the state log at step {bad[0]['st']}"
    if not impulses:
        return False, "no IMPULSE line: the mod never answered a hit"
    first = impulses[0]
    if first["st"] < MIN_CRASH_STEP:
        return False, f"an impulse at step {first['st']}, before the crash (normal driving must not get one; first allowed step {MIN_CRASH_STEP})"
    before = [s for s in states if s["st"] < first["st"]]
    if len(before) < MIN_QUIET_LINES:
        return False, f"only {len(before)} state lines before the impulse (need {MIN_QUIET_LINES}): the mod did not run in normal driving"
    for s in before:
        if s["flag"] != 0 or s["up"][2] < MIN_UPRIGHT_Z or max(abs(s["tilt"][0]), abs(s["tilt"][1])) > MAX_QUIET_TILT:
            return False, f"step {s['st']}: not a normal-driving state before the crash (flag {s['flag']:02X}, up.z {s['up'][2]:.3f}, tilt {s['tilt']})"
    window = [s for s in states if first["st"] <= s["st"] <= first["st"] + ROLL_WINDOW]
    if not window:
        return False, "no state lines after the impulse"
    low = min(window, key=lambda s: s["up"][2])
    top_tilt = max(max(abs(s["tilt"][0]), abs(s["tilt"][1])) for s in window)
    if not (low["up"][2] < MAX_ROLLED_UP_Z or top_tilt > MIN_ROLLED_TILT):
        return False, (f"the car did not tilt past the game's 35 deg clamp after the impulse (lowest up.z {low['up'][2]:.3f}, "
                       f"largest tilt {top_tilt:.3f} rad)")
    # the direction is read right after the hit (a car that keeps rolling swings through both sides later), against the lateral axis the car had
    # BEFORE the hit (the body's own axis turns with it: up . lat is always 0)
    lat0 = first["lat"] if first["lat"] is not None else before[-1]["lat"]
    early = [s for s in window if s["st"] <= first["st"] + DIRECTION_STEPS] or window[:1]
    tip = max((sum(u * l for u, l in zip(s["up"], lat0)) for s in early), key=abs)
    if tip * first["held"] <= 0:
        return False, f"the car tipped toward {'+' if tip > 0 else '-'}lat but the impulse was {first['held']:+.1f}: not toward the side that was hit"
    ok, message = judge_flag_scope(text)
    if not ok:
        return False, message
    if any(s["flag"] & FLIP_FLAG_BITS for s in states):
        return False, "the flip state machine woke (+0x74B bits set): the car would sink through the road and be pushed back up"
    for a, b in zip(states, states[1:]):
        dst = b["st"] - a["st"]
        if 0 < dst <= 30 and abs(b["pos"][2] - a["pos"][2]) / dst > MAX_Z_STEP:
            return False, f"the car jumped {b['pos'][2] - a['pos'][2]:+.2f} m in height between steps {a['st']} and {b['st']}"
    if expected_flipcalls and not any((s["flipcalls"] or 0) > 0 for s in states):
        return False, "the hook on the body step (0x08956DC8) never fired: the dispatch check is blind"
    final = [s for s in states if s["st"] >= first["st"] + SETTLE_STEPS]
    if not final:
        return False, f"the log ends before {SETTLE_STEPS} steps after the impulse"
    end = final[-1]
    if not (end["up"][2] > MIN_UPRIGHT_Z or (end["up"][2] < -0.5 and speed(end) < MAX_REST_SPEED)):
        return False, f"the car ends neither upright nor upside down at rest (up.z {end['up'][2]:.3f}, {speed(end):.1f} m/s)"
    return True, (f"first hit at step {first['st']} (rate {first['held']:+.1f}), {len(before)} quiet lines before it; lowest up.z {low['up'][2]:.3f}, "
                  f"tilt {top_tilt:.2f} rad toward the side hit; flip machine dormant, no height jump; ends up.z {end['up'][2]:.2f} at {speed(end):.1f} m/s")


def parse_box(text):
    return [{"st": int(m.group(1)), "box": m.group(2) == "1", "upn": float(m.group(3)), "zg": float(m.group(4)), "clear": float(m.group(5)),
             "roof": float(m.group(6)), "contacts": int(m.group(7)), "push": float(m.group(9))} for m in BOX.finditer(text)]


def judge_box(text):
    """The roll run's BOX lines. Returns (ok, message)."""
    impulses, lines = parse_impulses(text), parse_box(text)
    # before the first impact (normal driving; with no impulse at all: the whole run) the box must never push: the car is upright and the wheels own the contact
    first_hit = impulses[0]["st"] if impulses else 10 ** 9
    upright = [b for b in lines if b["st"] < first_hit and b["upn"] > 0.9 and b["push"] > 0]
    if upright:
        return False, f"the box pushed the car {len(upright)} times before the first impact (first at step {upright[0]['st']}, up.n {upright[0]['upn']:.2f}): it must stay invisible while driving"
    if not impulses:
        return False, "no IMPULSE line: no tumble to judge"
    tumble = [b for b in lines if b["st"] >= impulses[0]["st"] and b["upn"] < 0.77]
    if not tumble:
        return False, "no BOX line of a tilted car after the impulse: the mod did not measure the body (is GT_MOD_ROLLOVER_LOG=1 set?)"
    if not all(math.isfinite(v) for b in tumble for v in (b["zg"], b["clear"], b["roof"])):
        return False, "nan/inf in the BOX lines"
    rise = max(b["zg"] for b in tumble)
    if rise > MAX_TUMBLE_RISE:
        return False, f"the origin rose {rise:.2f} m over the ground in the tumble (max {MAX_TUMBLE_RISE} m): the box launched the car"
    deepest = min(b["clear"] for b in tumble)
    if deepest < -MAX_BODY_DEPTH:
        return False, f"the car's body sank {-deepest:.2f} m into the ground while tumbling (max {MAX_BODY_DEPTH} m; roof {min(b['roof'] for b in tumble):.2f})"
    if not any(b["box"] and b["push"] > 0 for b in tumble):
        return False, "the box never acted (no BOX line with box=1 and a push): body_box is off or the tumble never touched the ground"
    return True, (f"tumble: deepest body corner {-deepest:.2f} m below the ground (max {MAX_BODY_DEPTH}), origin rose {rise:.2f} m (max {MAX_TUMBLE_RISE}), "
                  f"{sum(1 for b in tumble if b['push'] > 0)} steps pushed, 0 upright touches")


def judge_box_compare(box_text, off_text):
    """The box run against a run with body_box = false: the state lines before the first impulse of the box-off run (the reference: the stock tumble) are
    byte-identical in the box run, and the box-off tumble sinks. Returns (ok, message)."""
    imp_off = parse_impulses(off_text)
    if not imp_off:
        return False, "no IMPULSE line in the box-off run"
    cut = imp_off[0]["st"]
    def before(text):
        return [m.group(0) for m in STATE.finditer(text) if int(m.group(1)) < cut]
    a, b = before(box_text), before(off_text)
    if len(b) < MIN_QUIET_LINES:
        return False, f"only {len(b)} state lines before the impulse (need {MIN_QUIET_LINES})"
    if a != b:
        first = next((i for i, (x, y) in enumerate(zip(a, b)) if x != y), min(len(a), len(b)))
        return False, f"the box changed the car before the first impact: state line {first + 1} of {len(b)} differs (or the counts differ: {len(a)} vs {len(b)})"
    tumble = [x for x in parse_box(off_text) if x["st"] >= cut and x["upn"] < 0.77]
    sink = -min((x["clear"] for x in tumble), default=0.0)
    if sink < MIN_PREMISE_DEPTH:
        return False, f"the box-off run sank only {sink:.2f} m (need {MIN_PREMISE_DEPTH}): the comparison proves nothing"
    return True, f"{len(b)} state lines before the impact byte-identical with and without the box; without it the body sinks {sink:.2f} m"


def judge_recover(text, reset_text):
    """The rollover + reset-to-track run: the car rolled (impulse, tilt past the clamp) before the reset's RESTORE line."""
    states, impulses = parse_states(text), parse_impulses(text)
    restores = [int(m.group(1)) for m in RESTORE.finditer(reset_text)]
    if len(restores) != 1:
        return False, f"{len(restores)} RESTORE lines (expected 1)"
    if not impulses or impulses[0]["st"] >= restores[0]:
        return False, "no impulse before the reset: the car was not rolling when it was reset"
    between = [s for s in states if impulses[0]["st"] <= s["st"] <= restores[0]]
    low = min((s["up"][2] for s in between), default=1.0)
    if not low < MAX_INVERTED_UP_Z:
        return False, f"the car was not upside down when it was reset (lowest up.z {low:.3f} between the hit and the reset, need under {MAX_INVERTED_UP_Z})"
    after = [s for s in states if restores[0] + 30 <= s["st"] <= restores[0] + 600]
    if not after or any(s["up"][2] < MIN_UPRIGHT_Z for s in after):
        return False, "the car is not upright after the reset"
    wobble = [s for s in states if restores[0] + SETTLE_AFTER_RESET <= s["st"] <= restores[0] + 120]
    if not wobble or any(abs(s["rate"]) > MAX_SETTLED_RATE for s in wobble):
        return False, f"the roll rate is not settled (< {MAX_SETTLED_RATE} rad/s) {SETTLE_AFTER_RESET} steps after the reset"
    return True, f"upside down at the reset (lowest up.z {low:.2f} before step {restores[0]}), upright from 30 steps after it"


def judge_quiet(text, toggled=False):
    """A run where the mod must not act: no impulse. A toggle run must show the mod ran before `-> OFF` and logged nothing after."""
    if parse_impulses(text):
        return False, "an IMPULSE line in a run where the mod must not act"
    cut = TOGGLE_LINE.search(text)
    if toggled:
        if cut is None:
            return False, "no `[mods] <id> -> OFF` line in the run log: the menu did not toggle the mod"
        before = sum(1 for m in STATE.finditer(text) if m.start() < cut.start())
        after = sum(1 for m in STATE.finditer(text) if m.start() > cut.start())
        if before < MIN_PRE_TOGGLE_LINES:
            return False, f"only {before} state lines before the toggle (need {MIN_PRE_TOGGLE_LINES}): the mod did not run"
        if after:
            return False, f"{after} mod state lines after the toggle: the mod ran while switched off"
        return True, f"no impulse; the mod ran ({before} lines) and was silent after the toggle"
    if STATE.search(text):
        return False, "the mod logged state in a run where it should be disabled"
    return True, "disabled: the mod logged nothing"


def read_ppm(path):
    with open(path, "rb") as handle:
        data = handle.read()
    match = re.match(rb"P6\s+(\d+)\s+(\d+)\s+(\d+)\s", data)
    if match is None:
        raise ValueError(f"{path}: not a P6 PPM")
    width, height = int(match.group(1)), int(match.group(2))
    pixels = data[match.end():]
    if len(pixels) != width * height * 3:
        raise ValueError(f"{path}: size mismatch")
    return width, height, pixels


def frames_of(directory):
    out = {}
    for name in os.listdir(directory):
        match = FRAME.search(name)
        if match:
            out[int(match.group(1))] = os.path.join(directory, name)
    return out


def judge_identity(base_frames, mod_frames, min_frames=6):
    """Every common frame byte-identical. Returns (ok, message)."""
    common = sorted(set(base_frames) & set(mod_frames))
    if len(common) < min_frames:
        return False, f"only {len(common)} common frames"
    for index in common:
        if read_ppm(base_frames[index])[2] != read_ppm(mod_frames[index])[2]:
            return False, f"frame {index} differs from the run without the mod"
    return True, f"{len(common)} frames byte-identical to the run without the mod"


def judge_toggle_frames(log, base_frames, mod_frames, toggle_at=None, min_frames=20):
    """With the flag lifted only where the roll needs it, a mod that makes no hit changes nothing: EVERY frame of the toggle run, from the first
    (the mod is on until the toggle) to the route's end (the crash after it included: it must not roll), equals the run without the mod.
    Returns (ok, message). `toggle_at` is kept for the command line."""
    if SCRIPT_DONE.search(log) is None:
        return False, "no `script done` line"
    ok, message = judge_identity(base_frames, mod_frames, min_frames)
    return ok, message.replace("frames", "frames (the whole run, the mod on until the toggle)", 1)


def judge_replay(text):
    """The replay re-simulates the race from the recorded pad stream: the mod must act there as it did live. Before `REPLAY start` is the live
    race, after it the replay. A replay counts its steps from another point of the start countdown (the stock game's replay trails the live
    car by up to 3 m at the same step number, proportional to the speed: a time shift, not other physics), so the two are paired by the steps
    since the hit (`since`, a line every 3 steps for 150 steps after it): same hits (count and size), a roll in both, and the replayed car
    within MAX_REPLAY_DRIFT of the live one at every paired step."""
    marker = text.find("REPLAY start")
    if marker < 0:
        return False, "no REPLAY start line: the mod never saw the player's car in its replay"
    live_text, rep_text = text[:marker], text[marker:]
    li, ri = parse_impulses(live_text), parse_impulses(rep_text)
    if not li:
        return False, "no live impulse"
    if [round(i["held"], 2) for i in li] != [round(i["held"], 2) for i in ri]:
        return False, f"impulses differ: live {[i['held'] for i in li]}, replay {[i['held'] for i in ri]}"
    def window(states):  # the first hit's dense window, by steps since it
        return {s["since"]: s for s in states if s["imp"] == 1 and 0 <= s["since"] < 150}
    live, rep = window(parse_states(live_text)), window(parse_states(rep_text))
    common = sorted(set(live) & set(rep))
    if len(common) < 20:
        return False, f"only {len(common)} steps after the hit logged in both"
    live_min, rep_min = min(live[k]["up"][2] for k in common), min(rep[k]["up"][2] for k in common)
    if live_min >= ROLLED_UP_Z or rep_min >= ROLLED_UP_Z:
        return False, f"the car must roll in both: lowest up.z live {live_min:.2f}, replay {rep_min:.2f} (need < {ROLLED_UP_Z})"
    drift, at = max((math.dist(rep[k]["pos"], live[k]["pos"]), k) for k in common)
    if drift > MAX_REPLAY_DRIFT:
        return False, f"the replay drifts {drift:.2f} m from the live car {at} steps after the hit (limit {MAX_REPLAY_DRIFT})"
    shift = ri[0]["st"] - li[0]["st"]
    return True, (f"{len(li)} hit(s) of the same size in both (replay step count shifted {shift:+d}), rolled (up.z live {live_min:.2f}, replay "
                  f"{rep_min:.2f}), {len(common)} steps after the hit compared, drift {drift:.2f} m at +{at}")


def read(path):
    with open(path, errors="replace") as handle:
        return handle.read()


def main(argv):
    out, mode = argv[1], argv[2]
    if mode == "roll":
        text = read(os.path.join(out, "roll", "run.log"))
        ok, message = judge_roll(text)
        print(("PASS " if ok else "FAIL ") + f"roll: {message}")
        okb, messageb = judge_box(text)
        print(("PASS " if okb else "FAIL ") + f"box: {messageb}")
        return 0 if ok and okb else 1
    if mode == "boxcmp":
        ok, message = judge_box_compare(read(os.path.join(out, "roll", "run.log")), read(os.path.join(out, "nobox", "run.log")))
        print(("PASS " if ok else "FAIL ") + f"boxcmp: {message}")
        return 0 if ok else 1
    if mode == "replay":
        ok, message = judge_replay(read(os.path.join(out, "replay", "run.log")))
        print(("PASS " if ok else "FAIL ") + f"replay: {message}")
        return 0 if ok else 1
    if mode == "recover":
        ok, message = judge_recover(read(os.path.join(out, "recover", "run.log")), read(os.path.join(out, "recover", "run.log")))
        print(("PASS " if ok else "FAIL ") + f"recover: {message}")
        return 0 if ok else 1
    label = {"disabled": "disabled", "toggle": "toggle"}[mode]
    log = read(os.path.join(out, label, "run.log"))
    ok1, m1 = judge_quiet(log, toggled=(mode == "toggle"))
    base, mod = frames_of(os.path.join(out, "baseline", "frames")), frames_of(os.path.join(out, label, "frames"))
    if mode == "toggle":
        ok2, m2 = judge_toggle_frames(log, base, mod, int(argv[3]))
    else:
        ok2, m2 = judge_identity(base, mod)
    ok = ok1 and ok2
    print(("PASS " if ok else "FAIL ") + f"{label}: {m1}; {m2}")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
