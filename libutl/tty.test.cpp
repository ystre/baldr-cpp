#include <libutl/tty.hpp>

#include <gtest/gtest.h>

#include <string>

TEST(Tty, VisibleWidth_PlainText) {
    EXPECT_EQ(utl::tty::visible_width("hello"), 5);
}

TEST(Tty, VisibleWidth_StripsSingleAnsiSequence) {
    EXPECT_EQ(utl::tty::visible_width("\x1b[1;34mhello"), 5);
}

TEST(Tty, VisibleWidth_StripsMultipleAnsiSequences) {
    EXPECT_EQ(utl::tty::visible_width("\x1b[31mred\x1b[0m"), 3);
}

TEST(Tty, VisibleWidth_EmptyString) {
    EXPECT_EQ(utl::tty::visible_width(""), 0);
}

TEST(Tty, CapVisibleWidth_UnderBudgetReturnsUnchanged) {
    EXPECT_EQ(utl::tty::cap_visible_width("short", 10), "short");
}

TEST(Tty, CapVisibleWidth_TruncatesWithEllipsis) {
    std::string capped = utl::tty::cap_visible_width("abcdefghij", 5);
    EXPECT_EQ(utl::tty::visible_width(capped), 5);
    EXPECT_EQ(capped.substr(capped.size() - 3), "...");
}

TEST(Tty, CapVisibleWidth_PreservesTrailingAnsiReset) {
    std::string line = "\x1b[31m" + std::string(20, 'x') + "\x1b[0m";
    std::string capped = utl::tty::cap_visible_width(line, 10);
    EXPECT_EQ(capped.substr(capped.size() - 4), "\x1b[0m");
}

TEST(Tty, CapVisibleWidth_ExactBudgetNotTruncated) {
    std::string line = "abcde";
    EXPECT_EQ(utl::tty::cap_visible_width(line, 5), line);
}

TEST(Tty, TerminalWidth_FallsBackWhenNotATty) {
    EXPECT_GT(utl::tty::terminal_width(), 0);
}
