from pathlib import Path

from tools.adhoc_symbols import load_natives, module_name, symbol_lines

TSV = (
    "NATIVE\t00382fb0\t00239394\tMLocale\tgetLanguage\t23961c\n"
    "NATIVE\t00382fb0\t00239394\tMLocale\tsetLanguage\t239588\n"
    "NATIVE\t00383108\t000032f8\tImpl,BoardTAImpl,STStructure\tinsert\t2c44\n"
    "NATIVE\t00383108\t000032f8\tImpl,BoardTAImpl,STStructure\tinsert\t28b0\n"
    "NATIVE\t00382fb0\t0029cdac\tHADHOC,psp,SYSTEM\tErrorAbort\t29d7f8\n"
    "NATIVE\t00382fb0\t00111111\tMSystemBGM\tplay\t23961c\n"
)


def test_module_name_prefers_an_M_class_name():
    assert module_name(["HADHOC", "psp", "MSystemBGM", "MOther"], 0x29CDAC) == "MSystemBGM"


def test_module_name_falls_back_to_the_initializer_address():
    assert module_name(["Impl", "STStructure"], 0x32F8) == "adhoc_init_0032F8"


def test_load_natives_parses_every_column(tmp_path: Path):
    path = tmp_path / "natives.tsv"
    path.write_text(TSV)
    natives = load_natives(path)
    assert natives[0] == (0x382FB0, 0x239394, ["MLocale"], "getLanguage", 0x23961C)
    assert len(natives) == 6


def test_symbol_lines_name_each_address_once_with_runtime_addresses(tmp_path: Path):
    path = tmp_path / "natives.tsv"
    path.write_text(TSV)
    lines = symbol_lines(load_natives(path))
    text = "\n".join(lines)
    assert "MLocale::getLanguage = 0x08A3D61C;" in text
    assert "MLocale::setLanguage = 0x08A3D588;" in text
    # same function registered twice: first name wins, no second line for 0x08A3D61C
    assert text.count("0x08A3D61C") == 1
    # two different functions under one name in one module: disambiguated by address
    assert "adhoc_init_0032F8::insert_08806C44 = 0x08806C44;" in text
    assert "adhoc_init_0032F8::insert_088068B0 = 0x088068B0;" in text
    assert all("// adhoc native" in line for line in lines if "=" in line)


def test_symbol_lines_are_sorted_by_address(tmp_path: Path):
    path = tmp_path / "natives.tsv"
    path.write_text(TSV)
    addresses = [int(line.split("= ")[1].split(";")[0], 16) for line in symbol_lines(load_natives(path)) if "=" in line]
    assert addresses == sorted(addresses)
