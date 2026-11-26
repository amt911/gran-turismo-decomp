# Drafter attribution corrections

Draft batches are written by a delegate model and verified by the main session's scripts (see `AGENTS.md`). Until
2026-11-25 the draft task and the verification scripts hard-coded the trailer `Co-Authored-By: DeepSeek V4.1 Flash via
opencode`, so batches drafted by Claude models carried it too. The scripts now take the drafter from the session log.
History on `main` is not rewritten; this table records the real drafter of the batches that landed with the wrong
trailer. The open PRs with such batches (#784, #832, #891, #892, #893) got an empty commit with the right trailer.

| Batch | Batch PR | Functions | Drafted by | Landed through |
| --- | --- | --- | --- | --- |
| 54 | #770 | 16 | Claude Sonnet 5.5 | #771 |
| 55 | #772 | 28 | Claude Sonnet 5.5 | #776 → #817 (re-landed in #872) |
| 57 | #774 | 22 | Claude Sonnet 5.5 | #776 → #817 (re-landed in #872) |
| 58 | #777 | 16 | Claude Sonnet 5.5 | #786 → #825 |
| 59 | #778 | 7 | Claude Sonnet 5.5 | #786 → #825 |
| 61 | #779 | 5 | Claude Sonnet 5.5 | #786 → #825 |
| 62 | #780 | 9 | Claude Sonnet 5.5 | #786 → #825 |
| 63 | #781 | 2 | Claude Sonnet 5.5 | #786 → #825 |
| 64 | #782 | 11 | Claude Sonnet 5.5 | #786 → #825 |
| 65 | #783 | 1 | Claude Sonnet 5.5 | #786 → #825 |
| 111 | #790 | 24 | Claude Sonnet 5.5 | #817 (re-landed in #872) |
| 112 | #793 | 17 | Claude Sonnet 5.5 | #817 (re-landed in #872) |
| 115 | #800 | 1 | Claude Sonnet 5.5 | #827 → #830 |
| 116 | #798 | 1 | Claude Sonnet 5.5 | #817 (re-landed in #872) |
| 120 | #803 | 24 | Claude Sonnet 5.5 | #827 → #830 |
| 123 | #785 | 11 | Claude Sonnet 5.5 | #786 → #825 |
| 124 | #773 | 10 | Claude Sonnet 5.5 | #776 → #817 (re-landed in #872) |
| 127 | #792 | 8 | Claude Sonnet 5.5 | #827 → #830 |
| 131 | #808 | 7 | Claude Sonnet 5.5 | #827 → #830 |
| 132 | #794 | 5 | Claude Sonnet 5.5 | #817 (re-landed in #872) |
| 137 | #796 | 5 | Claude Sonnet 5.5 | #827 → #830 |
| 142 | #802 | 1 | Claude Sonnet 5.5 | #817 (re-landed in #872) |
| 143 | #801 | 6 | Claude Sonnet 5.5 | #817 (re-landed in #872) |
| 146 | #805 | 1 | Claude Sonnet 5.5 | #817 (re-landed in #872) |
| 147 | #807 | 1 | Claude Sonnet 5.5 | #817 (re-landed in #872) |
| 148 | #804 | 1 | Claude Sonnet 5.5 | #817 (re-landed in #872) |
