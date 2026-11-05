from tools.mod_pedals_check import judge


def lines(rows):
    return "\n".join(
        f"[mod:analog-pedals] step={s} player thr={pt} brk={pb} eff={pt} ai thr={at} brk={ab}" for s, pt, pb, at, ab in rows
    )


GOOD = [(600 + 60 * i, 0.5, 0.3, 0.9, 0.0) for i in range(1, 8)]


def test_passes_when_player_matches_and_ai_differs():
    ok, message = judge(lines(GOOD))
    assert ok, message


def test_fails_without_mod_lines():
    ok, message = judge("[mods] loaded: none\n")
    assert not ok and "did not run" in message


def test_fails_when_throttle_is_off():
    rows = GOOD[:-1] + [(1200, 0.0, 0.3, 0.9, 0.0)]
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


def test_fails_when_only_the_game_response_shows():
    rows = [(s, 0.1035, 0.37, at, ab) for s, _, _, at, ab in GOOD]
    assert not judge(lines(rows))[0]


# --- judge_toggle: the mod menu turns the mod off mid-race (GT_MOD_MENU_TEST_TOGGLE) ---
from tools.mod_pedals_check import judge_toggle  # noqa: E402

TOGGLE = "[mods] analog-pedals -> OFF"


def test_toggle_passes_when_mod_logs_stop_at_the_toggle_line():
    ok, message = judge_toggle(lines(GOOD) + "\n" + TOGGLE + "\nscript done at vblank 9\n")
    assert ok, message


def test_toggle_fails_when_the_mod_keeps_running_after_the_toggle():
    ok, message = judge_toggle(lines(GOOD[:6]) + "\n" + TOGGLE + "\n" + lines(GOOD[6:]) + "\n")
    assert not ok and "after" in message


def test_toggle_fails_without_the_toggle_line():
    ok, message = judge_toggle(lines(GOOD))
    assert not ok and "toggle" in message


def test_toggle_fails_when_the_mod_never_injected_before():
    ok, message = judge_toggle(lines([(60, 0.0, 0.0, 0.0, 0.0)]) + "\n" + TOGGLE + "\n")
    assert not ok and "before" in message


def test_toggle_fails_when_pedals_before_are_off_target():
    rows = [(s, 0.0, pb, at, ab) for s, _, pb, at, ab in GOOD]
    ok, message = judge_toggle(lines(rows) + "\n" + TOGGLE + "\n")
    assert not ok and "before" in message
