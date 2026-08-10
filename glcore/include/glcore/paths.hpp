#pragma once

#include <filesystem>
#include <string_view>

// Locating data files without depending on the working directory.
//
// The project root is injected at compile time (GLC_PROJECT_ROOT) so a binary
// started from anywhere still finds its shaders -- and, crucially, so the shader
// hot-reloader watches the *source* file you are editing rather than a copy
// staged into build/. If the injected root no longer exists (a relocated or
// installed build), we fall back to walking up from the executable.

namespace glc::paths {

/// Directory containing the running executable.
[[nodiscard]] const std::filesystem::path& executableDir();

/// Repository root: GLC_PROJECT_ROOT if it still exists, else a marker search
/// upwards from executableDir(), else executableDir() itself.
[[nodiscard]] const std::filesystem::path& projectRoot();

/// projectRoot()/"assets"/relative
[[nodiscard]] std::filesystem::path asset(std::string_view relative);

/// Throws std::runtime_error with a helpful message if `path` does not exist,
/// otherwise passes it through.
///
/// Returns by value on purpose: a `const path&` return bound to a `const path&`
/// parameter dangles the moment anyone calls it with a temporary, which
/// `require(paths::asset("x"))` does.
[[nodiscard]] std::filesystem::path require(std::filesystem::path path);

#ifdef GLC_TUTORIAL_DIR
// Defined only in tutorial translation units -- add_tutorial() injects
// GLC_TUTORIAL_DIR per executable, so these live in the header where the macro
// is visible rather than inside the glcore library.

/// Source directory of the tutorial being built.
[[nodiscard]] inline std::filesystem::path tutorialDir() {
	return std::filesystem::path{GLC_TUTORIAL_DIR};
}

/// tutorialDir()/relative
[[nodiscard]] inline std::filesystem::path tutorialFile(std::string_view relative) {
	return tutorialDir() / relative;
}

/// tutorialDir()/"shaders"/relative
[[nodiscard]] inline std::filesystem::path tutorialShader(std::string_view relative) {
	return tutorialDir() / "shaders" / relative;
}
#endif

} // namespace glc::paths
