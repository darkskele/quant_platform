# data_source's own shared header, not a city.
qp_dir(include QP_INC)

add_library(qp_data_source INTERFACE)
target_include_directories(qp_data_source INTERFACE ${QP_INC})
target_link_libraries(qp_data_source INTERFACE
    qp_source_pure qp_sink qp_network_http qp_archive_zip qp_binance_historical
)
