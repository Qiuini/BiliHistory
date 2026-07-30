file(GLOB NETWORK_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/coro/*.cpp
)
file(GLOB NETWORK_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/*.h
    ${CMAKE_CURRENT_SOURCE_DIR}/coro/*.h
)

add_library(BiliHistoryNetwork STATIC ${NETWORK_SOURCES} ${NETWORK_HEADERS})

target_include_directories(BiliHistoryNetwork
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(BiliHistoryNetwork
    PUBLIC
        BiliHistoryCore
        Qt6::Core
        Qt6::Network
)
