#include <catch2/catch_test_macros.hpp>

#include <glcore/glsl_source.hpp>

#include <filesystem>
#include <fstream>
#include <string>

namespace fs = std::filesystem;

namespace {

/// Creates a uniquely named directory that removes itself at end of scope.
class TempDir {
public:
	explicit TempDir(const std::string& name)
	    : path_{fs::temp_directory_path() / ("glcore_test_" + name)} {
		std::error_code ec;
		fs::remove_all(path_, ec);
		fs::create_directories(path_);
	}

	~TempDir() {
		std::error_code ec;
		fs::remove_all(path_, ec);
	}

	TempDir(const TempDir&) = delete;
	TempDir& operator=(const TempDir&) = delete;
	TempDir(TempDir&&) = delete;
	TempDir& operator=(TempDir&&) = delete;

	/// Deliberately not [[nodiscard]]: the point is the side effect, and helper
	/// files are often written without needing their path back.
	fs::path write(const std::string& name, std::string_view contents) const { // NOLINT
		const fs::path file = path_ / name;
		std::ofstream stream{file, std::ios::binary};
		stream << contents;
		return file;
	}

private:
	fs::path path_;
};

} // namespace

TEST_CASE("plain source keeps #version first") {
	const TempDir dir{"version"};
	const fs::path shader = dir.write("plain.vert", "#version 330\nvoid main() {}\n");

	const auto result = glc::loadGlslSource(shader);
	REQUIRE(result.has_value());
	CHECK(result->text.starts_with("#version 330\n"));
	CHECK(result->files.size() == 1);
	CHECK(result->files[0].filename() == "plain.vert");
}

TEST_CASE("a missing file is an error, not an exception") {
	const auto result = glc::loadGlslSource("/definitely/not/here.vert");
	REQUIRE_FALSE(result.has_value());
	CHECK(result.error().contains("cannot open"));
}

TEST_CASE("#include is expanded and the file is recorded") {
	const TempDir dir{"include"};
	dir.write("helper.glsl", "float helper() { return 1.0; }\n");
	const fs::path shader =
	    dir.write("main.frag", "#version 330\n#include \"helper.glsl\"\nvoid main() {}\n");

	const auto result = glc::loadGlslSource(shader);
	REQUIRE(result.has_value());
	CHECK(result->text.contains("float helper()"));
	CHECK_FALSE(result->text.contains("#include"));

	// Both files are watched for hot-reload.
	REQUIRE(result->files.size() == 2);
	CHECK(result->files[0].filename() == "main.frag");
	CHECK(result->files[1].filename() == "helper.glsl");
}

TEST_CASE("#line directives are emitted so driver errors stay decodable") {
	const TempDir dir{"lines"};
	dir.write("helper.glsl", "// helper\n");
	const fs::path shader =
	    dir.write("main.frag", "#version 330\n#include \"helper.glsl\"\nvoid main() {}\n");

	const auto result = glc::loadGlslSource(shader);
	REQUIRE(result.has_value());
	// Root file is source string 0, the include is source string 1.
	CHECK(result->text.contains("#line 2 0"));
	CHECK(result->text.contains("#line 1 1"));
	// After the include, numbering resumes in the root file at line 3.
	CHECK(result->text.contains("#line 3 0"));
}

TEST_CASE("an #include cycle is reported rather than looping forever") {
	const TempDir dir{"cycle"};
	dir.write("a.glsl", "#include \"b.glsl\"\n");
	dir.write("b.glsl", "#include \"a.glsl\"\n");
	const fs::path shader = dir.write("main.frag", "#version 330\n#include \"a.glsl\"\n");

	const auto result = glc::loadGlslSource(shader);
	REQUIRE_FALSE(result.has_value());
	CHECK(result.error().contains("cycle"));
}

TEST_CASE("an unresolvable #include names what it looked for") {
	const TempDir dir{"missing_include"};
	const fs::path shader = dir.write("main.frag", "#version 330\n#include \"nope.glsl\"\n");

	const auto result = glc::loadGlslSource(shader);
	REQUIRE_FALSE(result.has_value());
	CHECK(result.error().contains("nope.glsl"));
}

TEST_CASE("the same header included twice keeps one source-string index") {
	const TempDir dir{"twice"};
	dir.write("helper.glsl", "// helper\n");
	const fs::path shader = dir.write(
	    "main.frag", "#version 330\n#include \"helper.glsl\"\n#include \"helper.glsl\"\n");

	const auto result = glc::loadGlslSource(shader);
	REQUIRE(result.has_value());
	CHECK(result->files.size() == 2);
}
