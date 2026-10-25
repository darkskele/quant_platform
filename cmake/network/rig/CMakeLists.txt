# network/rig. The network bench rig, a fixed peer and one receiver per backend.
qp_dir(benchmarks QP_RIG)

add_library(qp_network_rig INTERFACE)
target_include_directories(qp_network_rig INTERFACE ${QP_RIG})
target_compile_features(qp_network_rig INTERFACE cxx_std_23)
# The receiver harness reads both concepts.
target_link_libraries(qp_network_rig INTERFACE qp_network_transport_byte_stream qp_network_transport_datagram)

add_executable(netbench_peer ${QP_RIG}/peer.cpp)
target_link_libraries(netbench_peer PRIVATE qp_network_rig)

add_executable(netbench_kernel_tcp ${QP_RIG}/netbench_kernel_tcp.cpp)
target_link_libraries(netbench_kernel_tcp PRIVATE qp_network_rig)

add_executable(netbench_kernel_udp ${QP_RIG}/netbench_kernel_udp.cpp)
target_link_libraries(netbench_kernel_udp PRIVATE qp_network_rig)

# At the top of the build directory, where the sweep looks for them.
set_target_properties(netbench_peer netbench_kernel_tcp netbench_kernel_udp
    PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})

# Builds the peer and every receiver.
add_custom_target(netbench DEPENDS netbench_peer netbench_kernel_tcp netbench_kernel_udp)
