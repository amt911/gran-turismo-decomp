"""Branch coverage of the decompiled sources a gate run compared (docs/agents/decomp-workflow.md).

The differential gate proves only the paths its compared cases take. `tools.decomp_gate coverage TAG --only ...`
builds the sources that register those functions with LLVM source-based coverage (CMake's GT_COVERAGE_FILES), runs
the fast gate, and prints, per function, the branches no compared case took and the code regions that never ran:
those paths must be checked against the asm by hand and listed in the function's comment.

This module holds the pure parts: which files to instrument, and turning `llvm-cov export` JSON into that list.
"""

from __future__ import annotations

import re
from collections.abc import Iterable, Mapping, Sequence
from dataclasses import dataclass, field
from pathlib import Path

_ADDRESS_IN_NAME = re.compile(r"func_([0-9A-Fa-f]{8})")
_FUNCTION_NAME = re.compile(r"((?:\w+::)*\w+)\(")

# llvm-cov export region/branch tuples: [line_start, col_start, line_end, col_end, count, ...].
_REGION_COUNT = 4
_BRANCH_TRUE, _BRANCH_FALSE = 4, 5
_BRANCH_FILE, _REGION_FILE = 6, 5  # index into the function's filenames: 0 its own file, others headers / macro expansions
_REGION_KIND = 7  # 0 = code, 1 = expansion, 2 = skipped, 3 = gap, 4 = branch


@dataclass
class FunctionCoverage:
    name: str
    address: int | None
    file: str
    branches: int = 0
    untaken: list[tuple[int, int, str]] = field(default_factory=list)  # (line, column, "true" | "false")
    unexecuted: list[tuple[int, int]] = field(default_factory=list)    # (line, column) of code regions that never ran
    # Outcomes never taken inside a macro expansion or another file: (file, file ID, line, column, "true" | "false").
    # Every expansion has its own file ID even when two share a position (the macro's definition), so each keeps
    # its own outcomes; merging them by position would hide an expansion's untaken branch behind another's.
    elsewhere: list[tuple[str, int, int, int, str]] = field(default_factory=list)

    @property
    def complete(self) -> bool:
        return not self.untaken and not self.unexecuted and not self.elsewhere


def source_files(only: Iterable[int], src: Path, registration: re.Pattern[str]) -> list[Path]:
    """The files under `src` that register any of the addresses `only` (the sources to instrument)."""
    wanted = set(only)
    found = []
    for path in sorted(src.rglob("*.cpp")):
        text = path.read_text()
        if any(int(m[1], 16) in wanted for m in registration.finditer(text)):
            found.append(path)
    return found


def cmake_files_argument(files: Sequence[Path], root: Path) -> str:
    """The -DGT_COVERAGE_FILES=... value: paths relative to the repository root, separated by `;`."""
    return "-DGT_COVERAGE_FILES=" + ";".join(str(f.resolve().relative_to(root.resolve())) for f in files)


def function_label(raw: str) -> tuple[str, int | None]:
    """A readable name and the address in a name like `_ZN2gt4game...func_08B54760...` (None when it has none)."""
    address = _ADDRESS_IN_NAME.search(raw)
    short = raw
    match = _FUNCTION_NAME.search(raw)
    if match:
        short = match[1]
    return short, int(address[1], 16) if address else None


def demangle_names(export: dict, demangler) -> None:
    """Replaces every function name of the export by its demangled form (`demangler` maps a list of names to a list
    of the same length); a source-file prefix ("file.cpp:_ZN...", used for internal-linkage functions) is dropped."""
    functions = [f for data in export.get("data", []) for f in data.get("functions", [])]
    names = [re.sub(r"^[^:]*\.cpp:", "", f.get("name", "")) for f in functions]
    for function, name in zip(functions, demangler(names), strict=True):
        function["name"] = name


def uncovered(export: Mapping, files: Iterable[str]) -> list[FunctionCoverage]:
    """Per function of `files` in an `llvm-cov export -format=text` document: the branch outcomes no run took and
    the code regions with a zero count. Functions are returned in file order; a function seen in several
    translation units is merged (an outcome is untaken only if every copy left it untaken)."""
    wanted = [str(f) for f in files]

    def in_wanted(name: str) -> bool:
        return any(name == w or name.endswith("/" + w) or w.endswith(name) for w in wanted)

    merged: dict[tuple[str, str], FunctionCoverage] = {}
    seen_outcomes: dict[tuple[str, str], dict[tuple[str, int, int, int, str], bool]] = {}
    seen_regions: dict[tuple[str, str], dict[tuple[int, int], bool]] = {}
    for data in export.get("data", []):
        for function in data.get("functions", []):
            names = function.get("filenames", [])
            file = next((n for n in names if in_wanted(n)), None)
            if file is None:
                continue
            label, address = function_label(function.get("name", ""))
            key = (file, label)
            entry = merged.setdefault(key, FunctionCoverage(label, address, file))
            outcomes = seen_outcomes.setdefault(key, {})
            regions = seen_regions.setdefault(key, {})
            for branch in function.get("branches", []):
                line, col, file_id = branch[0], branch[1], branch[_BRANCH_FILE]
                where = names[file_id] if 0 < file_id < len(names) else ""
                for kind, index in (("true", _BRANCH_TRUE), ("false", _BRANCH_FALSE)):
                    o = (where, file_id if file_id else 0, line, col, kind)
                    outcomes[o] = outcomes.get(o, False) or branch[index] > 0
            for region in function.get("regions", []):
                if region[_REGION_KIND] != 0 or region[_REGION_FILE] != 0:
                    continue
                line, col = region[0], region[1]
                regions[(line, col)] = regions.get((line, col), False) or region[_REGION_COUNT] > 0
    for key, entry in merged.items():
        outcomes = seen_outcomes[key]
        entry.branches = len({o[:4] for o in outcomes})
        entry.untaken = sorted((line, col, kind) for (where, _, line, col, kind), taken in outcomes.items()
                               if not taken and not where)
        entry.elsewhere = sorted(o for o, taken in outcomes.items() if not taken and o[0])
        entry.unexecuted = sorted(r for r, ran in seen_regions[key].items() if not ran)
    return sorted(merged.values(), key=lambda f: (f.file, f.untaken[0][0] if f.untaken else 0, f.name))


def format_report(functions: Sequence[FunctionCoverage]) -> str:
    """One line per function (every branch taken, or the untaken outcomes and never-run regions by line:column)."""
    lines = []
    for f in functions:
        where = f"0x{f.address:08X} " if f.address is not None else ""
        if f.complete:
            lines.append(f"{where}{f.name}: all {f.branches} branches taken, every region ran")
            continue
        parts = []
        if f.untaken:
            parts.append("branches never " + ", ".join(f"{kind} at {line}:{col}" for line, col, kind in f.untaken))
        if f.unexecuted:
            parts.append("regions never run at " + ", ".join(f"{line}:{col}" for line, col in f.unexecuted))
        if f.elsewhere:
            parts.append("in macros/headers never " + ", ".join(
                f"{Path(where).name} (expansion {file_id}) {line}:{col} {kind}" for where, file_id, line, col, kind in f.elsewhere))
        lines.append(f"{where}{f.name}: " + "; ".join(parts))
    return "\n".join(lines)
