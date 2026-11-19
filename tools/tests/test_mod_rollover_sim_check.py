import math

import pytest

from tools import mod_rollover_sim_check as sc


def sim(st, n, pos=(-300.0, 250.0, 1.0), vel=(0.0, 0.0, 0.0), w=(0.0, 0.0, 0.0), upz=1.0, lowest=0.0, contacts=4, bottom=4, ground=0.0, E=0.0, comz=1.0, kd=(0.0, 0.0)):
    f = lambda v: ",".join(f"{x:.3f}" for x in v)  # noqa: E731
    return (f"[mod:rollover] SIM st={st} n={n} pos={f(pos)} vel={f(vel)} w={f(w)} up.z={upz:.3f} lowest={lowest:.3f} contacts={contacts}/{bottom} ground={ground:.3f} "
            f"E={E:.0f} com.z={comz:.3f} kd={kd[0]:.4f},{kd[1]:.4f}")


def st_line(st, speed=30.0, upz=1.0, since=0, pos=(-300.0, 250.0, 1.0)):
    f = lambda v: ",".join(f"{x:.3f}" for x in v)  # noqa: E731
    return f"[mod:rollover] ST st={st} pos={f(pos)} vel={speed:.2f},0.00,0.00 speed={speed:.2f} up.z={upz:.3f} since={since}"


def takeover(st=1855, reason=1):
    return f"[mod:rollover] TAKEOVER st={st} reason={reason} v=-30.0,-8.0,0.0 w=0.0,0.0,0.0 up.z=1.000"


def ends(kind="HANDBACK", st=2100, after=200):
    return f"[mod:rollover] {kind} st={st} after={after}"


def barrier_log(**kw):
    lines = [st_line(750 + i * 30) for i in range(40)]
    lines.append(takeover())
    e = 200000.0
    for n in range(1, 301):
        lines.append(sim(1855 + n, n, vel=(0, 0, 0) if n > 150 else (-5.0, 0.0, 0.0), w=(0, 0, 0), upz=1.0 if n > 150 else 0.1, E=e - 500 * n, lowest=0.01, **kw))
    return "\n".join(lines)


def test_parse_sim_and_friends():
    s = sc.parse_sim(sim(10, 2, pos=(1, 2, 3), vel=(4, 5, 6), upz=0.5, kd=(0.001, 0.002)))[0]
    assert s["st"] == 10 and s["n"] == 2 and s["pos"] == (1.0, 2.0, 3.0) and s["vel"] == (4.0, 5.0, 6.0) and s["upz"] == 0.5 and s["bottom"] == 4 and s["kd"] == 0.001 and s["kup"] == 0.002
    assert sc.parse_takeovers(takeover(7, 2)) == [{"st": 7, "reason": 2, "up": 1.0}]
    assert sc.parse_ends(ends("RESET", 9, 3)) == [{"kind": "RESET", "st": 9, "after": 3}]
    assert sc.parse_st(st_line(30, 12.5))[0]["speed"] == 12.5
    assert sc.parse_sim("garbage") == [] and sc.parse_takeovers("garbage") == []


def test_barrier_passes_and_each_defect_fails():
    ok, why = sc.judge_barrier(barrier_log())
    assert ok, why
    assert not sc.judge_barrier(barrier_log().replace("reason=1", "reason=3"))[0]  # not caused by a hit
    early = barrier_log().replace("TAKEOVER st=1855", "TAKEOVER st=900")
    assert not sc.judge_barrier(early)[0]
    no_takeover = "\n".join(line for line in barrier_log().splitlines() if "TAKEOVER" not in line and "SIM" not in line)
    assert "no takeover" in sc.judge_barrier(no_takeover)[1]
    lines = barrier_log().splitlines()
    lines[-50] = lines[-50].replace(lines[-50].split("E=")[1].split()[0], "400000")
    assert "energy" in sc.judge_barrier("\n".join(lines))[1]
    sunk = barrier_log().replace("lowest=0.010", "lowest=-0.200", 3)
    assert "below the ground" in sc.judge_barrier(sunk)[1]
    top = barrier_log().replace("w=0.000,0.000,0.000 up.z=0.100", "w=0.000,0.000,40.000 up.z=0.100")
    assert "spinning top" in sc.judge_barrier(top)[1]
    still_moving = barrier_log().replace("vel=0.000,0.000,0.000 w=0.000,0.000,0.000", "vel=3.000,0.000,0.000 w=0.000,0.000,0.000")
    assert "neither" in sc.judge_barrier(still_moving)[1]
    assert sc.judge_barrier(still_moving + "\n" + ends())[0]  # but handed back is fine
    assert "quiet" in sc.judge_barrier("\n".join(barrier_log().splitlines()[30:]))[1] or "state lines" in sc.judge_barrier("\n".join(barrier_log().splitlines()[30:]))[1]


def test_barrier_requires_a_tip_and_a_valid_rest_pose():
    no_tip = barrier_log().replace("up.z=0.100", "up.z=0.900")
    ok, why = sc.judge_barrier(no_tip)
    assert not ok and "never tipped" in why
    floating = barrier_log().replace("contacts=4/4", "contacts=1/1")  # final pose held by one contact
    assert "not a valid rest pose" in sc.judge_barrier(floating)[1]
    assert sc.judge_barrier(floating + "\n" + ends())[0]  # handed back: no rest pose asked


def test_sim_hits_are_bounded():
    spam = barrier_log() + "\n" + "\n".join(f"[mod:rollover] HIT st={2000 + i} dv=0.00,0.00,2.00 (in the simulation) n={i}" for i in range(30))
    assert "taken for a wall" in sc.judge_barrier(spam)[1]
    assert sc.judge_barrier(barrier_log() + "\n[mod:rollover] HIT st=1900 dv=4.00,0.00,0.00 (in the simulation) n=1")[0]


def drop_log(rebound=0.04, e_drop=2.0, handback=True, gain=12.0, upz_after=1.0):
    lines = [sim(100, 1, lowest=e_drop, vel=(4.0, 0, -0.1), bottom=0, contacts=0)]
    z = e_drop
    vz = 0.0
    n = 2
    while z > 0.02:  # free fall
        vz -= 0.163
        z += vz / 60
        lines.append(sim(100 + n, n, lowest=z, vel=(4.0, 0, vz), bottom=0, contacts=0))
        n += 1
    lines.append(sim(100 + n, n, lowest=0.0, vel=(3.9, 0, math.sqrt(2 * 9.81 * rebound)), bottom=4, contacts=4)); n += 1
    lines.append(sim(100 + n, n, lowest=rebound, vel=(3.8, 0, 0.5), bottom=0, contacts=0)); n += 1
    lines.append(sim(100 + n, n, lowest=rebound, vel=(3.8, 0, -0.2), bottom=0, contacts=0)); n += 1
    for k in range(120):
        lines.append(sim(100 + n, n, lowest=0.0, vel=(max(0.0, 3.8 - 0.07 * k), 0, 0), bottom=4, contacts=4)); n += 1
    if handback:
        lines.append(ends("HANDBACK", 100 + n, n))
        lines.append(st_line(100 + n, speed=2.0, upz=upz_after))
        lines.append(st_line(100 + n + RESUME, speed=2.0 + gain, upz=1.0))
    return "\n".join(lines)


RESUME = sc.RESUME_STEPS


def test_drop_bounce_bounds():
    ok, why = sc.judge_drop(drop_log())
    assert ok, why
    too_high = sc.judge_drop(drop_log(rebound=0.5))
    assert not too_high[0] and "above" in too_high[1]
    no_bounce = sc.judge_drop(drop_log(rebound=0.001))
    assert not no_bounce[0] and "did not bounce" in no_bounce[1]
    assert "handed back" in sc.judge_drop(drop_log(handback=False))[1]
    assert "driving does not resume" in sc.judge_drop(drop_log(gain=0.5))[1]
    assert "not upright" in sc.judge_drop(drop_log(upz_after=0.2))[1]
    assert not sc.judge_drop("nothing")[0]
    low = sc.judge_drop(drop_log(e_drop=0.5))
    assert not low[0] and "not ~2.0" in low[1]


def slide_log(v0=10.0, mu=0.7, stop_early=False, never=False):
    """A car landing flat at v0 and sliding to rest by Coulomb friction: v = sqrt(v0^2 - 2 mu g x)."""
    lines, x, v, n = [], 0.0, v0, 1
    while v > 0.05 or n < 3:
        v = 0.0 if stop_early and n > 5 else (v0 if never else math.sqrt(max(v * v - 2 * mu * 9.81 * (v / 60.0), 0.0)))
        x += v / 60.0
        lines.append(sim(100 + n, n, pos=(x, 0.0, 1.0), vel=(v, 0, 0), bottom=4, contacts=4))
        n += 1
        if n > 600:
            break
    for k in range(0 if never else 30):  # at rest
        lines.append(sim(100 + n + k, n + k, pos=(x, 0.0, 1.0), vel=(0, 0, 0), bottom=4, contacts=4))
    return "\n".join(lines)


def test_slide_needs_friction():
    ok, why = sc.judge_slide(slide_log())
    assert ok, why
    assert "never stops" in sc.judge_slide(slide_log(never=True))[1]  # no friction: it slides on
    ok, why = sc.judge_slide(slide_log(stop_early=True))
    assert not ok and "expected" in why  # it stops far too soon: too much friction
    assert not sc.judge_slide(sim(1, 1, bottom=4, vel=(0, 0, 0)))[0]  # never moved: shows nothing
    # only the first episode counts: a later fast slide (the barrier tumble) does not rescue a drop that stopped dead
    first = slide_log(stop_early=True)
    later = "\n".join(sim(5000 + n, n, pos=(100.0 + n * 0.2, 0, 1), vel=(12.0, 0, 0), bottom=4, contacts=4) for n in range(1, 60))
    assert not sc.judge_slide(first + "\n" + ends("HANDBACK", 4000, 50) + "\n" + later)[0]


def test_pose_reaches_the_game():
    v = 10.0
    step = v / 60
    good = "\n".join(sim(100 + n, n, vel=(v, 0, 0), kd=(step, 0.001)) for n in range(1, 40))
    ok, why = sc.judge_pose(sc.parse_sim(good))
    assert ok, why
    bad = "\n".join(sim(100 + n, n, vel=(v, 0, 0), kd=(step * 1.5 + 0.3 + 0.3 * n, 0.0)) for n in range(1, 40))  # the simulation runs away from the car: the pose never arrives
    ok, why = sc.judge_pose(sc.parse_sim(bad))
    assert not ok and "does not reach the game" in why
    twisted = "\n".join(sim(100 + n, n, vel=(v, 0, 0), kd=(step, 0.4)) for n in range(1, 40))
    assert not sc.judge_pose(sc.parse_sim(twisted))[0]
    slow = "\n".join(sim(100 + n, n, vel=(0.5, 0, 0), kd=(0.0, 0.0)) for n in range(1, 40))
    assert not sc.judge_pose(sc.parse_sim(slow))[0]  # nothing proven at rest


def test_replay_pairs_by_steps_since_takeover():
    live = [takeover(900)] + [sim(900 + n, n, pos=(-300.0 + n * 0.1, 250.0, 1.0)) for n in range(1, 60)]
    rep = ["[mod:rollover] REPLAY start", takeover(905)] + [sim(905 + n, n, pos=(-300.0 + n * 0.1, 250.0, 1.0)) for n in range(1, 60)]
    ok, why = sc.judge_replay("\n".join(live + rep))
    assert ok, why
    drift = ["[mod:rollover] REPLAY start", takeover(905)] + [sim(905 + n, n, pos=(-300.0 + n * 0.1 + 2.0, 250.0, 1.0)) for n in range(1, 60)]
    assert "drifts" in sc.judge_replay("\n".join(live + drift))[1]
    assert "no REPLAY start" in sc.judge_replay("\n".join(live))[1]
    no_rep_takeover = ["[mod:rollover] REPLAY start"]
    assert "no takeover in the replay" in sc.judge_replay("\n".join(live + no_rep_takeover))[1]
