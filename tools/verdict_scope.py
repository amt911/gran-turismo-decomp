"""Does a PR need the synthetic gate? (tools/verdict.sh; user decision 2026-11-10, docs/agents/pr-verification.md)

The synthetic gate compares decompiled functions with their recompiled originals on generated cases. A PR whose changed paths
cannot reach that comparison (docs, mods' checks, pad routes, the renderer and the mod host) skips it; everything else runs it.
The rule is an allowlist: a path that is not listed counts as needing the gate, so a new kind of file errs on the safe side.
A changed lcs-recomp patch skips only when every file it modifies is renderer/display/menu code (lcs/host/ge_*, display*, lcs_*).

  python -m tools.verdict_scope BASE HEAD   -> prints "synthetic" or "skip"
  python -m tools.verdict_scope BASE HEAD --only-file F
      -> "only N" (and the N addresses in F) when the PR changes nothing but decomp sources (src/game/**.cpp) and
         config/synth_shapes.txt (and config/synth_corpus/ADDR.txt): the synthetic gate then installs only the functions
         those files register (and the addresses whose shape lines or corpus recipes changed). A PR cannot change what another function compares except through shared
         code (headers, host, harness, build, patches), and any of those runs every function as before. User 2026-11-24
         ("y no se pueden acelerar mas las verificaciones"): each scene spent 650-950 s comparing all ~17.5k functions.
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


ONLY_SOURCES = re.compile(r"^src/game/.+\.cpp$")
SHAPES = "config/synth_shapes.txt"
REGISTRATION = re.compile(r"^[A-Z0-9_]+\(0x(0[89][0-9A-Fa-f]{6})", re.M)
SHAPE_LINE = re.compile(r"^0x(0[89][0-9A-Fa-f]{6})\b")
CORPUS = re.compile(r"^config/synth_corpus/(0[89][0-9A-Fa-f]{6})\.txt$")


def only_functions(paths, read_head, read_base):
    """The functions the synthetic gate must install for these changed paths, or None for every registered one.

    Only when every path the gate can see is a decomp source (src/game/**.cpp) or config/synth_shapes.txt: the
    registrations of those sources at HEAD plus the addresses of shape lines HEAD has and BASE has not. A deleted source
    or an empty result also gives None (the safe side)."""
    found = set()
    for path in paths:
        if any(fnmatch(path, p) for p in SKIP):
            continue
        if path == SHAPES:
            head, base = read_head(path) or "", read_base(path) or ""
            old = set(base.splitlines())
            for line in head.splitlines():
                m = SHAPE_LINE.match(line)
                if m and line not in old:
                    found.add(int(m.group(1), 16))
            continue
        corpus = CORPUS.match(path)
        if corpus:  # one function's synthetic input recipes (PR 817)
            found.add(int(corpus.group(1), 16))
            continue
        if not ONLY_SOURCES.match(path):
            return None
        text = read_head(path)
        if text is None:
            return None
        found.update(int(a, 16) for a in REGISTRATION.findall(text))
    return sorted(found) or None


def _changed(base, head):
    return subprocess.run(["git", "diff", "--name-only", f"{base}...{head}"], capture_output=True, text=True,
                          check=True).stdout.split()


def _reader(ref):
    def read(path):
        result = subprocess.run(["git", "show", f"{ref}:{path}"], capture_output=True, text=True)
        return result.stdout if result.returncode == 0 else None
    return read


def main(argv):
    base, head = argv[1], argv[2]
    only_file = argv[argv.index("--only-file") + 1] if "--only-file" in argv else None
    paths = _changed(base, head)
    read = _reader(head)
    if not needs_synthetic(paths, read):
        print("skip")
        return 0
    only = only_functions(paths, read, _reader(base)) if only_file else None
    if only is None:
        print("synthetic")
        return 0
    Path(only_file).write_text("".join(f"0x{a:08X}\n" for a in only))
    print(f"only {len(only)}")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
