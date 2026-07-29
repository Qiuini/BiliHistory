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
        BiliHistoryLicensing
)

target_compile_definitions(BiliHistoryCore PUBLIC APP_VERSION="${PROJECT_VERSION}")
