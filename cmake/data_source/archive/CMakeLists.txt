# archive — zip decompression.
qp_dir(include QP_INC)
qp_dir(src QP_SRC)

# pkg-config first; some libzip-dev packages ship a CMake export that
# hard-errors on find_package(CONFIG) instead of failing softly.
find_package(PkgConfig)
if(PkgConfig_FOUND)
    pkg_check_modules(libzip REQUIRED IMPORTED_TARGET libzip)
    add_library(libzip::zip ALIAS PkgConfig::libzip)
else()
    find_package(libzip CONFIG REQUIRED)
endif()

add_library(qp_archive_zip STATIC ${QP_SRC}/zip.cpp)
# Linked into the python module, which is a shared object.
set_target_properties(qp_archive_zip PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(qp_archive_zip PUBLIC ${QP_INC})
target_link_libraries(qp_archive_zip PRIVATE libzip::zip)
