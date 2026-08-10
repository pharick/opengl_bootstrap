#pragma once

#include <expected>
#include <filesystem>
#include <string>
#include <vector>

// GLSL source loading with `#include` support.
//
// Core GLSL has no #include (ARB_shading_language_include is an extension that
// macOS does not expose), so it is resolved here before the source reaches the
// driver. This matters from Tutorial 9 onward, where lighting functions are
// shared across chapters.
//
//     #include "common/lighting.glsl"   // relative to the including file, then assets/shaders
//     #include <lighting.glsl>          // relative to assets/shaders
//
// Line numbers are preserved with `#line` directives. GLSL's #line takes a
// *source-string number* rather than a filename, so each file is assigned an
// index; `files` maps those indices back to paths. A driver error reported as
// "2(15)" therefore means line 15 of files[2].

namespace glc {

struct GlslSource {
	std::string text;                         ///< fully expanded source
	std::vector<std::filesystem::path> files; ///< index -> path, and the hot-reload watch list
};

/// Loads and expands `path`. Returns an error message on a missing file or an
/// `#include` cycle rather than throwing, so the hot-reloader can report and
/// keep running.
[[nodiscard]] std::expected<GlslSource, std::string>
loadGlslSource(const std::filesystem::path& path);

/// Renders the index -> path legend for decoding driver error messages.
[[nodiscard]] std::string formatSourceLegend(const std::vector<std::filesystem::path>& files);

} // namespace glc
