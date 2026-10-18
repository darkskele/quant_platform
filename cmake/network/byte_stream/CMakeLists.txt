# network/byte_stream. The ByteStream concept.
qp_dir(include QP_INC)

add_library(qp_network_byte_stream INTERFACE)
target_include_directories(qp_network_byte_stream INTERFACE ${QP_INC})
target_compile_features(qp_network_byte_stream INTERFACE cxx_std_23)
target_link_libraries(qp_network_byte_stream INTERFACE qp_core)
