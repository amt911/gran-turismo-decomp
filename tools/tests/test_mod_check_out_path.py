"""Every mod in-game check takes OUT relative to the repo or absolute: the scripts join "$repo/$out", so an absolute OUT
must be turned into a path relative to the repo first (an absolute one silently pointed GT_MODS_DIR at a missing folder:
no mod loaded, and the check failed for the wrong reason)."""
import subprocess
from pathlib import Path

import pytest

TOOLS = Path(__file__).resolve().parents[1]
SCRIPTS = sorted(TOOLS.glob("mod_*_check.sh")) + [TOOLS / "mods_together_check.sh"]


def resolve(script: Path, repo: str, out: str) -> str:
    lines = script.read_text().splitlines()
    start = next(i for i, line in enumerate(lines) if line.startswith("out=${OUT:-"))
    snippet = "\n".join(lines[start:start + 2])
    result = subprocess.run(["bash", "-c", f'repo={repo}\n{snippet}\nprintf %s "$out"'], env={"OUT": out, "PATH": "/usr/bin:/bin"},
                            capture_output=True, text=True, check=True)
    return result.stdout


@pytest.mark.parametrize("script", SCRIPTS, ids=lambda p: p.name)
def test_absolute_out_becomes_relative_to_the_repo(script):
    assert resolve(script, "/a/repo", "/tmp/x/check") == "../../tmp/x/check"


@pytest.mark.parametrize("script", SCRIPTS, ids=lambda p: p.name)
def test_relative_out_is_kept(script):
    assert resolve(script, "/a/repo", "recomp/analysis/here") == "recomp/analysis/here"
