# integration — cross-lib integration/parity tests that can't live inside
# any single lib's own tests/ (see docs/DESIGN.md here): they compose two
# libs that deliberately never depend on each other. Empty for now:
# test_recorder_replay_parity.cpp (FileRecorder-writes vs FileReplaySource-
# reads) was removed along with FileRecorder itself (docs/decisions.md) —
# a natural successor is a parity test between whatever writes the
# historical-data converter's output and FileReplaySource's read side,
# once that converter exists.
