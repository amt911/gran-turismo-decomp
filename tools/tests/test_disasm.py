"""tools.disasm: resolved branch targets, relocated address pairs, call targets and the command line."""

import struct
from pathlib import Path

from tools import disasm
from tools.family_members import R_MIPS_26, R_MIPS_HI16, R_MIPS_LO16
from tools.progress import LOAD_BASE
from tools.tests.test_family_members import build_elf, pack


def test_resolve_branch_counts_from_the_delay_slot():
    assert disasm.resolve_branch("beqz $v0, . + 4 + (0xD << 2)", 0x08B54FBC) == "beqz $v0, -> 08B54FF4"
    assert disasm.resolve_branch("beq $a0, $v0, . + 4 + (0x4 << 2)", 0x08B55024) == "beq $a0, $v0, -> 08B55038"


def test_resolve_branch_handles_backward_branches_and_leaves_other_text_alone():
    assert disasm.resolve_branch("bnez $a1, . + 4 + (-0x7 << 2)", 0x08C437C4) == "bnez $a1, -> 08C437AC"
    assert disasm.resolve_branch("bnez $a1, . + 4 + (0xFFF9 << 2)", 0x08C437C4) == "bnez $a1, -> 08C437AC"
    assert disasm.resolve_branch("jr $ra", 0x100) == "jr $ra"


def test_disassemble_names_relocated_addresses_and_call_targets():
    lui = 0x3C020000 | 0x4D       # lui $v0, 0x4D
    lw = 0x8C440000 | 0x0300      # lw $a0, 0x300($v0)
    jal = 0x0C000000 | ((0x4000 + 0x10) >> 2)
    words = [lui, lw, jal]
    relocs = {0: R_MIPS_HI16 | (1 << 16), 1: R_MIPS_LO16, 2: R_MIPS_26}
    lines = disasm.disassemble(words, relocs, [0, 0x1000], 0x08804000)
    assert "[HI]" in lines[0]
    assert "[LO] = " in lines[1]
    assert lines[2].endswith("-> func_" + f"{0x4010 + LOAD_BASE:08X}")
    # a low half with no high half before it stays unresolved
    assert disasm.disassemble([lw], {0: R_MIPS_LO16}, [0], 0x08804000)[0].endswith("[LO]")


def test_main_prints_a_function_and_a_free_range(tmp_path: Path, capsys):
    text = pack(0x03E00008, 0x00000000, 0x10400003, 0x00000000)
    elf = tmp_path / "x.elf"
    elf.write_bytes(build_elf(text, struct.pack("<II", 0, 0)))
    tsv = tmp_path / "functions.tsv"
    tsv.write_text("28\t8\n")
    assert disasm.main([f"{0x28 + LOAD_BASE:08X}", f"{0x30 + LOAD_BASE:08X}:2", "--elf", str(elf), "--functions-tsv", str(tsv)]) == 0
    out = capsys.readouterr().out
    assert "== func_" in out and "jr" in out and "-> " in out


def test_main_refuses_an_unknown_function(tmp_path: Path, capsys):
    elf = tmp_path / "x.elf"
    elf.write_bytes(build_elf(pack(0), struct.pack("<II", 0, 0)))
    tsv = tmp_path / "functions.tsv"
    tsv.write_text("28\t4\n")
    assert disasm.main([f"{0x100 + LOAD_BASE:08X}", "--elf", str(elf), "--functions-tsv", str(tsv)]) == 1
    assert "not a known function" in capsys.readouterr().err
