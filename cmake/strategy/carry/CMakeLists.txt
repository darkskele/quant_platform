# carry — funding-rate/basis carry Strategy
qp_dir(include QP_INC)

add_library(qp_strategy_carry INTERFACE)
target_include_directories(qp_strategy_carry INTERFACE ${QP_INC})
target_link_libraries(qp_strategy_carry INTERFACE qp_strategy)

if(QP_BUILD_PYTHON)
    add_subdirectory(python)
endif()
