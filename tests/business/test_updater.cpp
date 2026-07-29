#include <gtest/gtest.h>

#include "business/updater.h"

using namespace bili::business;

TEST(UpdaterTest, ParseVersion)
{
    const QList<int> v1 = parseVersion(QStringLiteral("1.2.3"));
    ASSERT_EQ(v1.size(), 3);
    EXPECT_EQ(v1[0], 1);
    EXPECT_EQ(v1[1], 2);
    EXPECT_EQ(v1[2], 3);

    const QList<int> v2 = parseVersion(QStringLiteral("v2.0"));
    ASSERT_EQ(v2.size(), 3);
    EXPECT_EQ(v2[0], 2);
    EXPECT_EQ(v2[1], 0);
    EXPECT_EQ(v2[2], 0);

    const QList<int> v3 = parseVersion(QStringLiteral("0.5"));
    ASSERT_EQ(v3.size(), 3);
    EXPECT_EQ(v3[0], 0);
    EXPECT_EQ(v3[1], 5);
    EXPECT_EQ(v3[2], 0);
}

TEST(UpdaterTest, IsNewer)
{
    EXPECT_TRUE(isNewer(QStringLiteral("1.0.0"), QStringLiteral("1.0.1")));
    EXPECT_TRUE(isNewer(QStringLiteral("1.0.0"), QStringLiteral("2.0.0")));
    EXPECT_TRUE(isNewer(QStringLiteral("1.0"), QStringLiteral("1.0.1")));
    EXPECT_FALSE(isNewer(QStringLiteral("1.0.1"), QStringLiteral("1.0.0")));
    EXPECT_FALSE(isNewer(QStringLiteral("1.0.0"), QStringLiteral("1.0.0")));
    EXPECT_FALSE(isNewer(QStringLiteral("2.0.0"), QStringLiteral("1.9.9")));
}
