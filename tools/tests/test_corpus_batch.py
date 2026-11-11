from tools import corpus_batch as cb

LINE = "[decomp] gt::a::f @0x{addr:08X}: compared {c}, equal {e}, mismatched {m}, not comparable {s} (calls 8, cases that changed memory {w}, nested in 0 equal / 0 mismatched cases)\n"


def log(*rows):
    return "".join(LINE.format(addr=a, c=e + m, e=e, m=m, s=0, w=w) for a, e, m, w in rows)


def test_unverified_is_what_no_log_verified():
    one = log((0x08810000, 3, 0, 0), (0x08810004, 0, 0, 0), (0x08810008, 2, 1, 0))
    two = log((0x08810000, 0, 0, 0), (0x08810004, 4, 0, 0), (0x0881000C, 0, 0, 0))
    assert cb.unverified([one, two]) == [0x08810008, 0x0881000C]


def test_a_mismatch_in_any_log_keeps_a_function_unverified():
    assert cb.unverified([log((0x08810000, 5, 0, 0)), log((0x08810000, 1, 1, 0))]) == [0x08810000]


def test_group_by_file_keeps_only_the_wanted_functions():
    reg = {"src/a.cpp": [0x1, 0x2], "src/b.cpp": [0x3]}
    assert cb.group_by_file([0x2, 0x3, 0x9], reg) == {"src/a.cpp": [0x2], "src/b.cpp": [0x3]}


def test_batches_start_with_the_required_files_and_respect_the_limits():
    by_file = {"src/a.cpp": list(range(100)), "src/b.cpp": list(range(100, 150)), "src/c.cpp": list(range(150, 160)),
               "src/d.cpp": list(range(160, 170))}
    batches = cb.make_batches(by_file, must=["src/d.cpp"], max_functions=120, max_files=30)
    assert batches[0][0] == "src/d.cpp"
    assert all(sum(len(by_file[f]) for f in b) <= 120 or len(b) == 1 for b in batches)
    assert sorted(f for b in batches for f in b) == sorted(by_file)


def test_batches_split_at_the_file_limit():
    by_file = {f"src/{i}.cpp": [i] for i in range(7)}
    assert [len(b) for b in cb.make_batches(by_file, max_functions=1000, max_files=3)] == [3, 3, 1]


def test_summary_counts_verified_before_and_after():
    before = log((0x08810000, 0, 0, 0), (0x08810004, 1, 0, 0))
    after = log((0x08810000, 2, 0, 1), (0x08810004, 1, 0, 0), (0x08810008, 0, 2, 0))
    s = cb.summary(before, after)
    assert (s.functions, s.before, s.after, s.mismatched) == (3, 1, 2, [0x08810008])


def test_sabotage_pick_is_a_newly_verified_function_that_changed_memory():
    before = log((0x08810000, 0, 0, 0), (0x08810004, 0, 0, 0), (0x08810008, 1, 0, 0))
    after = log((0x08810000, 2, 0, 0), (0x08810004, 2, 0, 3), (0x08810008, 1, 0, 1))
    assert cb.pick_sabotage(before, after, {0x08810004: "src/x.cpp"}) == (0x08810004, "src/x.cpp")
    assert cb.pick_sabotage(before, after, {}) is None
    assert cb.pick_sabotage(after, after, {0x08810004: "src/x.cpp"}) is None


def test_main_summary_prints_the_numbers(tmp_path, capsys):
    b, a = tmp_path / "b.log", tmp_path / "a.log"
    b.write_text(log((0x08810000, 0, 0, 0)))
    a.write_text(log((0x08810000, 2, 0, 1)))
    assert cb.main(["summary", str(b), str(a)]) == 0
    assert "before 0 after 1" in capsys.readouterr().out


def test_main_plan_writes_batch_files(tmp_path):
    verified = tmp_path / "v.log"
    verified.write_text(log((0x08810000, 0, 0, 0), (0x08810004, 0, 0, 0)))
    src = tmp_path / "src"
    src.mkdir()
    (src / "a.cpp").write_text("GT_DECOMP(0x08810000u, gt::f, gt::ReturnKind::Void);\nGT_DECOMP(0x08810004u, gt::g, gt::ReturnKind::Void);\n")
    out = tmp_path / "out"
    assert cb.main(["plan", "--src", str(src), "--out", str(out), "--first", "1", str(verified)]) == 0
    assert (out / "batch1.txt").read_text() == "0x08810000,0x08810004"
