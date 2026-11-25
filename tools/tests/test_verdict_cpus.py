"""tools/verdict.sh: the gate and synthetic CPU sets come from GT_GATE_CPUS / GT_SYNTH_CPUS (desktop halves by default).

Runs the script with `taskset` and the heavy steps replaced by stubs that record their arguments, in a scratch tree."""
import os
import re
import subprocess
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]


def run_verdict(tmp_path: Path, env_extra: dict[str, str], fail_fetches: int = 0, scope: str = "synthetic",
                keep_scope: bool = False, gate_has_only: bool = True, keep_pytest: bool = False) -> list[str]:
    tree = tmp_path / "tree"
    bindir = tmp_path / "bin"
    bindir.mkdir()
    calls = tmp_path / "calls.txt"
    # A tree that is a git repo with an origin branch, so the script's fetch/checkout succeed.
    origin = tmp_path / "origin"
    subprocess.run(["git", "init", "-q", "-b", "main", str(origin)], check=True)
    subprocess.run(["git", "-C", str(origin), "-c", "user.email=t@t", "-c", "user.name=t", "commit", "-q", "--allow-empty", "-m", "x"], check=True)
    subprocess.run(["git", "clone", "-q", str(origin), str(tree)], check=True)
    for name in ("taskset", "systemd-run", "ctest", "cmake"):
        stub = bindir / name
        stub.write_text(f'#!/bin/sh\necho "{name} $*" >> {calls}\n')
        stub.chmod(0o755)
    if fail_fetches:  # a git that fails its first N fetches, as a concurrent verdict's ref lock does (verdict 615)
        real_git = subprocess.run(["sh", "-c", "command -v git"], capture_output=True, text=True, check=True).stdout.strip()
        count = tmp_path / "fetches"
        git = bindir / "git"
        git.write_text(f'#!/bin/sh\nif [ "$1" = fetch ]; then n=$(cat {count} 2>/dev/null || echo 0); echo $((n + 1)) > {count}\n'
                       f'  [ "$n" -lt {fail_fetches} ] && {{ echo "error: cannot lock ref" >&2; exit 1; }}; fi\nexec {real_git} "$@"\n')
        git.chmod(0o755)
    (tree / "tools").mkdir()
    # the tree's decomp_gate.py: verdict.sh passes `full --only` only when it has the argument
    (tree / "tools" / "decomp_gate.py").write_text('full.add_argument("--only")\n' if gate_has_only else "")
    for name in ("setup.sh", "cpu_lock.sh", "pad_route.sh"):
        stub = tree / "tools" / name
        # cpu_lock.sh records the boot route's GT_PAD_EXIT_AFTER (the boot runs through it)
        record = f'echo "cpu_lock exit_after=${{GT_PAD_EXIT_AFTER:-unset}} $*" >> {calls}\n' if name == "cpu_lock.sh" else ""
        stub.write_text(f"#!/bin/sh\n{record}exit 0\n")
        stub.chmod(0o755)
    venv = tree / ".venv" / "bin"
    venv.mkdir(parents=True)
    for name in ("python", "pytest"):
        stub = venv / name
        # tools.verdict_scope answers `scope` ("synthetic" by default) so the synthetic groups run too
        stub.write_text(f'#!/bin/sh\ncase "$*" in *verdict_scope*) echo "scope $*" >> {calls}; echo "{scope}";; esac\nexit 0\n')
        stub.chmod(0o755)
    (tree / "game_files" / "UCES01245-GAMEDAT").mkdir(parents=True)
    # The caller's own GT_GATE_CPUS / GT_SYNTH_CPUS / GT_VERDICT_SCOPE must not leak in (the runners export them: verdicts
    # 580 and 846).
    base = {k: v for k, v in os.environ.items() if k not in ("GT_GATE_CPUS", "GT_SYNTH_CPUS", "GT_VERDICT_SCOPE")}
    env = dict(base, PATH=f"{bindir}:{os.environ['PATH']}", GT_VERIFY_TREE=str(tree), **env_extra)
    subprocess.run(["bash", str(REPO / "tools" / "verdict.sh"), "main", "t"], env=env, cwd=tree, capture_output=True, timeout=60)
    lines = calls.read_text().splitlines() if calls.exists() else []
    gates = [line for line in lines if line.startswith("taskset") and (keep_pytest or ".venv/bin/pytest" not in line)]
    return gates + [line for line in lines if keep_scope and line.startswith("scope ")]


def test_defaults_split_the_desktop(tmp_path):
    lines = run_verdict(tmp_path, {})
    # gate + five synthetic groups (every function installed); their order depends on GT_VERIFY_OVERLAP
    gate = [line for line in lines if "decomp_gate full" in line]
    synth = [line for line in lines if "decomp_gate full" not in line]
    assert len(gate) == 1 and gate[0].startswith("taskset -c 6-11,18-23 ")
    assert len(synth) == 5 and all(line.startswith("taskset -c 0-5,12-17 ") for line in synth)


def test_environment_overrides_both_sets(tmp_path):
    lines = run_verdict(tmp_path, {"GT_GATE_CPUS": "0-11", "GT_SYNTH_CPUS": "0-11"})
    assert len(lines) == 6 and all(line.startswith("taskset -c 0-11 ") for line in lines)


def test_a_failed_fetch_is_retried(tmp_path):
    # two verdicts share one object store; a fetch that loses the ref lock must not end the verdict as "no branch"
    assert len(run_verdict(tmp_path, {"GT_FETCH_RETRY_S": "0"}, fail_fetches=2)) == 6


def test_a_fetch_that_keeps_failing_still_stops(tmp_path):
    assert run_verdict(tmp_path, {"GT_FETCH_RETRY_S": "0"}, fail_fetches=9) == []


def test_synth_groups_override(tmp_path):
    # GT_VERIFY_SYNTH_GROUPS replaces the scene groups: one group, one synthetic taskset line
    lines = run_verdict(tmp_path, {"GT_VERIFY_SYNTH_GROUPS": "dealer,race"})
    assert len(lines) == 2 and "GT_SYNTH_ROUTE" not in lines[1]


def test_boot_budget_defaults_to_600_and_is_overridable(tmp_path):
    text = (REPO / "tools" / "verdict.sh").read_text()
    assert 'tools/pad_route.sh save_garage "${GT_VERIFY_BOOT_SECONDS:-600}"' in text


def test_boot_ends_a_minute_after_the_script(tmp_path):
    # The boot check passes on "script done"; without GT_PAD_EXIT_AFTER the route idled to its wall-clock budget
    # (verdict 837: script done at 6 min of game, the step took 20 min).
    run_verdict(tmp_path, {})
    boot = [line for line in (tmp_path / "calls.txt").read_text().splitlines() if line.startswith("cpu_lock") and "verdict boot" in line]
    assert len(boot) == 1 and boot[0].startswith("cpu_lock exit_after=3600 ")


def test_boot_exit_after_can_be_overridden(tmp_path):
    run_verdict(tmp_path, {"GT_VERIFY_BOOT_EXIT_AFTER": "60"})
    boot = [line for line in (tmp_path / "calls.txt").read_text().splitlines() if line.startswith("cpu_lock") and "verdict boot" in line]
    assert len(boot) == 1 and boot[0].startswith("cpu_lock exit_after=60 ")


def test_synth_installs_every_function_by_default(tmp_path):
    synth = [line for line in run_verdict(tmp_path, {}) if "decomp_gate synth" in line]
    assert len(synth) == 5 and not any("--only" in line for line in synth)


def test_synth_installs_only_the_prs_functions_when_the_scope_says_so(tmp_path):
    # tools.verdict_scope "only N": the PR changes only decomp sources / shapes, so each group installs that list
    synth = [line for line in run_verdict(tmp_path, {}, scope="only 2") if "decomp_gate synth" in line]
    assert len(synth) == 3 and all(re.search(r"--only @\S*synth_only\.txt", line) for line in synth)


def test_skip_scope_runs_no_synth(tmp_path):
    assert [line for line in run_verdict(tmp_path, {}, scope="skip") if "decomp_gate synth" in line] == []


def test_scope_tool_can_come_from_the_verdict_branch(tmp_path):
    lines = run_verdict(tmp_path, {"GT_VERDICT_SCOPE": "/x/verdict_scope.py"}, keep_scope=True)
    scope = [line for line in lines if line.startswith("scope ")]
    assert len(scope) == 1 and scope[0].startswith("scope /x/verdict_scope.py origin/main HEAD --only-file ")


def test_scope_tool_defaults_to_the_trees_module(tmp_path):
    scope = [line for line in run_verdict(tmp_path, {}, keep_scope=True) if line.startswith("scope ")]
    assert len(scope) == 1 and scope[0].startswith("scope -m tools.verdict_scope origin/main HEAD --only-file ")


def test_gate_installs_only_the_prs_functions_when_the_scope_says_so(tmp_path):
    gate = [line for line in run_verdict(tmp_path, {}, scope="only 2") if "decomp_gate full" in line]
    assert len(gate) == 1 and re.search(r"decomp_gate full vt --only @\S*synth_only\.txt", gate[0])


def test_gate_installs_every_function_otherwise(tmp_path):
    for scope in ("synthetic", "skip"):
        (tmp_path / scope).mkdir()
        gate = [line for line in run_verdict(tmp_path / scope, {}, scope=scope) if "decomp_gate full" in line]
        assert len(gate) == 1 and "--only" not in gate[0], scope


def test_gate_keeps_every_function_when_the_tree_cannot_take_only(tmp_path):
    gate = [line for line in run_verdict(tmp_path, {}, scope="only 2", gate_has_only=False) if "decomp_gate full" in line]
    assert len(gate) == 1 and "--only" not in gate[0]


def test_pytest_runs_on_the_synthetic_cpus(tmp_path):
    """pytest runs beside the synthetic groups and the gate (about 4 min less per verdict), on the synthetic CPUs."""
    lines = run_verdict(tmp_path, {}, keep_pytest=True)
    pytest = [line for line in lines if ".venv/bin/pytest" in line]
    assert len(pytest) == 1 and pytest[0].startswith("taskset -c 0-5,12-17 ")


def test_full_scope_gate_waits_for_the_synthetic_groups(tmp_path):
    """Every function installed: the groups take up to ~30 min, so the gate waits for them (verdict 846 lost three
    groups to the lock cap beside the gate)."""
    lines = [line for line in run_verdict(tmp_path, {}, scope="synthetic") if "decomp_gate" in line]
    assert [("full" in line) for line in lines] == [False] * 5 + [True]
