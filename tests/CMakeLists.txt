file(GLOB_RECURSE TEST_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE TEST_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_executable(BiliHistoryTests ${TEST_SOURCES} ${TEST_HEADERS})

# 测试 helper（如 TestHttpServer）跨目录共享
target_include_directories(BiliHistoryTests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/network)

target_link_libraries(BiliHistoryTests
    PRIVATE
        BiliHistoryCore
        BiliHistoryNetwork
        BiliHistoryBusiness
        BiliHistoryGui
        miniz
        Qt6::Core
        Qt6::Network
        Qt6::Widgets
        GTest::gtest
)

include(GoogleTest)
# PRE_TEST 模式将测试发现推迟到 ctest 运行时，避免构建阶段因 PATH 中缺少 Qt DLL 而失败
gtest_discover_tests(BiliHistoryTests DISCOVERY_MODE PRE_TEST)
