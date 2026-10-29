"""tools.frame_audit: a body's GuestFrame size against the `addiu $sp, $sp, -N` of the original."""

from pathlib import Path

from tools import frame_audit
from tools.tests.test_family_members import build_elf, pack

ADDIU_SP = 0x27BD0000
JR_RA, NOP = 0x03E00008, 0


def addiu_sp(delta: int) -> int:
    return ADDIU_SP | (delta & 0xFFFF)


def body(address: int, frame: int | None) -> str:
    inner = f"{{ GuestFrame frame(ctx, 0x{frame:X}u); G[2] = 0u; }}" if frame is not None else "G[2] = 0u;"
    return f"void func_{address:08X}(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) {{ {inner} }}\n"


def test_declared_frames_takes_the_first_frame_of_each_body():
    source = body(0x08804028, 0x20) + body(0x08804100, None) + (
        "void func_08804200(psprecomp::Runtime &rt, psprecomp::AllegrexContext &ctx) "
        "{ { GuestFrame frame(ctx, 0x30u); } { GuestFrame frame(ctx, 0x10u); } }\n")
    assert frame_audit.declared_frames(source) == {0x08804028: 0x20, 0x08804200: 0x30}


def test_asm_frame_finds_the_first_stack_allocation_near_the_entry():
    assert frame_audit.asm_frame([0x3C02004C, addiu_sp(-0x30), JR_RA, addiu_sp(0x30)]) == 0x30
    assert frame_audit.asm_frame([JR_RA, NOP]) is None
    # a release (positive delta) is not an allocation
    assert frame_audit.asm_frame([addiu_sp(0x10), JR_RA]) is None
    # allocations beyond the prologue window belong to another function
    assert frame_audit.asm_frame([NOP] * frame_audit.PROLOGUE_WORDS + [addiu_sp(-0x10)]) is None


def test_mismatches_reports_only_bodies_whose_frame_differs():
    words = {0x08804028: [addiu_sp(-0x10), JR_RA, addiu_sp(0x10)],
             0x08804040: [addiu_sp(-0x20), JR_RA, addiu_sp(0x20)],
             0x08804060: [JR_RA, NOP]}
    declared = {0x08804028: 0x10, 0x08804040: 0x8, 0x08804060: 0x10, 0x08804080: 0x10}
    assert frame_audit.mismatches(declared, words.get) == [(0x08804040, 0x20, 0x8)]


def test_main_reads_the_elf_and_fails_on_a_wrong_frame(tmp_path: Path, capsys):
    text = pack(addiu_sp(-0x10), JR_RA, addiu_sp(0x10), NOP,
                addiu_sp(-0x20), JR_RA, addiu_sp(0x20), NOP)
    elf = tmp_path / "EBOOT.elf"
    elf.write_bytes(build_elf(text, b"", text_addr=0x28))
    tsv = tmp_path / "functions.tsv"
    tsv.write_text("28\t16\n38\t16\n")
    good = tmp_path / "good.cpp"
    good.write_text(body(0x08804028, 0x10) + body(0x08804038, 0x20))
    args = ["--elf", str(elf), "--functions-tsv", str(tsv)]
    assert frame_audit.main([*args, str(good)]) == 0
    assert "0 wrong" in capsys.readouterr().out
    bad = tmp_path / "bad.cpp"
    bad.write_text(body(0x08804038, 0x8))
    assert frame_audit.main([*args, str(bad)]) == 1
    out = capsys.readouterr().out
    assert "func_08804038" in out and "asm 0x20" in out and "body 0x8" in out
