#include <gtest/gtest.h>
#include <zip.h>

#include <cstring>
#include <stdexcept>

#include "zip.hpp"

using qp::data_source::archive::zip::unzip_single_entry;

namespace {

// Builds an in-memory single-entry zip, for round-tripping unzip_single_entry.
std::vector<std::byte> make_zip(const char* entry_name, std::string_view content) {
    zip_error_t error;
    zip_error_init(&error);

    zip_source_t* store = zip_source_buffer_create(nullptr, 0, 0, &error);
    zip_source_keep(store);
    zip_t* archive = zip_open_from_source(store, ZIP_TRUNCATE, &error);

    zip_source_t* entry_src = zip_source_buffer(archive, content.data(), content.size(), 0);
    zip_file_add(archive, entry_name, entry_src, ZIP_FL_ENC_UTF_8);
    zip_close(archive);

    zip_source_open(store);
    zip_source_seek(store, 0, SEEK_END);
    auto size = zip_source_tell(store);
    zip_source_seek(store, 0, SEEK_SET);

    std::vector<std::byte> bytes(static_cast<std::size_t>(size));
    zip_source_read(store, bytes.data(), bytes.size());
    zip_source_close(store);
    zip_source_free(store);

    return bytes;
}

}  // namespace

TEST(UnzipSingleEntryTest, RoundTripsContent) {
    auto zip_bytes = make_zip("data.csv", "open,high,low,close\n1,2,3,4\n");
    auto out       = unzip_single_entry(zip_bytes);

    std::string text(reinterpret_cast<const char*>(out.data()), out.size());
    EXPECT_EQ(text, "open,high,low,close\n1,2,3,4\n");
}

TEST(UnzipSingleEntryTest, ThrowsOnGarbageInput) {
    std::vector<std::byte> garbage(16, std::byte{0xAB});
    EXPECT_THROW(unzip_single_entry(garbage), std::runtime_error);
}
