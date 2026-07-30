file(GLOB CORE_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB CORE_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(BiliHistoryCore STATIC ${CORE_SOURCES} ${CORE_HEADERS})

target_include_directories(BiliHistoryCore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(BiliHistoryCore
    PUBLIC
        Qt6::Core
        Qt6::Network
        # SecretsStore 已迁移至 licensing 层，但 core/config.cpp 的 cookie 存取
        # 仍使用 SecretsStore，故 core 仍需链接 BiliHistoryLicensing；
        # tests/licensing/test_secrets_store.cpp 亦经此 PUBLIC 传递获取头文件路径。
        BiliHistoryLicensing
)

target_compile_definitions(BiliHistoryCore PUBLIC APP_VERSION="${PROJECT_VERSION}")
