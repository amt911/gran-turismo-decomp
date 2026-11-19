"""Verdict of tools/mod_rollover_check.sh --sim / --drop / --sim-replay: the rollover mod's rigid-body style ("realistic", gt-mods 0.5.0) in a headless run.

The mod logs (GT_MOD_ROLLOVER_LOG=1), one tag `[mod:rollover]`:
  ST st=<step> pos= vel= speed= up.z= since=<steps since the car first moved>      every 30 steps while the game drives the car
  TAKEOVER st= reason=<1 hit, 2 airborne, 3 flipped> v= w= up.z=                  the simulation takes the car
  HIT st= dv= at=                                                                 an impulse at a contact point (the barrier, a fake hit)
  SIM st= n= pos= vel= w= up.z= lowest= contacts=<touching>/<on the floor face> ground= E=<J> com.z= kd=<dpos>,<dup>   one line per simulated step
  HANDBACK st= after=<sim steps> / RESET st= after=                               the game gets the car back
  REPLAY start                                                                    the player's car in its replay
`kd` is how far the pose the game reports at the start of a step is from the pose the mod wrote at the end of the last one: the game's own step moves a written
pose by its velocity times the step (and by its suspension), so kd is about |v|/60 (and ~0 for the up vector) when the pose reaches the game, and grows without bound when it does not.

Judged:
  barrier   no takeover before MIN_CRASH_STEP (quiet driving untouched), one at the crash because of a hit; then the simulation obeys the energy budget (total
            energy m g z + 1/2 m v^2 + 1/2 w.I.w never above the energy right after the hit impulse by more than ENERGY_TOLERANCE: no hopping, no "peonza"), no hull point
            more than MAX_BELOW_GROUND under the ground, |w_z| under MAX_YAW_RATE while up.z < 0.3, and the car ends at rest or handed back;
  drop      a fake drop of ~2 m: the first rebound's apex is at most BOUNCE_FACTOR * e^2 * the drop height over the rest height, then the car lies still and is handed back,
            the throttle then accelerates it again (driving resumes);
  pose      (every run) kd within a step of motion while simulating at speed: what the game draws is the simulation's pose;
  replay    the same takeover happens live and in the replay, and the simulated poses agree within MAX_REPLAY_DRIFT at the same number of steps after the takeover.
"""
import math
import os
import re
import sys

TAG = r"\[mod:rollover\] "
NUM = r"-?(?:\d+\.?\d*(?:e[+-]?\d+)?|nan|inf)"
VEC = rf"({NUM}),({NUM}),({NUM})"
SIM = re.compile(TAG + rf"SIM st=(\d+) n=(\d+) pos={VEC} vel={VEC} w={VEC} up\.z=({NUM}) lowest=({NUM}) contacts=(\d+)/(\d+) ground=({NUM}) E=({NUM}) com\.z=({NUM}) kd=({NUM}),({NUM})")
ST = re.compile(TAG + rf"ST st=(\d+) pos={VEC} vel={VEC} speed=({NUM}) up\.z=({NUM}) since=(-?\d+)")
TAKEOVER = re.compile(TAG + rf"TAKEOVER st=(\d+) reason=(\d) v={VEC} w={VEC} up\.z=({NUM})")
HIT = re.compile(TAG + rf"HIT st=(\d+) dv={VEC}")
END = re.compile(TAG + r"(HANDBACK|RESET) st=(\d+) after=(\d+)")

MIN_CRASH_STEP = 1500       # the race starts at player-car step ~750 and the route hits the barrier at ~1850 (docs/FACTS.md "Rollovers")
MIN_QUIET_LINES = 20
MAX_BELOW_GROUND = 0.05     # m: no hull point deeper (spec)
ENERGY_TOLERANCE = 0.03     # fraction of the energy above the ground right after the hit; plus ENERGY_SLACK joules
ENERGY_SLACK = 1500.0
MAX_YAW_RATE = 15.0         # rad/s: the world-vertical angular velocity while the car is on its side or roof (a spinning top at 40+)
LOW_UP_Z = 0.3
MAX_REST_SPEED = 0.6
MAX_KD = 0.8                # m beyond one step of motion (POSE_STEP_MARGIN x |v|/60): the game's own collision pushes a car out of the barrier by ~0.3 m per step (measured 2026-11-18 at rest against it); a pose never sent drifts by metres
MAX_KUP = 0.06              # up-vector units (~3.5 degrees)
POSE_STEP_MARGIN = 1.5
KD_MIN_SPEED = 2.0          # m/s: slower than this the game's own step barely moves the pose and kd proves nothing
BOUNCE_FACTOR = 1.2
MIN_BOUNCE_FACTOR = 0.5     # the first rebound must exist: without a bounce at all the premise (a drop) is not shown
MIN_RESUME_GAIN = 3.0       # m/s the car must gain in ROLL_FORWARD steps after the handback (throttle held by the route)
RESUME_STEPS = 400
MAX_REPLAY_DRIFT = 0.5
G = 9.81
SLIDE_MIN_SPEED = 1.0       # m/s on the floor that count as sliding
SLIDE_TOLERANCE = 0.25      # of v0^2 / (2 mu g)
MAX_SIM_HITS = 12           # impulses taken from the game's own collision while simulating (a barrier scrape is a few; landings and rests with grounded wheel records must add none)
MIN_TIP_UP_Z = 0.5          # the barrier hit must tip the car at least this far (up.z under this)
REBOUND_STEPS = 10         # steps after the first contact in which the rebound speed is read
RESTITUTION = 0.15          # the shipped default (mod.toml)


def f(x):
    return float(x)


def parse_sim(text):
    out = []
    for m in SIM.finditer(text):
        g = m.groups()
        out.append({"st": int(g[0]), "n": int(g[1]), "pos": (f(g[2]), f(g[3]), f(g[4])), "vel": (f(g[5]), f(g[6]), f(g[7])), "w": (f(g[8]), f(g[9]), f(g[10])),
                    "upz": f(g[11]), "lowest": f(g[12]), "contacts": int(g[13]), "bottom": int(g[14]), "ground": f(g[15]), "E": f(g[16]), "comz": f(g[17]),
                    "kd": f(g[18]), "kup": f(g[19])})
    return out


def parse_st(text):
    out = []
    for m in ST.finditer(text):
        g = m.groups()
        out.append({"st": int(g[0]), "pos": (f(g[1]), f(g[2]), f(g[3])), "vel": (f(g[4]), f(g[5]), f(g[6])), "speed": f(g[7]), "upz": f(g[8]), "since": int(g[9])})
    return out


def parse_takeovers(text):
    return [{"st": int(m.group(1)), "reason": int(m.group(2)), "up": f(m.group(9))} for m in TAKEOVER.finditer(text)]


def parse_ends(text):
    return [{"kind": m.group(1), "st": int(m.group(2)), "after": int(m.group(3))} for m in END.finditer(text)]


def finite(s):
    return all(math.isfinite(x) for x in (*s["pos"], *s["vel"], *s["w"], s["upz"], s["lowest"], s["E"]))


def judge_pose(sims):
    """kd: the pose the game reports at the start of a step is the one the mod wrote at the end of the last (the game's step integrates the velocity once more and its suspension
    acts a little: at most one step of motion). The draw-point write the spec proposed turned out to change nothing on screen (docs/FACTS.md "Rollover physics research"), so
    what is drawn is this pose and the check is that it is the simulation's."""
    judged = [s for s in sims if s["n"] > 1 and math.hypot(*s["vel"]) > KD_MIN_SPEED]
    if len(judged) < 10:
        return False, f"only {len(judged)} simulated steps at speed: the pose write is not shown"
    worst_pos = max(s["kd"] - POSE_STEP_MARGIN * math.hypot(*s["vel"]) / 60.0 for s in judged)
    worst_up = max(s["kup"] for s in judged)
    if worst_pos > MAX_KD or worst_up > MAX_KUP:
        return False, (f"the pose the game reports differs from the simulation's by more than one step of motion (excess {worst_pos:.3f} m, up vector {worst_up:.3f}): "
                       f"the simulation's pose does not reach the game")
    return True, f"{len(judged)} steps at speed, the pose at the start of each step is the written one within a step of motion (excess {worst_pos:.4f} m, up {worst_up:.4f})"


def sim_hit_count(text):
    return len(re.findall(TAG + r"HIT st=\d+ .*\(in the simulation\)", text))


def judge_barrier(text):
    sims, takeovers, ends = parse_sim(text), parse_takeovers(text), parse_ends(text)
    early = [t for t in takeovers if t["st"] < MIN_CRASH_STEP]
    if early:
        return False, f"the simulation took the car at step {early[0]['st']}, before the crash (quiet driving must be untouched)"
    quiet = [s for s in parse_st(text) if s["st"] < MIN_CRASH_STEP]
    if len(quiet) < MIN_QUIET_LINES:
        return False, f"only {len(quiet)} state lines before the crash (need {MIN_QUIET_LINES}): the run is too short or the mod saw no car"
    if not takeovers:
        return False, "no takeover: the barrier hit never reached the simulation"
    if takeovers[0]["reason"] != 1:
        return False, f"the takeover reason is {takeovers[0]['reason']}, expected 1 (a hit)"
    if not sims:
        return False, "a takeover but no simulated step"
    if not all(finite(s) for s in sims):
        return False, "a non-finite value in the simulation"
    # the energy right after the hit impulse: the first simulated step's (the impulse is applied before it)
    e0 = sims[0]["E"]
    ground_e = sims[0]["comz"] - sims[0]["ground"]
    limit = e0 + ENERGY_TOLERANCE * abs(e0) + ENERGY_SLACK
    over = [s for s in sims if s["E"] > limit]
    if over:
        s = max(over, key=lambda x: x["E"])
        return False, f"energy {s['E']:.0f} J at step {s['st']} above the budget {limit:.0f} J (start {e0:.0f}): the car hops higher than it can"
    low = min(s["lowest"] for s in sims)
    if low < -MAX_BELOW_GROUND:
        return False, f"a hull point {-low:.3f} m below the ground (limit {MAX_BELOW_GROUND})"
    spin = [abs(s["w"][2]) for s in sims if s["upz"] < LOW_UP_Z]
    if spin and max(spin) > MAX_YAW_RATE:
        return False, f"|w_z| reaches {max(spin):.1f} rad/s while up.z < {LOW_UP_Z}: a spinning top"
    tail = sims[-120:]
    rest = all(math.hypot(*s["vel"]) < MAX_REST_SPEED and math.hypot(*s["w"]) < MAX_REST_SPEED for s in tail)
    handed = [e for e in ends if e["kind"] == "HANDBACK"]
    if not rest and not handed:
        return False, "the car neither comes to rest nor is handed back"
    # a hit at ~90 km/h into the barrier must TIP the car (the check is about a tumble, not a scrape): up.z under MIN_TIP_UP_Z at some step after the takeover
    hits = sim_hit_count(text)
    if hits > MAX_SIM_HITS:
        return False, f"{hits} impulses from the game's collision while simulating (limit {MAX_SIM_HITS}): the landing/rest of the car is being taken for a wall"
    tipped = min(s["upz"] for s in sims)
    if tipped >= MIN_TIP_UP_Z:
        return False, f"the hit never tipped the car: lowest up.z {tipped:.2f} (need under {MIN_TIP_UP_Z})"
    if not handed:  # a valid rest pose: still, held by at least 3 contacts, not in the ground
        last = sims[-1]
        if last["contacts"] < 3 or last["lowest"] < -MAX_BELOW_GROUND or not all(math.isfinite(x) for x in (*last["pos"], last["upz"])):
            return False, f"the final pose is not a valid rest pose (contacts {last['contacts']}, lowest {last['lowest']:.3f})"
    final = "handed back" if handed else f"at rest (up.z {sims[-1]['upz']:.2f})"
    return True, (f"takeover at step {takeovers[0]['st']} (hit), {len(sims)} simulated steps, peak energy {max(s['E'] for s in sims):.0f} J of {limit:.0f} allowed, "
                  f"lowest point {low:.3f} m, max |w_z| while down {max(spin) if spin else 0:.1f} rad/s, {final}; ground height gap {ground_e:.2f} m")


def judge_drop(text, restitution=RESTITUTION, drop=2.0):
    sims, ends = parse_sim(text), parse_ends(text)
    if not sims:
        return False, "no simulated step: the drop never reached the simulation"
    first = sims[0]
    h0 = first["lowest"]  # the hull's lowest point over the ground when the drop starts (falling, not yet in contact)
    if h0 < drop * 0.7:
        return False, f"the drop started {h0:.2f} m over the ground, not ~{drop} m"
    # the first impact: the first step where the lowest point is at the contact tolerance; the rebound is read from the vertical speed (the lowest point is no measure once
    # the landing rocks the car: one end stays down while the other lifts): the fastest upward speed in the next REBOUND_STEPS steps, as the height it would reach
    hit = next((i for i, s in enumerate(sims) if s["lowest"] <= 0.02), None)
    if hit is None:
        return False, "the car never reached the ground"
    v_imp = -min(s["vel"][2] for s in sims[:hit + 1])
    v_up = max(s["vel"][2] for s in sims[hit:hit + REBOUND_STEPS])
    apex = v_up * v_up / (2 * G)
    limit = BOUNCE_FACTOR * restitution * restitution * h0
    if apex > limit:
        return False, f"the first rebound reaches {apex:.3f} m, above {BOUNCE_FACTOR} e^2 h = {limit:.3f} m (e {restitution}, drop {h0:.2f} m, impact {v_imp:.1f} m/s)"
    if apex < MIN_BOUNCE_FACTOR * restitution * restitution * h0:
        return False, f"the rebound {apex:.3f} m is under {MIN_BOUNCE_FACTOR} e^2 h = {MIN_BOUNCE_FACTOR * restitution * restitution * h0:.3f} m: it did not bounce like a body of restitution {restitution}"
    if sim_hit_count(text) > MAX_SIM_HITS:
        return False, f"{sim_hit_count(text)} impulses from the game's collision while simulating (limit {MAX_SIM_HITS})"
    handed = [e for e in ends if e["kind"] == "HANDBACK"]
    if not handed:
        return False, "the car lies at rest upright but was never handed back"
    ok, why = judge_resume(text, handed[0])
    if not ok:
        return False, why
    return True, f"drop from {h0:.2f} m, first rebound {apex:.3f} m (limit {limit:.3f} m), handed back after {handed[0]['after']} steps; {why}"


def judge_resume(text, handback):
    """After the handback the game drives again: the throttle (held by the route) accelerates the car."""
    st = [s for s in parse_st(text) if s["st"] >= handback["st"]]
    if not st:
        return False, "no state line after the handback"
    first = st[0]
    later = [s for s in st if first["st"] + RESUME_STEPS - 30 <= s["st"] <= first["st"] + RESUME_STEPS + 60]
    if not later:
        return False, f"the run ends before {RESUME_STEPS} steps after the handback"
    gain = max(s["speed"] for s in later) - first["speed"]
    if gain < MIN_RESUME_GAIN:
        return False, f"driving does not resume: the speed gains {gain:.1f} m/s in {RESUME_STEPS} steps after the handback (need {MIN_RESUME_GAIN})"
    upright = min(s["upz"] for s in st[:20])
    if upright < 0.9:
        return False, f"the car is not upright after the handback (up.z {upright:.2f})"
    return True, f"driving resumes (+{gain:.1f} m/s in {RESUME_STEPS} steps)"


def judge_slide(text, mu=0.7):
    """The drop run's car arrives with a forward speed: Coulomb friction stops it. From its first moving step on its floor to the first step AFTER its last moving step at which it
    is still, it travels v0^2 / (2 mu g) within SLIDE_TOLERANCE (a car that slides on without friction never stops; one that stops too soon or too late has the wrong friction)."""
    sims = parse_sim(text)
    ends = parse_ends(text)
    if ends:  # the first simulation episode only (the drop): the route's barrier crash comes later and has its own sliding
        sims = [s for s in sims if s["st"] <= ends[0]["st"]]
    if not sims:
        return False, "no simulated step"
    horiz = lambda s: math.hypot(s["vel"][0], s["vel"][1])  # noqa: E731
    moving = [i for i, s in enumerate(sims) if s["bottom"] >= 3 and horiz(s) > SLIDE_MIN_SPEED]
    if not moving:
        return False, "the car never slid on its floor: the friction check shows nothing"
    first, last = moving[0], moving[-1]
    stopped = next((i for i in range(last + 1, len(sims)) if horiz(sims[i]) < 0.3), None)
    if stopped is None:
        return False, "the sliding car never stops: no friction"
    v0 = horiz(sims[first])
    travelled = math.hypot(sims[stopped]["pos"][0] - sims[first]["pos"][0], sims[stopped]["pos"][1] - sims[first]["pos"][1])
    expected = v0 * v0 / (2 * mu * G)
    if abs(travelled - expected) > SLIDE_TOLERANCE * expected:
        return False, f"the car slid {travelled:.2f} m from {v0:.2f} m/s, expected {expected:.2f} m (mu {mu}) within {SLIDE_TOLERANCE:.0%}"
    return True, f"slid {travelled:.2f} m from {v0:.2f} m/s (expected {expected:.2f} m), still after {sims[stopped]['n'] - sims[first]['n']} steps"


def judge_replay(text):
    marker = text.find("REPLAY start")
    if marker < 0:
        return False, "no REPLAY start line: the mod never saw the player's car in its replay"
    live, rep = text[:marker], text[marker:]
    lt, rt = parse_takeovers(live), parse_takeovers(rep)
    if not lt:
        return False, "no live takeover"
    if not rt:
        return False, "no takeover in the replay"
    ls = {s["n"]: s for s in parse_sim(live)}
    rs = {s["n"]: s for s in parse_sim(rep)}
    common = sorted(set(ls) & set(rs))
    if len(common) < 30:
        return False, f"only {len(common)} simulated steps in both"
    drift, at = max((math.dist(rs[n]["pos"], ls[n]["pos"]), n) for n in common)
    if drift > MAX_REPLAY_DRIFT:
        return False, f"the replay's simulated car drifts {drift:.2f} m from the live one {at} steps after the takeover (limit {MAX_REPLAY_DRIFT})"
    return True, f"takeover live (step {lt[0]['st']}) and in the replay (step {rt[0]['st']}), {len(common)} steps compared, drift {drift:.3f} m at +{at}"


def read(path):
    with open(path, errors="replace") as handle:
        return handle.read()


def main(argv):
    out, mode = argv[1], argv[2]
    label = {"sim": "sim", "drop": "drop", "replay": "replay", "others": "sim"}[mode]
    text = read(os.path.join(out, label, "run.log"))
    verdicts = []
    if mode in ("sim", "others"):
        verdicts.append(("barrier", judge_barrier(text)))
        verdicts.append(("pose", judge_pose(parse_sim(text))))
    elif mode == "drop":
        verdicts.append(("drop", judge_drop(text)))
        verdicts.append(("slide", judge_slide(text)))
        verdicts.append(("pose", judge_pose(parse_sim(text))))
    elif mode == "replay":
        verdicts.append(("replay", judge_replay(text)))
    ok = True
    for name, (good, message) in verdicts:
        print(("PASS " if good else "FAIL ") + f"{name}: {message}")
        ok = ok and good
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
