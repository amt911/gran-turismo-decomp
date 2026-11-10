"""Does a PR need the synthetic gate? (tools/verdict.sh; user decision 2026-11-10, docs/agents/pr-verification.md)

The synthetic gate compares decompiled functions with their recompiled originals on generated cases. A PR whose changed paths
cannot reach that comparison (docs, mods' checks, pad routes, the renderer and the mod host) skips it; everything else runs it.
The rule is an allowlist: a path that is not listed counts as needing the gate, so a new kind of file errs on the safe side.
A changed lcs-recomp patch skips only when every file it modifies is renderer/display/menu code (lcs/host/ge_*, display*, lcs_*).

  python -m tools.verdict_scope BASE HEAD   -> prints "synthetic" or "skip"
"""
import re
import subprocess
import sys
from fnmatch import fnmatch
from pathlib import Path

SKIP = ["docs/*", "*.md", "config/pad_routes/*", "tools/mod_*", "tools/mods_together_*", "tools/tests/test_mod*",
        "tools/tests/test_verdict_scope.py", "tools/verdict_scope.py", "host/mod_*", "host/mini_json*", "host/pad_menu*",
        "host/play_backends.cpp", "host/ge_*", "host/frame_dump*", "host/dump_bucket*", "host/tests/*"]
PATCH_SKIP = ["lcs/host/ge_*", "lcs/host/vulkan/*", "lcs/host/display*", "lcs/host/lcs_*"]
PATCH = re.compile(r"^tools/patches/lcs-recomp/[^/]+\.patch$")


def _patch_skips(path, read):
    text = read(path)
    if not text:
        return False
    files = re.findall(r"^\+\+\+ b/(\S+)", text, re.M)
    return bool(files) and all(any(fnmatch(f, p) for p in PATCH_SKIP) for f in files)


def needs_synthetic(paths, read):
    for path in paths:
        if PATCH.match(path):
            if not _patch_skips(path, read):
                return True
        elif not any(fnmatch(path, p) for p in SKIP):
            return True
    return False


def main(argv):
    base, head = argv[1], argv[2]
    paths = subprocess.run(["git", "diff", "--name-only", f"{base}...{head}"], capture_output=True, text=True,
                           check=True).stdout.split()

    def read(path):
        result = subprocess.run(["git", "show", f"{head}:{path}"], capture_output=True, text=True)
        return result.stdout if result.returncode == 0 else None

    print("synthetic" if needs_synthetic(paths, read) else "skip")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
