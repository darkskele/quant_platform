# sink — the Sink concept (see docs/DESIGN.md). Header-only, depends on
# core only. FanoutSink, the one concrete Sink implementation, lives under
# fanout/ (add_subdirectory'd below) — FileRecorder (the persistence Sink)
# was removed along with the rest of the live-collector machinery
# (docs/decisions.md); the on-disk wire format itself (encode/decode, zstd
# codec, day/segment naming) still lives in data_source/wire, shared with
# FileReplaySource's read side.
qp_dir(include QP_INC)

add_library(qp_sink INTERFACE)
target_include_directories(qp_sink INTERFACE ${QP_INC})
target_link_libraries(qp_sink INTERFACE qp_core)

add_subdirectory(fanout)
