#pragma once

#include <glcore/program.hpp>

#include <filesystem>
#include <memory>
#include <utility>
#include <vector>

// Shader hot-reload.
//
// Save a .vert/.frag/.glsl and the program rebuilds on the next frame. If the
// new source fails to compile or link, the error is logged and the previously
// working program stays bound -- a typo never costs you the running scene.
//
// The watch list is the full `#include` closure reported by the loader, not
// just the entry files, so editing a shared header reloads everything using it.

namespace glc {

class ReloadableProgram {
public:
	/// Throws if the *initial* build fails: at startup there is no good program
	/// to fall back to.
	explicit ReloadableProgram(std::vector<ShaderSource> sources);

	[[nodiscard]] const Program& get() const noexcept {
		return program_;
	}
	const Program* operator->() const noexcept {
		return &program_;
	}
	const Program& operator*() const noexcept {
		return program_;
	}

	/// Rebuilds if any watched file changed. Returns true only on a successful
	/// reload. Never throws.
	bool reloadIfChanged();

	/// Rebuilds unconditionally. Returns true on success.
	bool reload();

	/// Number of failed reload attempts since the last success.
	[[nodiscard]] int failureCount() const noexcept {
		return failures_;
	}

private:
	using Stamp = std::pair<std::filesystem::path, std::filesystem::file_time_type>;

	void snapshotTimestamps();
	[[nodiscard]] bool anyFileChanged() const;

	std::vector<ShaderSource> sources_;
	Program program_;
	std::vector<Stamp> watched_;
	int failures_ = 0;
};

class ShaderWatcher {
public:
	/// Programs are heap-allocated so the returned reference stays valid as more
	/// are added.
	ReloadableProgram& add(std::vector<ShaderSource> sources);
	ReloadableProgram& add(const std::filesystem::path& vertex,
	                       const std::filesystem::path& fragment);

	/// Checks for changes, at most once per interval. Called once per frame by
	/// App, so the filesystem is not stat-ed at frame rate.
	void poll();

	/// Rebuilds everything regardless of timestamps.
	void reloadAll();

	void setInterval(float seconds) noexcept {
		interval_ = seconds;
	}

	[[nodiscard]] std::size_t size() const noexcept {
		return programs_.size();
	}

	/// Number of successful reloads since startup, for a status readout.
	[[nodiscard]] int reloadCount() const noexcept {
		return reloads_;
	}

private:
	std::vector<std::unique_ptr<ReloadableProgram>> programs_;
	double lastPoll_ = 0.0;
	float interval_ = 0.25F;
	int reloads_ = 0;
};

} // namespace glc
