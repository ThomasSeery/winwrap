#include "winwrap/fs.hpp"

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>

namespace {

std::filesystem::path make_temp_file() {
    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "winwrap_fs_test.txt";
    std::filesystem::remove(path);
    std::ofstream out{path};
    out << "probe\n";
    return path;
}

}  // namespace

TEST_CASE("file_attributes reports a file's bits and surfaces the sentinel as an error") {
    const std::filesystem::path path = make_temp_file();

    const auto attributes = winwrap::file_attributes(path);
    REQUIRE(attributes.has_value());
    CHECK((*attributes & FILE_ATTRIBUTE_DIRECTORY) == 0);

    const auto missing =
        winwrap::file_attributes(std::filesystem::temp_directory_path() / "winwrap_no_such_file");
    REQUIRE_FALSE(missing.has_value());
    CHECK(missing.error().value() == ERROR_FILE_NOT_FOUND);

    std::filesystem::remove(path);
}

TEST_CASE("add_file_attributes turns bits on without dropping the existing ones") {
    const std::filesystem::path path = make_temp_file();
    REQUIRE(winwrap::set_file_attributes(path, FILE_ATTRIBUTE_ARCHIVE).has_value());

    REQUIRE(winwrap::add_file_attributes(path, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)
                .has_value());

    const auto attributes = winwrap::file_attributes(path);
    REQUIRE(attributes.has_value());
    CHECK((*attributes & FILE_ATTRIBUTE_HIDDEN) != 0);
    CHECK((*attributes & FILE_ATTRIBUTE_SYSTEM) != 0);
    CHECK((*attributes & FILE_ATTRIBUTE_ARCHIVE) != 0);

    REQUIRE(winwrap::set_file_attributes(path, FILE_ATTRIBUTE_NORMAL).has_value());
    std::filesystem::remove(path);
}

TEST_CASE("remove_file_attributes turns bits off and leaves the rest alone") {
    const std::filesystem::path path = make_temp_file();
    REQUIRE(winwrap::set_file_attributes(
                path, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_READONLY)
                .has_value());

    REQUIRE(winwrap::remove_file_attributes(path, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)
                .has_value());

    const auto attributes = winwrap::file_attributes(path);
    REQUIRE(attributes.has_value());
    CHECK((*attributes & FILE_ATTRIBUTE_HIDDEN) == 0);
    CHECK((*attributes & FILE_ATTRIBUTE_SYSTEM) == 0);
    CHECK((*attributes & FILE_ATTRIBUTE_READONLY) != 0);

    REQUIRE(winwrap::set_file_attributes(path, FILE_ATTRIBUTE_NORMAL).has_value());
    std::filesystem::remove(path);
}

TEST_CASE("remove_file_attributes clearing every bit leaves FILE_ATTRIBUTE_NORMAL") {
    const std::filesystem::path path = make_temp_file();
    REQUIRE(winwrap::set_file_attributes(path, FILE_ATTRIBUTE_HIDDEN).has_value());

    REQUIRE(winwrap::remove_file_attributes(path, FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_ARCHIVE)
                .has_value());

    const auto attributes = winwrap::file_attributes(path);
    REQUIRE(attributes.has_value());
    CHECK(*attributes == FILE_ATTRIBUTE_NORMAL);

    std::filesystem::remove(path);
}
