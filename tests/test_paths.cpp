#include <doctest.h>

#include <glcore/paths.hpp>

#include <filesystem>
#include <tuple>

namespace fs = std::filesystem;

TEST_CASE("the project root resolves to a real directory holding the project") {
	const fs::path& root = glc::paths::projectRoot();
	REQUIRE(fs::is_directory(root));
	CHECK(fs::exists(root / "glcore"));
	CHECK(fs::exists(root / "tutorials"));
	CHECK(fs::exists(root / "CMakeLists.txt"));
}

TEST_CASE("asset paths hang off the project root") {
	CHECK(glc::paths::asset("shaders/x.glsl") ==
	      glc::paths::projectRoot() / "assets" / "shaders/x.glsl");
}

TEST_CASE("the executable directory is found and contains this test binary") {
	const fs::path& dir = glc::paths::executableDir();
	REQUIRE(fs::is_directory(dir));
	CHECK(fs::exists(dir / "glcore_tests"));
}

TEST_CASE("require() reports the offending path") {
	CHECK_THROWS_AS(std::ignore = glc::paths::require("/definitely/not/here"), std::runtime_error);
	CHECK_NOTHROW(std::ignore = glc::paths::require(glc::paths::projectRoot()));
}

TEST_CASE("require() returns by value, so a temporary argument does not dangle") {
	// The earlier `const path& require(const path&)` signature returned a
	// reference into this temporary.
	const fs::path result = glc::paths::require(glc::paths::projectRoot() / "CMakeLists.txt");
	CHECK(fs::exists(result));
	CHECK(result.filename() == "CMakeLists.txt");
}
