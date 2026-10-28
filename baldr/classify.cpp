#include <baldr/classify.hpp>

#include <libutl/tty.hpp>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <array>
#include <span>
#include <string>

namespace baldr {

namespace {

using namespace std::string_view_literals;

constexpr auto ErrorMarkers = std::to_array({
    ": error:"sv,
    ": fatal error:"sv,
    "error: linker command failed"sv,
    "undefined reference to"sv,
    "multiple definition of"sv,
    "CMake Error"sv,
    ": *** "sv,                 // make: *** [target] Error 1, make[2]: *** ...
    "FAILED:"sv,                // ninja
    "ninja: build stopped"sv,
    "[  FAILED  ]"sv,           // gtest
    "tests failed out of"sv,    // ctest
    "***Failed"sv,              // ctest
    "AddressSanitizer"sv,
    "runtime error:"sv,         // UBSan
});

constexpr auto WarningMarkers = std::to_array({
    ": warning:"sv,
    "CMake Warning"sv,
    "CMake Deprecation Warning"sv,
});

[[nodiscard]] auto contains_any(std::string_view line, std::span<const std::string_view> markers) -> bool {
    return std::ranges::any_of(markers, [&](std::string_view marker) { return line.contains(marker); });
}

} // namespace

auto classify(std::string_view line) -> spdlog::level::level_enum {
    const auto plain = utl::tty::strip_ansi(std::string{ line });

    if (plain.contains("Waiting for unfinished jobs")) {
        return spdlog::level::info;
    }
    if (contains_any(plain, ErrorMarkers)) {
        return spdlog::level::err;
    }
    if (contains_any(plain, WarningMarkers)) {
        return spdlog::level::warn;
    }
    return spdlog::level::info;
}

void log_output(std::string_view line) {
    spdlog::log(classify(line), "{}", line);
}

} // namespace baldr
