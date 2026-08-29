# source/venue — the Parser and SymbolTable concepts (venue.hpp): the
# minimal shape any venue's wire-protocol glue and symbol universe
# satisfy. Header-only, depends on qp_source_pure (venue_types.hpp's
# SymbolTable/RestEndpoint/DepthSnapshot) only. binance/ (add_subdirectory'd
# below) is the one concrete venue.
qp_dir(include QP_INC)

add_library(qp_source_venue INTERFACE)
target_include_directories(qp_source_venue INTERFACE ${QP_INC})
target_link_libraries(qp_source_venue INTERFACE qp_source_pure)

add_subdirectory(binance)
