#include <gtest/gtest.h>

#include <QRegularExpression>
#include <QString>
#include <QStringList>

#include "gui/theme.h"

using namespace bili::gui;

// theme::globalStyleSheet() 是纯字符串生成函数，不依赖任何 UI 渲染。
// theme 命名空间下的颜色常量也是纯数据，可被直接验证。

TEST(ThemeTest, GlobalStyleSheetIsNonEmpty)
{
    const QString sheet = theme::globalStyleSheet();
    EXPECT_FALSE(sheet.isEmpty());
}

TEST(ThemeTest, GlobalStyleSheetContainsKeySelectors)
{
    const QString sheet = theme::globalStyleSheet();
    // 样式表应当覆盖主窗口、按钮、输入框、表格等核心控件
    EXPECT_TRUE(sheet.contains(QStringLiteral("QMainWindow")));
    EXPECT_TRUE(sheet.contains(QStringLiteral("QPushButton")));
    EXPECT_TRUE(sheet.contains(QStringLiteral("QPushButton#primaryButton")));
    EXPECT_TRUE(sheet.contains(QStringLiteral("QPushButton#secondaryButton")));
    EXPECT_TRUE(sheet.contains(QStringLiteral("QLineEdit")));
    EXPECT_TRUE(sheet.contains(QStringLiteral("QTableView")));
    EXPECT_TRUE(sheet.contains(QStringLiteral("QHeaderView::section")));
    EXPECT_TRUE(sheet.contains(QStringLiteral("QScrollBar:vertical")));
}

TEST(ThemeTest, ColorConstantsAreValidHexColors)
{
    // 所有颜色常量应为 #RRGGBB 格式的合法十六进制颜色
    const QStringList colors = {
        theme::PINK,          theme::PINK_HOVER,    theme::PINK_PRESSED,
        theme::BLUE,          theme::BLUE_HOVER,    theme::BLUE_LIGHT,
        theme::PINK_LIGHT,    theme::BG,            theme::CARD,
        theme::SURFACE_HOVER, theme::TEXT,          theme::TEXT_2,
        theme::TEXT_3,        theme::BORDER,        theme::SUCCESS,
        theme::WARNING,       theme::DANGER,
    };
    static const QRegularExpression re(QStringLiteral("^#[0-9A-Fa-f]{6}$"));
    for (const QString& c : colors) {
        EXPECT_TRUE(re.match(c).hasMatch()) << qPrintable(c);
    }
}

TEST(ThemeTest, ColorConstantsHaveExpectedValues)
{
    // 校验关键品牌色值，防止误改
    EXPECT_EQ(theme::PINK, QStringLiteral("#FB7299"));
    EXPECT_EQ(theme::BLUE, QStringLiteral("#00AEEC"));
    EXPECT_EQ(theme::BG, QStringLiteral("#F7F6F9"));
    EXPECT_EQ(theme::CARD, QStringLiteral("#FFFFFF"));
    EXPECT_EQ(theme::TEXT, QStringLiteral("#1F1F2E"));
    EXPECT_EQ(theme::SUCCESS, QStringLiteral("#00B578"));
    EXPECT_EQ(theme::DANGER, QStringLiteral("#FF4D4F"));
}

TEST(ThemeTest, StyleSheetReferencesThemeColors)
{
    const QString sheet = theme::globalStyleSheet();
    // 主窗口背景使用 BG，主按钮使用 PINK 系列颜色
    EXPECT_TRUE(sheet.contains(theme::BG));
    EXPECT_TRUE(sheet.contains(theme::PINK));
    EXPECT_TRUE(sheet.contains(theme::PINK_HOVER));
    EXPECT_TRUE(sheet.contains(theme::PINK_PRESSED));
    EXPECT_TRUE(sheet.contains(theme::TEXT));
    EXPECT_TRUE(sheet.contains(theme::BORDER));
}

TEST(ThemeTest, StyleSheetIsDeterministic)
{
    // 同一次调用应返回相同内容（纯函数无副作用）
    const QString a = theme::globalStyleSheet();
    const QString b = theme::globalStyleSheet();
    EXPECT_EQ(a, b);
}
