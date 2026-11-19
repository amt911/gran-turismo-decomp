from pathlib import Path

import pytest

from tools.extra_seeds import (
    effective_sizes,
    Evidence, REASON_NO_EVIDENCE, REASON_SHARED_TAIL, falls_into, references, select, verify, write_seeds,
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


def test_write_seeds(tmp_path: Path):
    write_seeds(tmp_path / "s.txt", [0x20, 0x10], load_base=0x08804000)
    assert (tmp_path / "s.txt").read_text().splitlines()[-2:] == ["0x08804010", "0x08804020"]


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
