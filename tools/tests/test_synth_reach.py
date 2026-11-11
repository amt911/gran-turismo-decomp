from tools import synth_reach as sr

LOG = """\
[decomp] gt::a::f @0x08810000: compared 8, equal 8, mismatched 0, not comparable 0 (calls 8, cases that changed memory 3, nested in 0 equal / 0 mismatched cases)
[decomp] gt::a::g @0x08810100: compared 0, equal 0, mismatched 0, not comparable 8 (calls 8, cases that changed memory 0, nested in 0 equal / 0 mismatched cases)
[decomp] gt::a::h @0x08810200: compared 4, equal 3, mismatched 1, not comparable 4 (calls 8, cases that changed memory 0, nested in 0 equal / 0 mismatched cases)
noise line
"""


def test_parse_reads_every_function_line():
    results = sr.parse_synth_log(LOG)
    assert set(results) == {0x08810000, 0x08810100, 0x08810200}
    assert results[0x08810000] == sr.Result(compared=8, equal=8, mismatched=0, skipped=0, changed_memory=3)
    assert results[0x08810200].mismatched == 1


def test_verified_needs_an_equal_case_and_no_mismatch():
    r = sr.parse_synth_log(LOG)
    assert r[0x08810000].verified and not r[0x08810100].verified and not r[0x08810200].verified


def test_size_bucket_edges():
    assert [sr.bucket(n) for n in (0, 99, 100, 299, 300, 999, 1000)] == ["<100", "<100", "100-300", "100-300", "300-1000", "300-1000", ">=1000"]


def test_summarize_counts_by_bucket_and_changed_memory():
    r = sr.parse_synth_log(LOG)
    sizes = {0x08810000: 50, 0x08810100: 50, 0x08810200: 400}
    rows = sr.summarize(r, sizes, {0x08810000, 0x08810100, 0x08810200})
    assert rows["<100"] == sr.Row(functions=2, verified=1, verified_changed_memory=1)
    assert rows["300-1000"] == sr.Row(functions=1, verified=0, verified_changed_memory=0)


def test_summarize_only_counts_the_selected_functions():
    r = sr.parse_synth_log(LOG)
    rows = sr.summarize(r, {0x08810000: 50}, {0x08810000})
    assert sum(row.functions for row in rows.values()) == 1


def test_reached_from_probe_logs_ignores_zero_hits():
    text = "[probe] 0x08810000 hits=3 first=#1 c_x\n[probe] 0x08810004 hits=0 c_y\n"
    assert sr.reached_from_probe(text) == {0x08810000}


def test_main_prints_a_table(tmp_path, capsys):
    log = tmp_path / "s.log"; log.write_text(LOG)
    funcs = tmp_path / "f.tsv"
    funcs.write_text("%x\t%d\n" % (0x08810000 - 0x08804000, 50) + "%x\t%d\n" % (0x08810200 - 0x08804000, 400))
    assert sr.main(["--log", str(log), "--functions", str(funcs)]) == 0
    out = capsys.readouterr().out
    assert "<100" in out and "300-1000" in out
