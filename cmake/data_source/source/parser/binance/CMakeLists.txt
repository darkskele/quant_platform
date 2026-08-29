# source/parser/binance — Binance's Parser implementation. Currently a
# placeholder (binance_historical.hpp): still carries the old live-WS
# parsing logic verbatim pending its rewrite into an actual historical-data
# (bulk klines/funding-rate) parser — see docs/decisions.md. Header-only
# for now (no .cpp; the extern template declarations it still carries have
# no definitions anywhere, harmless as long as nothing instantiates them,
# which nothing does yet). No test/bench target until the rewrite lands —
# none exists to wire up.
qp_dir(include QP_INC)

add_library(qp_source_parser_binance INTERFACE)
target_include_directories(qp_source_parser_binance INTERFACE ${QP_INC})
target_link_libraries(qp_source_parser_binance INTERFACE qp_source_parser)
