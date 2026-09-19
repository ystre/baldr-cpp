// #include <gtest/gtest.h>

// #include <baldr/exec.hpp>

// namespace {

// TEST(exec, RunStreamedDefaultsChildToPlainLogMode) {
    // // Guards against the double-logging regression: `run_streamed()` must
    // // default a spawned child to `SPDLOG_MODE=plain` so a nested
    // // `nxs::rlog` user (e.g. a nested `baldr` invocation) doesn't attach
    // // its own timestamp/name/level prefix on top of this function's own
    // // per-line re-logging.
    // const auto status = baldr::run_streamed({ "/bin/sh", "-c", "test \"$SPDLOG_MODE\" = plain" }, ".");
    // EXPECT_TRUE(status.success());
// }

// TEST(exec, RunStreamedRespectsCallerSuppliedSpdlogMode) {
    // const auto status = baldr::run_streamed(
        // { "/bin/sh", "-c", "test \"$SPDLOG_MODE\" = rlog" },
        // ".",
        // { { "SPDLOG_MODE", "rlog" } }
    // );
    // EXPECT_TRUE(status.success());
// }

// } // namespace
