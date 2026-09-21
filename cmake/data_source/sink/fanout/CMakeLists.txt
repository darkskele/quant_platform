# sink/fanout — FanoutSink.
qp_dir(include QP_INC)

add_library(qp_sink_fanout INTERFACE)
target_include_directories(qp_sink_fanout INTERFACE ${QP_INC})
target_link_libraries(qp_sink_fanout INTERFACE qp_sink)
