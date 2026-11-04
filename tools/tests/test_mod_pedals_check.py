from tools.mod_pedals_check import judge


def lines(rows):
    return "\n".join(
        f"[mod:analog-pedals] step={s} player thr={pt} brk={pb} eff={pt} ai thr={at} brk={ab}" for s, pt, pb, at, ab in rows
    )


GOOD = [(600 + 60 * i, 0.1035, 0.37, 0.9, 0.0) for i in range(1, 8)]


def test_passes_when_player_matches_and_ai_differs():
    ok, message = judge(lines(GOOD))
    assert ok, message


def test_fails_without_mod_lines():
    ok, message = judge("[mods] loaded: none\n")
    assert not ok and "did not run" in message


def test_fails_when_throttle_is_off():
    rows = GOOD[:-1] + [(1200, 0.0, 0.37, 0.9, 0.0)]
    assert not judge(lines(rows))[0]


def test_fails_when_brake_is_off():
    rows = [(s, pt, 0.0, at, ab) for s, pt, _, at, ab in GOOD]
    assert not judge(lines(rows))[0]


def test_fails_when_ai_shows_the_injected_values_too():
    rows = [(s, pt, pb, pt, pb) for s, pt, pb, _, _ in GOOD]
    ok, message = judge(lines(rows))
    assert not ok and "AI" in message


def test_ignores_samples_before_settling():
    rows = [(60, 0.0, 0.0, 0.0, 0.0)] + GOOD
    assert judge(lines(rows))[0]
