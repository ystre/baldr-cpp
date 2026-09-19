#include <gtest/gtest.h>
#include <libutl/line_reader.hpp>

#include <string>
#include <vector>

TEST(LineReader, SingleCompleteLine) {
    std::vector<std::string> received;
    utl::line_reader reader([&received](std::string& line) { received.push_back(line); });

    reader.feed("hello\n");

    ASSERT_EQ(received.size(), 1);
    EXPECT_EQ(received[0], "hello");
}

TEST(LineReader, MultipleLinesInOneChunk) {
    std::vector<std::string> received;
    utl::line_reader reader([&received](std::string& line) { received.push_back(line); });

    reader.feed("a\nb\nc\n");

    ASSERT_EQ(received.size(), 3);
    EXPECT_EQ(received[0], "a");
    EXPECT_EQ(received[1], "b");
    EXPECT_EQ(received[2], "c");
}

TEST(LineReader, LineSplitAcrossTwoFeeds) {
    std::vector<std::string> received;
    utl::line_reader reader([&received](std::string& line) { received.push_back(line); });

    reader.feed("hel");
    reader.feed("lo\n");

    ASSERT_EQ(received.size(), 1);
    EXPECT_EQ(received[0], "hello");
}

TEST(LineReader, NoNewlineYieldsNoDispatch) {
    std::vector<std::string> received;
    utl::line_reader reader([&received](std::string& line) { received.push_back(line); });

    reader.feed("partial");

    EXPECT_TRUE(received.empty());
}

TEST(LineReader, FeedEof_FlushesTrailingPartialLine) {
    std::vector<std::string> received;
    utl::line_reader reader([&received](std::string& line) { received.push_back(line); });

    reader.feed("partial");
    reader.feed_eof();

    ASSERT_EQ(received.size(), 1);
    EXPECT_EQ(received[0], "partial");
}

TEST(LineReader, FeedEof_IsNoOpWhenBufferEmpty) {
    std::vector<std::string> received;
    utl::line_reader reader([&received](std::string& line) { received.push_back(line); });

    reader.feed("line\n");
    reader.feed_eof();

    ASSERT_EQ(received.size(), 1);
    EXPECT_EQ(received[0], "line");
}

TEST(LineReader, FeedEof_IsNoOpOnSecondCall) {
    std::vector<std::string> received;
    utl::line_reader reader([&received](std::string& line) { received.push_back(line); });

    reader.feed("partial");
    reader.feed_eof();
    reader.feed_eof();

    ASSERT_EQ(received.size(), 1);
    EXPECT_EQ(received[0], "partial");
}

TEST(LineReader, EmptyLinesArePreserved) {
    std::vector<std::string> received;
    utl::line_reader reader([&received](std::string& line) { received.push_back(line); });

    reader.feed("a\n\nb\n");

    ASSERT_EQ(received.size(), 3);
    EXPECT_EQ(received[0], "a");
    EXPECT_EQ(received[1], "");
    EXPECT_EQ(received[2], "b");
}
