# Path-derivation helpers.
function(qp_dir kind out_var)
    string(REPLACE "/cmake/" "/${kind}/" _result "${CMAKE_CURRENT_SOURCE_DIR}")
    set(${out_var} "${_result}" PARENT_SCOPE)
endfunction()
