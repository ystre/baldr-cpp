/**
 * Part of Utility Library
 *
 * @brief   Tests of `env.hpp`.
 *
 * @author      Gábor Krisztián Girhiny
 * @created     2026-10-28
 * @updated     2026-10-28
 * @reviewed    read
 * SPDX-License-Identifier: BSL-1.0
 */

#include <libutl/env.hpp>

#include <gtest/gtest.h>

TEST(Env, EnvName_JoinsPrefixAndKey) {
    EXPECT_EQ(utl::env_name("baldr", "LOG_VIEW"), "BALDR_LOG_VIEW");
}

TEST(Env, EnvName_UppercasesAndSanitizesBothParts) {
    EXPECT_EQ(utl::env_name("my-app", "log.file"), "MY_APP_LOG_FILE");
}
