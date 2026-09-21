#pragma once
#include <pybind11/embed.h>

namespace qp::test {

/// The one interpreter every python test in the binary shares. Started on first
/// use and finalized at exit, since the interpreter cannot be restarted cleanly
/// and a second one while the first runs throws.
inline pybind11::scoped_interpreter& python() {
    static pybind11::scoped_interpreter interpreter;
    return interpreter;
}

/// The embedded module holding every C++ type the python tests pass across.
/// Registered once per process, since pybind refuses a type registered twice.
inline constexpr const char* kTestTypesModule = "qp_test_types";

}  // namespace qp::test
