# Path-derivation helpers (D50): every module's CMakeLists.txt now lives
# under cmake/, mirroring the content-type roots (src/include/tests/
# benchmarks) instead of sitting next to its own sources. CMAKE_SOURCE_DIR
# is fixed (repo root) regardless of nesting depth, so a plain string swap
# on the current file's own directory gives every module its sibling paths
# without a hand-written relative path anywhere.
function(qp_dir kind out_var)
    string(REPLACE "/cmake/" "/${kind}/" _result "${CMAKE_CURRENT_SOURCE_DIR}")
    set(${out_var} "${_result}" PARENT_SCOPE)
endfunction()
