# source/parser — the Parser concept: the minimal shape a venue's
# wire-protocol glue satisfies (build_stream_path/parse_message/
# depth_snapshot_url/parse_depth_snapshot). Header-only, depends on
# qp_source_pure (venue_types.hpp's SymbolTable/RestEndpoint/DepthSnapshot)
# only. binance/ (add_subdirectory'd below) is the one concrete venue.
qp_dir(include QP_INC)

add_library(qp_source_parser INTERFACE)
target_include_directories(qp_source_parser INTERFACE ${QP_INC})
target_link_libraries(qp_source_parser INTERFACE qp_source_pure)

add_subdirectory(binance)
