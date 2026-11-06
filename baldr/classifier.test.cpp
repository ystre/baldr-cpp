#include <baldr/classifier.hpp>

#include <libnova/dyn_value.hpp>
#include <libnova/error.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <string_view>
#include <tuple>

namespace {

constexpr auto err = spdlog::level::err;
constexpr auto warn = spdlog::level::warn;
constexpr auto info = spdlog::level::info;

[[nodiscard]] auto rules(std::string_view text) -> baldr::rules_t {
    return baldr::parse_rules(nova::dyn_value::parse(text, nova::dyn_value::format_e::toml).value());
}

TEST(Classifier, IgnoresAnsi) {
    const auto parsed = baldr::rules_t{ { .level = err, .markers = { ": error:" } } };
    EXPECT_EQ(baldr::classify(parsed, "\x1b[01ma.cpp:4:2:\x1b[m\x1b[K \x1b[01;31m\x1b[Kerror: \x1b[m\x1b[Kbad"), err);
}

TEST(Classifier, NoMatchMeansInfo) {
    const auto parsed = baldr::rules_t{ { .level = err, .markers = { "marker" } } };
    EXPECT_EQ(baldr::classify(parsed, "something else"), info);
    EXPECT_EQ(baldr::classify(parsed, ""), info);
}

TEST(Classifier, NoRulesMeansInfo) {
    EXPECT_EQ(baldr::classify({ }, "a.cpp:1:1: error: bad"), info);
}

TEST(Classifier, FirstMatchWins) {
    const auto parsed = rules(R"toml(
        [classifier]
        use = ["second", "first"]

        [[classifier.sets.first]]
        level = "error"
        contains = ["both", "only first"]

        [[classifier.sets.second]]
        level = "warning"
        contains = ["both"]

        [[classifier.sets.second]]
        level = "debug"
        contains = ["both", "only second"]
    )toml");

    EXPECT_EQ(baldr::classify(parsed, "both"), warn);
    EXPECT_EQ(baldr::classify(parsed, "only first"), err);
    EXPECT_EQ(baldr::classify(parsed, "only second"), spdlog::level::debug);
}

TEST(Classifier, OnlySelectedSetsApply) {
    const auto parsed = rules(R"toml(
        [classifier]
        use = ["selected"]

        [[classifier.sets.selected]]
        level = "error"
        contains = ["selected"]

        [[classifier.sets.other]]
        level = "error"
        contains = ["other"]
    )toml");

    EXPECT_EQ(baldr::classify(parsed, "selected"), err);
    EXPECT_EQ(baldr::classify(parsed, "other"), info);
}

TEST(Classifier, Negative_UnknownSet) {
    EXPECT_THAT(
        [] { std::ignore = rules("[classifier]\nuse = ['missing']\n"); },
        testing::ThrowsMessage<nova::exception>(testing::HasSubstr("`classifier.use`: unknown set `missing`"))
    );
}

TEST(Classifier, Negative_UnknownLevel) {
    EXPECT_THAT(
        [] {
            std::ignore = rules("[classifier]\nuse = ['a']\n[[classifier.sets.a]]\nlevel = 'loud'\ncontains = ['x']\n");
        },
        testing::ThrowsMessage<nova::exception>(testing::HasSubstr("`classifier.sets.a.0.level`: unknown level `loud`"))
    );
}

TEST(Classifier, Negative_OffIsNoLevelOfALine) {
    EXPECT_THROW(
        std::ignore = rules("[classifier]\nuse = ['a']\n[[classifier.sets.a]]\nlevel = 'off'\ncontains = ['x']\n"),
        nova::exception
    );
}

TEST(Classifier, Negative_MissingContains) {
    EXPECT_THROW(std::ignore = rules("[classifier]\nuse = ['a']\n[[classifier.sets.a]]\nlevel = 'info'\n"), nova::exception);
}

} // namespace
