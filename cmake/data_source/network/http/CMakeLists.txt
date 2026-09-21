# network/http — HttpClient.
qp_dir(include QP_INC)
qp_dir(src QP_SRC)

find_package(Boost 1.81 CONFIG REQUIRED COMPONENTS system)
find_package(OpenSSL REQUIRED)
find_package(Threads REQUIRED)

add_library(qp_network_http STATIC ${QP_SRC}/client.cpp)
# Linked into the python module, which is a shared object.
set_target_properties(qp_network_http PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(qp_network_http PUBLIC ${QP_INC})
target_link_libraries(qp_network_http
    PRIVATE Boost::headers Boost::system OpenSSL::SSL OpenSSL::Crypto Threads::Threads
)
