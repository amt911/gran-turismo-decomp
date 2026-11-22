"""Every active registration names a function start of the recompiled table (spec 2026-11-19-synthetic-pointer-graphs-design.md, item 6):
a registration for an address the recomp has no function at is "not installed" in every gate run and sits silently.

The four Adhoc script natives below are entry points inside larger functions of recomp/analysis/functions.tsv (splat's table); they are
registered from src/game/adhoc and install fine, so they are allowed with their reason."""

from pathlib import Path

import pytest

from tools.progress import LOAD_BASE, decompiled_functions, load_functions

ROOT = Path(__file__).resolve().parents[2]
FUNCTIONS = ROOT / "recomp" / "analysis" / "functions.tsv"

ALLOWED = {
    0x089A3F54: "MRootWindow_getFocusedWidget: Adhoc script native, an entry 0x624 bytes into the function at 0x089A3930",
    0x08A20414: "Adhoc script native, an entry 0xB4 bytes into the function at 0x08A20360",
    0x08A29540: "MSystemCondition_IsExitGame: Adhoc script native, an entry 0x138 bytes into the function at 0x08A29408",
    0x08A29730: "MSystemConfiguration_SetTemporalAntialias: Adhoc script native, an entry 0xA8 bytes into the function at 0x08A29688",
}


@pytest.mark.skipif(not FUNCTIONS.exists(), reason="needs the recompiled function table (recomp/analysis/functions.tsv)")
def test_active_registrations_are_function_starts():
    starts = load_functions(FUNCTIONS)
    missing = sorted(a + LOAD_BASE for a in decompiled_functions(ROOT / "src") if a not in starts)
    unexplained = [f"{a:#010x}" for a in missing if a not in ALLOWED]
    assert not unexplained, f"registered at addresses with no recompiled function and no allow-list reason: {unexplained}"


@pytest.mark.skipif(not FUNCTIONS.exists(), reason="needs the recompiled function table (recomp/analysis/functions.tsv)")
def test_the_allow_list_has_no_stale_entries():
    starts = load_functions(FUNCTIONS)
    registered = {a + LOAD_BASE for a in decompiled_functions(ROOT / "src")}
    stale = [f"{a:#010x}" for a in ALLOWED if a not in registered or (a - LOAD_BASE) in starts]
    assert not stale, f"allow-listed addresses that are not registered or are function starts now: {stale}"
