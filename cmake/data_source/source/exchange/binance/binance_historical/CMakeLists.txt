# binance_historical — Binance Vision historical source.
qp_dir(include QP_INC)
qp_dir(src QP_SRC)

find_package(Threads REQUIRED)

add_library(qp_binance_historical STATIC
    ${QP_SRC}/listing.cpp
    ${QP_SRC}/http_fetch_pool.cpp
)
# Linked into the python module, which is a shared object.
set_target_properties(qp_binance_historical PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(qp_binance_historical PUBLIC ${QP_INC} ${QP_INC}/parsers)
target_link_libraries(qp_binance_historical
    PUBLIC qp_source_pure Threads::Threads
    PRIVATE qp_network_http qp_archive_zip
)

if(QP_BUILD_PYTHON)
    add_subdirectory(python)
endif()
