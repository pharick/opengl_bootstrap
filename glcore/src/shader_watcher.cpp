#include <glcore/shader_watcher.hpp>

#include <glcore/log.hpp>

#include <GLFW/glfw3.h>

#include <utility>

namespace fs = std::filesystem;

namespace glc {
namespace {

std::string describe(const std::vector<ShaderSource>& sources) {
	std::string names;
	for (const ShaderSource& source : sources) {
		if (!names.empty()) {
			names += " + ";
		}
		names += source.path.filename().string();
	}
	return names;
}

} // namespace

// ---------------------------------------------------------------------------
// ReloadableProgram
// ---------------------------------------------------------------------------

ReloadableProgram::ReloadableProgram(std::vector<ShaderSource> sources)
    : sources_{std::move(sources)}, program_{Program::fromFiles(sources_)} {
	snapshotTimestamps();
}

void ReloadableProgram::snapshotTimestamps() {
	watched_.clear();
	std::error_code ec;
	for (const fs::path& file : program_.dependencies()) {
		watched_.emplace_back(file, fs::last_write_time(file, ec));
	}
}

bool ReloadableProgram::anyFileChanged() const {
	std::error_code ec;
	for (const auto& [file, stamp] : watched_) {
		const fs::file_time_type now = fs::last_write_time(file, ec);
		if (ec) {
			continue; // mid-save: the file may briefly not exist
		}
		if (now != stamp) {
			return true;
		}
	}
	return false;
}

bool ReloadableProgram::reload() {
	auto rebuilt = Program::tryFromFiles(sources_);
	if (!rebuilt) {
		++failures_;
		log::error("shader reload failed ({}), keeping the previous program:\n{}",
		           describe(sources_), rebuilt.error());
		// Re-snapshot anyway, so a broken file is not retried every frame; the
		// next save re-triggers.
		snapshotTimestamps();
		return false;
	}

	program_ = std::move(*rebuilt);
	failures_ = 0;
	snapshotTimestamps();
	log::info("reloaded {}", describe(sources_));
	return true;
}

bool ReloadableProgram::reloadIfChanged() {
	if (!anyFileChanged()) {
		return false;
	}
	return reload();
}

// ---------------------------------------------------------------------------
// ShaderWatcher
// ---------------------------------------------------------------------------

ReloadableProgram& ShaderWatcher::add(std::vector<ShaderSource> sources) {
	programs_.push_back(std::make_unique<ReloadableProgram>(std::move(sources)));
	return *programs_.back();
}

ReloadableProgram& ShaderWatcher::add(const fs::path& vertex, const fs::path& fragment) {
	return add(std::vector<ShaderSource>{
	    ShaderSource{.stage = GL_VERTEX_SHADER, .path = vertex},
	    ShaderSource{.stage = GL_FRAGMENT_SHADER, .path = fragment},
	});
}

void ShaderWatcher::poll() {
	const double now = glfwGetTime();
	if (now - lastPoll_ < static_cast<double>(interval_)) {
		return;
	}
	lastPoll_ = now;

	for (const std::unique_ptr<ReloadableProgram>& program : programs_) {
		if (program->reloadIfChanged()) {
			++reloads_;
		}
	}
}

void ShaderWatcher::reloadAll() {
	for (const std::unique_ptr<ReloadableProgram>& program : programs_) {
		if (program->reload()) {
			++reloads_;
		}
	}
}

} // namespace glc
