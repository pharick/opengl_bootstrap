#include <glcore/glsl_source.hpp>

#include <glcore/paths.hpp>

#include <algorithm>
#include <format>
#include <fstream>
#include <optional>
#include <sstream>
#include <string_view>

namespace fs = std::filesystem;

namespace glc {
namespace {

constexpr std::string_view kWhitespace = " \t\r\n";

std::string_view trim(std::string_view text) {
	const std::size_t begin = text.find_first_not_of(kWhitespace);
	if (begin == std::string_view::npos) {
		return {};
	}
	const std::size_t end = text.find_last_not_of(kWhitespace);
	return text.substr(begin, end - begin + 1);
}

std::expected<std::string, std::string> readFile(const fs::path& path) {
	const std::ifstream stream{path, std::ios::binary};
	if (!stream) {
		return std::unexpected(std::format("cannot open '{}'", path.string()));
	}
	std::ostringstream buffer;
	buffer << stream.rdbuf();
	return buffer.str();
}

/// Extracts the target of an `#include` line, or nullopt if this is not one.
/// Returns {target, angled}.
struct IncludeDirective {
	std::string target;
	bool angled = false;
};

std::optional<IncludeDirective> parseInclude(std::string_view line) {
	const std::string_view trimmed = trim(line);
	if (!trimmed.starts_with("#")) {
		return std::nullopt;
	}
	std::string_view rest = trim(trimmed.substr(1));
	if (!rest.starts_with("include")) {
		return std::nullopt;
	}
	rest = trim(rest.substr(std::string_view{"include"}.size()));
	if (rest.size() < 2) {
		return std::nullopt;
	}

	const char open = rest.front();
	char close = '\0';
	if (open == '<') {
		close = '>';
	} else if (open == '"') {
		close = '"';
	} else {
		return std::nullopt;
	}

	const std::size_t end = rest.find(close, 1);
	if (end == std::string_view::npos) {
		return std::nullopt;
	}
	return IncludeDirective{.target = std::string{rest.substr(1, end - 1)}, .angled = open == '<'};
}

/// Resolves an include target: next to the including file first (quoted form
/// only), then under assets/shaders.
std::expected<fs::path, std::string> resolveInclude(const IncludeDirective& directive,
                                                    const fs::path& includingFile) {
	std::error_code ec;
	if (!directive.angled) {
		const fs::path sibling = includingFile.parent_path() / directive.target;
		if (fs::exists(sibling, ec)) {
			return fs::weakly_canonical(sibling, ec);
		}
	}
	const fs::path shared = paths::asset("shaders") / directive.target;
	if (fs::exists(shared, ec)) {
		return fs::weakly_canonical(shared, ec);
	}
	return std::unexpected(
	    std::format("cannot resolve #include \"{}\" from '{}' (looked next to it and in '{}')",
	                directive.target, includingFile.string(), paths::asset("shaders").string()));
}

class Expander {
public:
	std::expected<GlslSource, std::string> run(const fs::path& root) {
		std::error_code ec;
		const fs::path canonicalRoot = fs::weakly_canonical(root, ec);
		const fs::path& start = ec ? root : canonicalRoot;

		auto contents = readFile(start);
		if (!contents) {
			return std::unexpected(contents.error());
		}

		// #version must be the first thing the driver sees, so hoist it out
		// before any #line directive is emitted.
		std::size_t firstBodyLine = 1;
		std::istringstream probe{*contents};
		std::string line;
		std::size_t lineNumber = 0;
		while (std::getline(probe, line)) {
			++lineNumber;
			const std::string_view trimmed = trim(line);
			if (trimmed.empty() || trimmed.starts_with("//")) {
				continue;
			}
			if (trimmed.starts_with("#version")) {
				out_ += line;
				out_ += '\n';
				firstBodyLine = lineNumber + 1;
			}
			break;
		}

		if (auto result = expand(start, *contents, firstBodyLine); !result) {
			return std::unexpected(result.error());
		}
		return GlslSource{.text = std::move(out_), .files = std::move(files_)};
	}

private:
	std::size_t indexOf(const fs::path& path) {
		for (std::size_t i = 0; i < files_.size(); ++i) {
			if (files_[i] == path) {
				return i;
			}
		}
		files_.push_back(path);
		return files_.size() - 1;
	}

	std::expected<void, std::string> expand(const fs::path& path, const std::string& contents,
	                                        std::size_t startLine) {
		if (std::ranges::find(stack_, path) != stack_.end()) {
			std::string chain;
			for (const fs::path& entry : stack_) {
				chain += entry.filename().string();
				chain += " -> ";
			}
			chain += path.filename().string();
			return std::unexpected(std::format("#include cycle: {}", chain));
		}
		stack_.push_back(path);

		const std::size_t index = indexOf(path);
		out_ += std::format("#line {} {}\n", startLine, index);

		std::istringstream stream{contents};
		std::string line;
		std::size_t lineNumber = 0;
		while (std::getline(stream, line)) {
			++lineNumber;
			if (lineNumber < startLine) {
				continue;
			}

			const std::optional<IncludeDirective> directive = parseInclude(line);
			if (!directive) {
				out_ += line;
				out_ += '\n';
				continue;
			}

			auto resolved = resolveInclude(*directive, path);
			if (!resolved) {
				return std::unexpected(resolved.error());
			}
			auto included = readFile(*resolved);
			if (!included) {
				return std::unexpected(included.error());
			}
			if (auto result = expand(*resolved, *included, 1); !result) {
				return result;
			}
			// Resume numbering in the including file after the #include line.
			out_ += std::format("#line {} {}\n", lineNumber + 1, index);
		}

		stack_.pop_back();
		return {};
	}

	std::string out_;
	std::vector<fs::path> files_;
	std::vector<fs::path> stack_;
};

} // namespace

std::expected<GlslSource, std::string> loadGlslSource(const fs::path& path) {
	return Expander{}.run(path);
}

std::string formatSourceLegend(const std::vector<fs::path>& files) {
	std::string legend;
	for (std::size_t i = 0; i < files.size(); ++i) {
		legend += std::format("\n    [{}] {}", i, files[i].string());
	}
	return legend;
}

} // namespace glc
