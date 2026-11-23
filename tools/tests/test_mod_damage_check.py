import pytest

from tools import mod_damage_check as md


def s(st, mv, v, yaw=0.0, role=1, vb=None, tin=1.0, tout=1.0):
    return (f"[mod:mechanical-damage] S st={st} mv={mv} vb={vb if vb is not None else st * 2} v={v:.3f} thr={tin:.3f}/{tout:.3f} yaw={yaw:.4f} pos=1.00,2.00,3.00 role={role}")


def hit(engine=0.95, drive=0.0, align=0.475, dv=25.0, step=100):
    return (f"[mod:mechanical-damage] HIT step={step} dv={dv:.2f} contact=(1.00 0.00 0.00) strength=0.950 -> engine={engine:.3f} drive={drive:.3f} align={align:.3f}")


def run(speed, n=1000, moved=50, yaw=0.0, role=1, start=0):
    """steps start..start+n; the car moves at step `moved`; speed(k) with k = steps since it moved"""
    lines = []
    for st in range(start, start + n):
        k = st - moved
        lines.append(s(st, moved if st >= moved else -1, speed(k) if k >= 0 else 0.0, yaw(k) if callable(yaw) else yaw, role))
    return lines


def accel(rate, top=60.0):
    return lambda k: min(top, rate * k)


def test_accel_table_and_ratio():
    rows = md.by_k(md.states("\n".join(run(accel(0.1)))))
    table = md.accel_table(rows)
    assert all(abs(v - 0.1) < 1e-3 for b, v in table.items() if b * md.BIN < 58) and len(table) > 20
    slow = md.by_k(md.states("\n".join(run(accel(0.06)))))
    assert abs(md.accel_ratio(rows, slow, 35, 200) - 0.6) < 0.02
    assert md.accel_ratio(rows, slow, 5000, 5100) is None  # nothing to compare


def test_power_passes_for_a_reduced_gain_and_fails_for_none_or_too_much():
    base = "\n".join(run(accel(0.1)))
    good = "\n".join([hit()] + run(accel(0.06)))
    assert md.judge_power(base, good)[0]
    same = "\n".join([hit()] + run(accel(0.1)))
    assert not md.judge_power(base, same)[0]  # sabotage: no effect
    huge = "\n".join([hit()] + run(accel(0.01)))
    assert not md.judge_power(base, huge)[0]
    assert not md.judge_power(base, "\n".join(run(accel(0.06))))[0]  # no hit line


def test_repair_needs_the_gain_back_after_the_repair():
    base = "\n".join(run(accel(0.1)))
    def damaged(k):  # slower up to k=200, the undamaged rate after
        return 0.06 * k if k < 200 else 12.0 + 0.1 * (k - 200)
    good = "\n".join([hit(), "[mod:mechanical-damage] REPAIR"] + run(damaged))
    assert md.judge_repair(base, good)[0]
    never = "\n".join([hit(), "[mod:mechanical-damage] REPAIR"] + run(accel(0.06)))
    assert not md.judge_repair(base, never)[0]
    assert not md.judge_repair(base, "\n".join([hit()] + run(damaged)))[0]  # no REPAIR line


def test_cap():
    base = "\n".join(run(accel(0.1, top=38.0)))
    capped = "\n".join([hit(engine=0.0, drive=1.0)] + run(accel(0.1, top=25.0)))
    assert md.judge_cap(base, capped)[0]
    assert not md.judge_cap(base, "\n".join([hit(engine=0.0, drive=1.0)] + run(accel(0.1, top=38.0))))[0]
    assert not md.judge_cap("\n".join(run(accel(0.1, top=20.0))), capped)[0]  # premise: the undamaged car must pass the cap
    assert not md.judge_cap(base, "\n".join(run(accel(0.1, top=25.0))))[0]  # no hit


def test_pull_sign_and_size():
    base = "\n".join(run(accel(0.1)))
    right = "\n".join([hit(engine=0.0, align=1.0)] + run(accel(0.1), yaw=-0.06))
    left = "\n".join([hit(engine=0.0, align=-1.0)] + run(accel(0.1), yaw=0.06))
    assert md.judge_pull(base, right, 1)[0] and md.judge_pull(base, left, -1)[0]
    assert not md.judge_pull(base, right, -1)[0]  # wrong side
    assert not md.judge_pull(base, "\n".join([hit(engine=0.0, align=1.0)] + run(accel(0.1))), 1)[0]  # no pull
    assert not md.judge_pull(base, "\n".join(run(accel(0.1), yaw=-0.06)), 1)[0]  # no hit line


def test_replay_rebuilds_the_damage():
    live = [hit()] + run(accel(0.06))
    rep = ["[mod:mechanical-damage] RACE replay", hit()] + run(accel(0.06), role=3)
    assert md.judge_replay("\n".join(live + rep))[0]
    slow_live = "\n".join(live + ["[mod:mechanical-damage] RACE replay"] + run(accel(0.1), role=3))
    assert not md.judge_replay(slow_live)[0]  # no hit in the replay
    undamaged_replay = "\n".join(live + ["[mod:mechanical-damage] RACE replay", hit()] + run(accel(0.1), role=3))
    assert not md.judge_replay(undamaged_replay)[0]  # the replayed car is not slowed
    assert not md.judge_replay("\n".join(live))[0]  # never saw the replay


def test_toggle():
    off = "[mods] mechanical-damage -> OFF"
    assert md.judge_toggle("\n".join(run(accel(0.1), n=10) + [off]))[0]
    assert not md.judge_toggle("\n".join(run(accel(0.1), n=10) + [off] + run(accel(0.1), n=5)))[0]
    assert not md.judge_toggle("\n".join(run(accel(0.1), n=10)))[0]
    assert not md.judge_toggle("\n".join([off] + run(accel(0.1), n=5)))[0]


def test_sanity_and_main(tmp_path, capsys):
    assert md.judge_sanity("\n".join(run(accel(0.1), n=5)))[0]
    bad = "\n".join(run(accel(0.1), n=5)).replace("v=", "v=nan", 1)
    assert not md.judge_sanity(bad)[0]
    for label, lines in (("idle", run(accel(0.1))), ("front", [hit()] + run(accel(0.06)))):
        d = tmp_path / label
        d.mkdir()
        (d / "run.log").write_text("\n".join(lines))
    assert md.main(["x", str(tmp_path), "power"]) == 0
    assert md.main(["x", str(tmp_path), "nope"]) == 2


def test_idle_frames(tmp_path):
    def make(label, byte_for_vb):
        d = tmp_path / label
        (d / "frames").mkdir(parents=True, exist_ok=True)
        log = ["[mod:mechanical-damage] placeholder"] + run(accel(0.1), n=600)
        for i in range(40):
            vb = 30 * i
            (d / "frames" / f"frame_{i:06d}.ppm").write_bytes(bytes([byte_for_vb(vb)]))
            log.append(f"[frame-dump] frame {i} vblank {vb}")
        (d / "run.log").write_text("\n".join(log))
    make("off", lambda vb: 1)
    make("idle", lambda vb: 1)
    ok, detail = md.judge_idle(str(tmp_path / "off"), str(tmp_path / "idle"))
    assert ok, detail
    make("idle", lambda vb: 2 if vb == 300 else 1)
    assert not md.judge_idle(str(tmp_path / "off"), str(tmp_path / "idle"))[0]
