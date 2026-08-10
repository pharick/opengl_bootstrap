#pragma once

#include <cstdint>
#include <format>
#include <string_view>
#include <utility>

// Minimal leveled logger. Writes to stderr, colorized when stderr is a TTY.
// Replaces scattered `std::cerr <<` calls so diagnostics have a consistent
// shape and can be silenced wholesale in a release build.
//
// Everything here is noexcept on purpose. Logging happens inside destructors
// (~MatrixStack::Frame, ~FrameErrorScope) and inside noexcept move assignment,
// where a propagating exception would be far worse than the failure being
// reported. The trade is that a std::bad_alloc while formatting terminates
// rather than unwinding, which for a logger is the honest outcome.

namespace glc::log {

enum class Level : std::uint8_t { Trace = 0, Info = 1, Warn = 2, Error = 3, Off = 4 };

void setLevel(Level level) noexcept;
[[nodiscard]] Level level() noexcept;

namespace detail {

void write(Level level, std::string_view message) noexcept;

/// std::format allocates and can therefore throw. Catching here is what lets
/// every entry point below be honestly noexcept.
template<class... Args>
void formatAndWrite(Level level, std::format_string<Args...> fmt, Args&&... args) noexcept {
	try {
		write(level, std::format(fmt, std::forward<Args>(args)...));
	} catch (...) {
		write(level, "<log message could not be formatted>");
	}
}

} // namespace detail

template<class... Args>
void trace(std::format_string<Args...> fmt, Args&&... args) noexcept {
	if (level() <= Level::Trace) {
		detail::formatAndWrite(Level::Trace, fmt, std::forward<Args>(args)...);
	}
}

template<class... Args>
void info(std::format_string<Args...> fmt, Args&&... args) noexcept {
	if (level() <= Level::Info) {
		detail::formatAndWrite(Level::Info, fmt, std::forward<Args>(args)...);
	}
}

template<class... Args>
void warn(std::format_string<Args...> fmt, Args&&... args) noexcept {
	if (level() <= Level::Warn) {
		detail::formatAndWrite(Level::Warn, fmt, std::forward<Args>(args)...);
	}
}

template<class... Args>
void error(std::format_string<Args...> fmt, Args&&... args) noexcept {
	if (level() <= Level::Error) {
		detail::formatAndWrite(Level::Error, fmt, std::forward<Args>(args)...);
	}
}

} // namespace glc::log
