# network/transport. Receive times shared by every transport.
qp_dir(include QP_INC)

add_library(qp_network_transport INTERFACE)
target_include_directories(qp_network_transport INTERFACE ${QP_INC})
target_compile_features(qp_network_transport INTERFACE cxx_std_23)
target_link_libraries(qp_network_transport INTERFACE qp_core)

add_subdirectory(byte_stream)
add_subdirectory(datagram)
