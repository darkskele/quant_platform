# python — the Binance historical source's bindings.
qp_dir(include QP_INC)
qp_dir(src QP_SRC)

add_library(qp_binance_historical_bindings STATIC ${QP_SRC}/binance_historical_bindings.cpp)
# Linked into the shared python modules, and pybind's types are hidden.
set_target_properties(qp_binance_historical_bindings PROPERTIES POSITION_INDEPENDENT_CODE ON CXX_VISIBILITY_PRESET hidden)
target_include_directories(qp_binance_historical_bindings PUBLIC ${QP_INC})
target_link_libraries(qp_binance_historical_bindings PUBLIC qp_binance_historical qp_network_http pybind11::pybind11)
