#include "zip.hpp"

#include <zip.h>

#include <stdexcept>
#include <string>

namespace qp::data_source::archive::zip {

std::vector<std::byte> unzip_single_entry(std::span<const std::byte> zip_bytes) {
    zip_error_t error;
    zip_error_init(&error);

    zip_source_t* source = zip_source_buffer_create(zip_bytes.data(), zip_bytes.size(), 0, &error);
    if (!source) {
        std::string msg = zip_error_strerror(&error);
        zip_error_fini(&error);
        throw std::runtime_error("zip: " + msg);
    }

    zip_t* archive = zip_open_from_source(source, ZIP_RDONLY, &error);
    if (!archive) {
        std::string msg = zip_error_strerror(&error);
        zip_error_fini(&error);
        zip_source_free(source);
        throw std::runtime_error("zip: " + msg);
    }
    zip_error_fini(&error);

    if (zip_get_num_entries(archive, 0) != 1) {
        zip_discard(archive);
        throw std::runtime_error("zip: expected a single entry archive");
    }

    zip_stat_t stat;
    zip_stat_init(&stat);
    if (zip_stat_index(archive, 0, 0, &stat) != 0 || !(stat.valid & ZIP_STAT_SIZE)) {
        zip_discard(archive);
        throw std::runtime_error("zip: could not stat the entry");
    }

    zip_file_t* entry = zip_fopen_index(archive, 0, 0);
    if (!entry) {
        zip_discard(archive);
        throw std::runtime_error("zip: could not open the entry");
    }

    std::vector<std::byte> out(stat.size);
    zip_int64_t            read = zip_fread(entry, out.data(), out.size());
    zip_fclose(entry);
    zip_discard(archive);

    if (read < 0 || static_cast<zip_uint64_t>(read) != stat.size)
        throw std::runtime_error("zip: short read on entry");

    return out;
}

}  // namespace qp::data_source::archive::zip
