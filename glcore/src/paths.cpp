#include <glcore/paths.hpp>

#include <glcore/log.hpp>

#include <array>
#include <cstdint>
#include <format>
#include <stdexcept>
#include <vector>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#elifdef _WIN32
// std::wstring, for GetModuleFileNameW's result. The pragma stops
// include-cleaner flagging it on platforms where this branch is inactive.
#include <string> // IWYU pragma: keep
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace glc::paths {
namespace {

fs::path findExecutablePath() {
#ifdef __APPLE__
	std::uint32_t size = 0;
	_NSGetExecutablePath(nullptr, &size); // query required length
	std::vector<char> buffer(size + 1, '\0');
	if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
		return {};
	}
	std::error_code ec;
	const fs::path resolved = fs::canonical(fs::path{buffer.data()}, ec);
	return ec ? fs::path{buffer.data()} : resolved;
#elifdef _WIN32
	std::vector<wchar_t> buffer(MAX_PATH);
	const DWORD length =
	    GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
	return length == 0 ? fs::path{} : fs::path{std::wstring{buffer.data(), length}};
#else
	std::error_code ec;
	const fs::path resolved = fs::read_symlink("/proc/self/exe", ec);
	return ec ? fs::path{} : resolved;
#endif
}

/// Walk up from `start` looking for a directory that contains all of `markers`.
fs::path findRootAbove(const fs::path& start) {
	static constexpr std::array<std::string_view, 3> markers{"assets", "glcore", "tutorials"};
	std::error_code ec;
	for (fs::path dir = start; !dir.empty(); dir = dir.parent_path()) {
		bool all = true;
		for (const std::string_view marker : markers) {
			if (!fs::exists(dir / marker, ec)) {
				all = false;
				break;
			}
		}
		if (all) {
			return dir;
		}
		if (dir == dir.parent_path()) {
			break; // reached the filesystem root
		}
	}
	return {};
}

} // namespace

const fs::path& executableDir() {
	static const fs::path dir = [] {
		const fs::path exe = findExecutablePath();
		return exe.empty() ? fs::current_path() : exe.parent_path();
	}();
	return dir;
}

const fs::path& projectRoot() {
	static const fs::path root = [] {
		std::error_code ec;

#ifdef GLC_PROJECT_ROOT
		// Not const: constness would block the implicit move on return.
		fs::path injected{GLC_PROJECT_ROOT};
		if (fs::is_directory(injected, ec)) {
			return injected;
		}
		log::warn("injected project root '{}' no longer exists; searching from the executable",
		          injected.string());
#endif

		if (const fs::path found = findRootAbove(executableDir()); !found.empty()) {
			return found;
		}

		log::warn("could not locate the project root; falling back to '{}'",
		          executableDir().string());
		return executableDir();
	}();
	return root;
}

fs::path asset(std::string_view relative) {
	return projectRoot() / "assets" / relative;
}

fs::path require(fs::path path) {
	std::error_code ec;
	if (!fs::exists(path, ec)) {
		throw std::runtime_error(std::format("file not found: {}", path.string()));
	}
	return path;
}

} // namespace glc::paths
