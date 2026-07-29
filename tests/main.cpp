#include <gtest/gtest.h>

#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("BiliHistoryTests"));

    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
