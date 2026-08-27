#pragma once

// Which Sink/recorder implementation this collector binary is built
// against — same one-file-names-the-concrete-type convention as
// collector_venue.hpp, for an orthogonal axis (recorder format, not venue).
// QP_COLLECTOR_RECORDER (CMake option, cmake/apps/collector/CMakeLists.txt)
// selects which branch compiles via a QP_RECORDER_* define; collector.hpp/
// .cpp read only SelectedRecorder and never mention sink::FileRecorder (or
// a future sink::FixRecorder) directly. Adding a recorder is additive: a
// new #elif branch here, nothing in collector.hpp/.cpp changes.

#if defined(QP_RECORDER_FILE)

#include "file_recorder.hpp"

namespace qp::collector {

using SelectedRecorder = sink::FileRecorder;

}  // namespace qp::collector

#else
#error "apps/collector: unknown QP_COLLECTOR_RECORDER (supported: file)"
#endif
