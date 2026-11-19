from pathlib import Path

import pytest

from tools.extra_seeds import (
    REASON_BRANCH_TARGET, REASON_INSIDE_EMITTED, REASON_NO_EVIDENCE, REASON_SHARED_TAIL, blocks, body_changes, effective_sizes,
    falls_into, identical_dirs, merge_with_previous, read_seed_file, references, run_select, run_verify, select, verify, write_seeds,
)

JR_RA, NOP = 0x03E00008, 0
PROLOGUE = 0x27BDFFF0           # addiu $sp,$sp,-16
BASE = 0x1000


def jal(target):
    return 0x0C000000 | (target >> 2)


def j(target):
    return 0x08000000 | (target >> 2)


def lay(*functions):
    """Words of consecutive functions (lists of words) and {vaddr: size}."""
    words, funcs, at = [], {}, BASE
    for body in functions:
        funcs[at] = 4 * len(body)
        words += body
        at += 4 * len(body)
    return words, funcs


RET = [JR_RA, NOP]


def test_references_find_calls_jumps_data_words_and_lui_pairs():
    words, funcs = lay([PROLOGUE, jal(BASE + 0x20), JR_RA, NOP, NOP, NOP, NOP, NOP],
                       [PROLOGUE, j(BASE + 0x40), JR_RA, NOP, NOP, NOP, NOP, NOP],
                       [PROLOGUE] + RET, [PROLOGUE] + RET)
    ev = references(words, BASE, elf_words=[BASE + 0x40, 0, 7], functions=funcs)
    assert ev[BASE + 0x20].callers == frozenset({BASE + 4})
    assert ev[BASE + 0x40].callers == frozenset({BASE + 0x24})
    assert ev[BASE + 0x40].data_words == 1
    assert BASE not in ev


def test_references_find_a_lui_addiu_pair():
    target = BASE + 0x1C
    words, funcs = lay([0x3C000000 | (1 << 16) | (target >> 16), 0x24210000 | (target & 0xFFFF), JR_RA, NOP, NOP, NOP, NOP],
                       [PROLOGUE] + RET)
    # $at = lui(target >> 16); addiu $at,$at,low
    words[0] = 0x3C010000 | (target >> 16)
    words[1] = 0x24210000 | (target & 0xFFFF)
    ev = references(words, BASE, elf_words=[], functions=funcs)
    assert ev[target].lui_sites == frozenset({BASE})


def test_falls_into_detects_a_previous_function_without_a_return():
    returns = [PROLOGUE, JR_RA, NOP]
    falls = [PROLOGUE, 0x27BD0010, 0x00000021]
    assert falls_into(returns) is False
    assert falls_into(falls) is True
    assert falls_into(returns + [NOP, NOP]) is False   # trailing alignment nops do not hide the return
    assert falls_into([PROLOGUE, j(BASE), NOP]) is False


def test_select_phase_a_takes_referenced_functions_only():
    words, funcs = lay([PROLOGUE, jal(BASE + 0x20), JR_RA, NOP, NOP, NOP, NOP, NOP],
                       [PROLOGUE] + RET + [NOP] * 5,          # 0x20: called
                       [PROLOGUE] + RET + [NOP] * 5)          # 0x40: nothing refers to it
    seeds, rejected = select(words, BASE, funcs, emitted={BASE}, elf_words=[], phase="A")
    assert seeds == [BASE + 0x20]
    assert rejected == {}


def test_select_rejects_a_function_the_previous_one_falls_into():
    words, funcs = lay([PROLOGUE, jal(BASE + 0x20), JR_RA, NOP],
                       [PROLOGUE, 0x27BD0010, 0x24020001, 0x00000021],      # no return: runs into the next
                       [PROLOGUE] + RET)                                     # wrong boundary: called, but entered by fall-through
    seeds, rejected = select(words, BASE, funcs, emitted={BASE}, elf_words=[], phase="A")
    assert seeds == []
    assert rejected == {BASE + 0x20: REASON_SHARED_TAIL}


def test_select_phase_b_needs_a_prologue_and_no_branch_into_the_middle():
    words, funcs = lay([PROLOGUE] + RET + [NOP] * 5,                       # emitted
                       [PROLOGUE, 0x24020001] + RET + [NOP] * 4,           # 0x20 dead, prologue: accepted
                       [0x24020001] + RET + [NOP] * 5,                     # 0x40 dead, no prologue: rejected
                       [PROLOGUE, 0x10000003, NOP, NOP] + RET + [NOP] * 2) # 0x60 dead, prologue, entered by the branch below
    # a branch in the first function lands in the middle of the last one (offset 0x6C)
    words[2] = 0x10000000 | (((BASE + 0x6C) - (BASE + 8) - 4) >> 2 & 0xFFFF)
    seeds, rejected = select(words, BASE, funcs, emitted={BASE}, elf_words=[], phase="B")
    assert seeds == [BASE + 0x20]
    assert rejected[BASE + 0x60] == "entered-by-branch"
    assert rejected[BASE + 0x40] == REASON_NO_EVIDENCE


def test_write_seeds_and_read_seed_file_round_trip_elf_vaddrs(tmp_path: Path):
    write_seeds(tmp_path / "c" / "s.txt", [0x20, 0x10, 0x20])
    assert (tmp_path / "c" / "s.txt").read_text().splitlines()[-2:] == ["0x00000010", "0x00000020"]
    assert read_seed_file(tmp_path / "c" / "s.txt") == [0x10, 0x20]


def test_read_seed_file_errors(tmp_path: Path):
    with pytest.raises(FileNotFoundError):
        read_seed_file(tmp_path / "none")
    f = tmp_path / "bad.txt"
    f.write_text("0x10\nnope\n")
    with pytest.raises(ValueError, match=r"bad.txt:2"):
        read_seed_file(f)


def test_merge_never_shrinks_the_list():
    assert merge_with_previous([3, 1], [2, 3]) == [1, 2, 3]


def generated(tmp_path: Path, name: str, addresses, labels=()):
    d = tmp_path / name
    d.mkdir()
    body = "".join(f"runtime.register_function(0x{a:08X}u, f);\n" for a in addresses)
    body += "".join(f"L_{a:08X}:;\n" for a in labels)
    (d / "generated_unit_0000.cpp").write_text(body)
    return d


def tsv(tmp_path: Path, name: str, text: str):
    p = tmp_path / name
    p.write_text(text)
    return p


FUNCS = {0x100: 0x40, 0x140: 0x40}


def test_verify_passes_for_a_superset(tmp_path: Path):
    before = generated(tmp_path, "b", [0x100], labels=[0x108])
    after = generated(tmp_path, "a", [0x100, 0x140], labels=[0x108, 0x14C])
    t = tsv(tmp_path, "f.tsv", "100\t64\n")
    assert verify(before, after, t, t, seeds=[0x140], functions=FUNCS) == []


def test_verify_fails_when_a_registered_address_disappears(tmp_path: Path):
    before = generated(tmp_path, "b", [0x100, 0x120])
    after = generated(tmp_path, "a", [0x100])
    t = tsv(tmp_path, "f.tsv", "x\n")
    errors = verify(before, after, t, t, seeds=[], functions=FUNCS)
    assert any("0x00000120" in e and "disappeared" in e for e in errors)


def test_verify_fails_when_a_block_label_disappears(tmp_path: Path):
    before = generated(tmp_path, "b", [0x100], labels=[0x108])
    after = generated(tmp_path, "a", [0x100], labels=[])
    t = tsv(tmp_path, "f.tsv", "x\n")
    assert any("block" in e for e in verify(before, after, t, t, seeds=[], functions=FUNCS))


def test_verify_fails_when_the_function_list_changes(tmp_path: Path):
    before = generated(tmp_path, "b", [0x100])
    after = generated(tmp_path, "a", [0x100])
    errors = verify(before, after, tsv(tmp_path, "1", "a\n"), tsv(tmp_path, "2", "b\n"), seeds=[], functions=FUNCS)
    assert any("functions.tsv" in e for e in errors)


def test_sabotage_a_seed_inside_an_existing_function_is_a_split(tmp_path: Path):
    before = generated(tmp_path, "b", [0x100])
    after = generated(tmp_path, "a", [0x100, 0x120])
    t = tsv(tmp_path, "f.tsv", "x\n")
    errors = verify(before, after, t, t, seeds=[0x120], functions=FUNCS)
    assert any("splits" in e and "0x00000120" in e for e in errors)


def test_select_rejects_an_unknown_phase():
    with pytest.raises(ValueError):
        select([], BASE, {}, set(), [], "C")


def test_effective_sizes_cut_an_overlapping_thunk_at_its_jump():
    words, funcs = lay([j(BASE + 0x40), NOP, PROLOGUE] + RET + [NOP] * 3)
    assert funcs == {BASE: 32}
    assert effective_sizes(words, BASE, funcs) == {BASE: 8}
    assert effective_sizes(words, BASE, {BASE: 8}) == {BASE: 8}


def test_effective_sizes_keep_code_after_a_return_that_a_branch_jumps_over():
    skip = 0x10000000 | 3  # b +3 words
    words, funcs = lay([skip, NOP, JR_RA, NOP, JR_RA, NOP, NOP, NOP])
    assert effective_sizes(words, BASE, funcs)[BASE] == 24


def test_select_rejects_a_seed_inside_an_emitted_function_even_in_phase_a():
    # the first function is emitted and 0x30 bytes long by Ghidra, but its code only runs to 0x08; the called one at 0x20 is outside
    words, funcs = lay([PROLOGUE, jal(BASE + 0x10), JR_RA, NOP, PROLOGUE] + RET + [NOP] * 3 + RET)
    funcs = {BASE: 0x20, BASE + 0x10: 0x10}   # Ghidra says the first function covers 0x10 too (an overlap)
    seeds, rejected = select(words, BASE, funcs, emitted={BASE}, elf_words=[], phase="A")
    assert seeds == [BASE + 0x10]            # code-derived extent of the first function is 0x10: not inside
    funcs2 = {BASE: 0x20, BASE + 8: 0x10}
    words2 = [PROLOGUE, jal(BASE + 8), NOP, NOP, JR_RA, NOP, NOP, NOP, PROLOGUE] + RET + [NOP] * 5
    seeds, rejected = select(words2, BASE, funcs2, emitted={BASE}, elf_words=[], phase="A")
    assert seeds == []
    assert rejected == {BASE + 8: REASON_INSIDE_EMITTED}


def test_select_rejects_the_target_of_a_branch_of_an_emitted_function():
    branch = 0x10000000 | 0x0002  # b +2 words from the next instruction: target BASE + 4 + 4 + 8 = BASE + 0x10
    words, funcs = lay([PROLOGUE, jal(BASE + 0x40), branch, NOP, NOP, NOP, JR_RA, NOP] + [NOP] * 0,
                       [PROLOGUE] + RET + [NOP] * 5)
    # the second function starts at BASE + 0x20; make the branch land exactly there
    offset = ((BASE + 0x20) - (BASE + 8 + 4)) >> 2
    words[2] = 0x10000000 | offset
    words[1] = jal(BASE + 0x20)
    seeds, rejected = select(words, BASE, funcs, emitted={BASE}, elf_words=[], phase="A")
    assert BASE + 0x20 not in seeds
    assert rejected[BASE + 0x20] == REASON_BRANCH_TARGET


def test_references_stop_a_lui_scan_where_the_register_is_overwritten():
    target = BASE + 0x40
    lui = 0x3C010000 | (target >> 16)
    clobber = 0x24010005                     # addiu $at,$zero,5: writes $at
    use = 0x24210000 | (target & 0xFFFF)     # addiu $at,$at,low: would complete the address
    words, funcs = lay([lui, clobber, use, JR_RA, NOP, NOP, NOP, NOP], [PROLOGUE] * 2 + RET + [NOP] * 4,
                       [PROLOGUE] + RET + [NOP] * 5, [PROLOGUE] + RET + [NOP] * 5)
    assert target not in references(words, BASE, [], funcs)
    words[1] = NOP
    assert references(words, BASE, [], funcs)[target].lui_sites == frozenset({BASE})


def test_a_self_reference_is_not_evidence():
    words, funcs = lay([PROLOGUE, jal(BASE), JR_RA, NOP])    # a function that calls itself
    assert BASE not in references(words, BASE, [], funcs)


def block_dir(tmp_path: Path, name: str, text: str) -> Path:
    d = tmp_path / name
    d.mkdir()
    (d / "generated_unit_0000.cpp").write_text(text)
    return d


UNIT = """    case 1u: goto L_00000100;
    1, 0, 0,
L_00000100:
    ctx.gpr[2] = 1u;
    if (rt.invoke_chained_direct<&recomp_unit_0001_entry, 1u, 17u, 0x00000200u>(ctx, &aot_mem)) goto L_00000110;
L_00000110:
    ctx.gpr[3] = 2u;
    return;
}
"""


def test_blocks_ignore_tables_case_lines_and_chained_ids(tmp_path: Path):
    a = blocks(block_dir(tmp_path, "a", UNIT))
    b = blocks(block_dir(tmp_path, "b", UNIT.replace("17u", "99u").replace("case 1u", "case 7u")))
    assert a == b and set(a) == {0x100, 0x110}


def test_body_changes_flags_the_same_labels_with_a_different_body(tmp_path: Path):
    before = blocks(block_dir(tmp_path, "a", UNIT))
    after = blocks(block_dir(tmp_path, "b", UNIT.replace("ctx.gpr[3] = 2u;", "ctx.gpr[3] = 3u;")))
    assert body_changes(before, after) == [0x110]


def test_body_changes_accepts_a_block_split_by_a_new_label(tmp_path: Path):
    whole = UNIT.replace("    return;\n", "    ctx.gpr[9] = 0u;\n    return;\n")
    split = UNIT.replace("    return;\n", "    goto L_00000118;\nL_00000118:\n    ctx.gpr[9] = 0u;\n    return;\n")
    before = blocks(block_dir(tmp_path, "a", whole))
    assert body_changes(before, blocks(block_dir(tmp_path, "b", split))) == []
    # the same split but the new label's code differs: the baseline block is not reproduced
    bad = split.replace("ctx.gpr[9] = 0u;", "ctx.gpr[9] = 7u;")
    assert body_changes(before, blocks(block_dir(tmp_path, "c", bad))) == [0x110]


def test_verify_fails_when_a_block_changes_its_code(tmp_path: Path):
    before = block_dir(tmp_path, "a", UNIT + "runtime.register_function(0x00000100u, f);\n")
    after = block_dir(tmp_path, "b", UNIT.replace("ctx.gpr[2] = 1u;", "ctx.gpr[2] = 9u;") + "runtime.register_function(0x00000100u, f);\n")
    t = tsv(tmp_path, "f.tsv", "x\n")
    assert any("changed its code" in e for e in verify(before, after, t, t, seeds=[], functions={}))


def test_identical_dirs(tmp_path: Path):
    a, b = tmp_path / "a", tmp_path / "b"
    a.mkdir(); b.mkdir()
    (a / "x.cpp").write_text("1"); (b / "x.cpp").write_text("1")
    (a / ".inputs.sha1").write_text("a"); (b / ".inputs.sha1").write_text("b")
    (b / "extra_seeds.txt").write_text("0x1\n")
    assert identical_dirs(a, b) == []
    (b / "x.cpp").write_text("2"); (b / "y.cpp").write_text("")
    assert identical_dirs(a, b) == ["only in %s: y.cpp" % b, "x.cpp differs"]


def fake_loader(words, base=BASE):
    return lambda _elf: (words, base, [])


def test_run_select_is_idempotent_and_never_shrinks_the_list(tmp_path: Path):
    words, funcs = lay([PROLOGUE, jal(BASE + 0x20), JR_RA, NOP, NOP, NOP, NOP, NOP],
                       [PROLOGUE] + RET + [NOP] * 5)
    t = tsv(tmp_path, "f.tsv", "".join(f"{a:x}\t{s}\n" for a, s in funcs.items()))
    baseline = generated(tmp_path, "base", [BASE + 0x8804000])
    out = tmp_path / "config" / "seeds.txt"
    first, _ = run_select(Path("x"), t, baseline, "A", out, loader=fake_loader(words))
    assert first == [BASE + 0x20]
    again, _ = run_select(Path("x"), t, baseline, "A", out, loader=fake_loader(words))
    assert again == first
    # a baseline that already emits the seed (a regeneration WITH seeds) would select nothing: the list must not shrink
    seeded = generated(tmp_path, "seeded", [BASE + 0x8804000, BASE + 0x20 + 0x8804000])
    kept, _ = run_select(Path("x"), t, seeded, "A", out, loader=fake_loader(words))
    assert kept == first


def test_run_verify_reads_seeds_as_elf_vaddrs_and_flags_a_split(tmp_path: Path):
    words, funcs = lay([PROLOGUE, 0x24020001, 0x24020002, JR_RA, NOP, NOP, NOP, NOP])
    t = tsv(tmp_path, "f.tsv", f"{BASE:x}\t32\n")
    before = generated(tmp_path, "b", [BASE + 0x8804000])
    after = generated(tmp_path, "a", [BASE + 0x8804000])
    seeds = tmp_path / "seeds.txt"
    seeds.write_text(f"{BASE + 4:#x}\n")
    errors = run_verify(before, after, seeds, t, t, Path("x"), loader=fake_loader(words))
    assert any("splits" in e for e in errors)
    seeds.write_text("# none\n")
    assert run_verify(before, after, seeds, t, t, Path("x"), loader=fake_loader(words)) == []


def test_writes_covers_the_instruction_kinds():
    from tools.extra_seeds import _writes

    assert _writes(0x03E00008, 31) is False            # jr $ra writes no register
    assert _writes(0x0C000000, 31) is True             # jal writes $ra
    assert _writes(0x00221021, 2) is True              # addu $v0,$at,$v0: rd = 2
    assert _writes(0x8C220000, 2) is True              # lw $v0
    assert _writes(0xAC220000, 2) is False             # sw does not write


def test_main_dispatches_select_verify_and_noop(tmp_path: Path, monkeypatch, capsys):
    import tools.extra_seeds as m

    monkeypatch.setattr(m, "run_select", lambda *a, **k: ([BASE], {BASE + 4: "shared-tail"}))
    assert m.main(["select", "--phase", "A", "--baseline", str(tmp_path), "--out", str(tmp_path / "s.txt")]) == 0
    assert "rejected 1" in capsys.readouterr().out
    monkeypatch.setattr(m, "run_verify", lambda *a, **k: ["block 0x1 disappeared"])
    assert m.main(["verify", "--before", str(tmp_path), "--tsv-before", str(tmp_path / "t")]) == 1
    assert "FAIL: block" in capsys.readouterr().out
    monkeypatch.setattr(m, "run_verify", lambda *a, **k: [])
    assert m.main(["verify", "--before", str(tmp_path), "--tsv-before", str(tmp_path / "t")]) == 0
    monkeypatch.setattr(m, "run_noop", lambda seed: 7)
    assert m.main(["noop", "--seed", "08805000"]) == 7
