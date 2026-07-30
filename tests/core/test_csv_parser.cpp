#include <gtest/gtest.h>

#include "core/csv_parser.h"

using namespace bili;

TEST(CsvParser, ParseSimpleFields)
{
    const CsvParser parser;
    const QStringList result = parser.parseLine(QStringLiteral("a,b,c"));
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], QStringLiteral("a"));
    EXPECT_EQ(result[1], QStringLiteral("b"));
    EXPECT_EQ(result[2], QStringLiteral("c"));
}

TEST(CsvParser, ParseEmptyFields)
{
    const CsvParser parser;
    const QStringList result = parser.parseLine(QStringLiteral("a,,c"));
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], QStringLiteral("a"));
    EXPECT_TRUE(result[1].isEmpty());
    EXPECT_EQ(result[2], QStringLiteral("c"));
}

TEST(CsvParser, ParseQuotedFieldWithDelimiter)
{
    const CsvParser parser;
    const QStringList result = parser.parseLine(QStringLiteral("a,\"b,c\",d"));
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], QStringLiteral("a"));
    EXPECT_EQ(result[1], QStringLiteral("b,c"));
    EXPECT_EQ(result[2], QStringLiteral("d"));
}

TEST(CsvParser, ParseEscapedQuotes)
{
    const CsvParser parser;
    const QStringList result = parser.parseLine(QStringLiteral("\"a\"\"b\""));
    EXPECT_EQ(result.size(), 1);
    EXPECT_EQ(result[0], QStringLiteral("a\"b"));
}

TEST(CsvParser, ParseMixedQuotedAndUnquoted)
{
    const CsvParser parser;
    const QStringList result = parser.parseLine(QStringLiteral("\"hello\",world,\"foo,bar\""));
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[0], QStringLiteral("hello"));
    EXPECT_EQ(result[1], QStringLiteral("world"));
    EXPECT_EQ(result[2], QStringLiteral("foo,bar"));
}

TEST(CsvParser, JoinSimpleFields)
{
    const QStringList fields{QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")};
    EXPECT_EQ(CsvParser::joinFields(fields), QStringLiteral("a,b,c"));
}

TEST(CsvParser, JoinEscapesDelimiterAndQuotes)
{
    const QStringList fields{QStringLiteral("a"), QStringLiteral("b,c"), QStringLiteral("d\"e")};
    EXPECT_EQ(CsvParser::joinFields(fields), QStringLiteral("a,\"b,c\",\"d\"\"e\""));
}

TEST(CsvParser, EscapeFieldEscapesOnlyWhenNeeded)
{
    EXPECT_EQ(CsvParser::escapeField(QStringLiteral("plain")), QStringLiteral("plain"));
    EXPECT_EQ(CsvParser::escapeField(QStringLiteral("with,comma")), QStringLiteral("\"with,comma\""));
    EXPECT_EQ(CsvParser::escapeField(QStringLiteral("with\"quote")), QStringLiteral("\"with\"\"quote\""));
}

TEST(CsvParser, LooksLikeHeader)
{
    const CsvParser parser;
    const QStringList expected{QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("age")};

    // 完整匹配
    EXPECT_TRUE(parser.looksLikeHeader(
        QStringList{QStringLiteral("id"), QStringLiteral("name"), QStringLiteral("age")}, expected));
    // 顺序打乱仍能识别（核心修复点）
    EXPECT_TRUE(parser.looksLikeHeader(
        QStringList{QStringLiteral("name"), QStringLiteral("id"), QStringLiteral("age")}, expected));
    // 含未期望的额外列也能识别
    EXPECT_TRUE(parser.looksLikeHeader(
        QStringList{QStringLiteral("extra"), QStringLiteral("id"), QStringLiteral("name")}, expected));
    // 单列表头：阈值自动夹到 1，仍可识别
    EXPECT_TRUE(parser.looksLikeHeader(
        QStringList{QStringLiteral("id")}, QStringList{QStringLiteral("id")}));
    // 数据行：值与表头名不冲突，不误判
    EXPECT_FALSE(parser.looksLikeHeader(
        QStringList{QStringLiteral("1"), QStringLiteral("alice"), QStringLiteral("30")}, expected));
    // 空输入
    EXPECT_FALSE(parser.looksLikeHeader(QStringList{}, expected));
    EXPECT_FALSE(parser.looksLikeHeader(
        QStringList{QStringLiteral("id")}, QStringList{}));
}

TEST(CsvParser, CustomDelimiter)
{
    const CsvParser parser(QChar(';'));
    const QStringList result = parser.parseLine(QStringLiteral("a;b;c"));
    EXPECT_EQ(result.size(), 3);
    EXPECT_EQ(result[1], QStringLiteral("b"));
}
