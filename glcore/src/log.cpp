#include <glcore/log.hpp>

#include <cstdio>
#include <print>
#include <string_view>

#ifdef _WIN32
#include <io.h>
#define GLC_ISATTY(fd) (_isatty(fd) != 0)
#define GLC_FILENO(f) _fileno(f)
#else
#include <unistd.h>
#define GLC_ISATTY(fd) (isatty(fd) != 0)
#define GLC_FILENO(f) fileno(f)
#endif

namespace glc::log {
namespace {

/// Function-local so it is not a mutable global, and so its initialization is
/// ordered and thread-safe.
Level& currentLevel() noexcept {
	static Level level = Level::Info;
	return level;
}

bool stderrIsTty() {
	static const bool tty = GLC_ISATTY(GLC_FILENO(stderr));
	return tty;
}

struct Style {
	std::string_view label;
	std::string_view color;
};

Style styleFor(Level level) {
	switch (level) {
		case Level::Trace:
			return {.label = "trace", .color = "\033[90m"};
		case Level::Info:
			return {.label = "info ", .color = "\033[36m"};
		case Level::Warn:
			return {.label = "warn ", .color = "\033[33m"};
		case Level::Error:
			return {.label = "error", .color = "\033[31m"};
		case Level::Off:
			break;
	}
	return {.label = "?????", .color = ""};
}

} // namespace

void setLevel(Level level) noexcept {
	currentLevel() = level;
}

Level level() noexcept {
	return currentLevel();
}

void detail::write(Level level, std::string_view message) noexcept {
	const Style style = styleFor(level);
	try {
		if (stderrIsTty()) {
			std::print(stderr, "{}[{}]\033[0m {}\n", style.color, style.label, message);
		} else {
			std::print(stderr, "[{}] {}\n", style.label, message);
		}
	} catch (...) {
		// std::print allocates, so it can throw. Rather than terminate inside
		// whatever destructor was logging, fall back to a write that cannot.
		std::fwrite(message.data(), 1, message.size(), stderr);
		std::fputc('\n', stderr);
	}
}

} // namespace glc::log
