# source/venue — the Parser and SymbolTable concepts.
qp_dir(include QP_INC)

add_library(qp_source_venue INTERFACE)
target_include_directories(qp_source_venue INTERFACE ${QP_INC})
target_link_libraries(qp_source_venue INTERFACE qp_source_pure)

add_subdirectory(binance)
