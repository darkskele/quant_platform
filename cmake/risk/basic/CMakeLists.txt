# basic — BasicRiskGate, the first concrete RiskGate.
qp_dir(include QP_INC)

add_library(qp_risk_basic INTERFACE)
target_include_directories(qp_risk_basic INTERFACE ${QP_INC})
target_link_libraries(qp_risk_basic INTERFACE qp_risk)
