#pragma once
#include <cstddef>
#include <span>
#include <vector>

namespace qp::data_source::archive::zip {

/// Decompresses the single entry of a zip archive. Throws if the archive
/// does not contain exactly one entry.
std::vector<std::byte> unzip_single_entry(std::span<const std::byte> zip_bytes);

}  // namespace qp::data_source::archive::zip
