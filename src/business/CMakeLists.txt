set(BUSINESS_SOURCES
    analytics.cpp
    exporter.cpp
    updater.cpp
    fetch_worker.cpp
)

set(BUSINESS_HEADERS
    analytics.h
    exporter.h
    updater.h
    fetch_worker.h
)

add_library(BiliHistoryBusiness STATIC ${BUSINESS_SOURCES} ${BUSINESS_HEADERS})

target_include_directories(BiliHistoryBusiness
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(BiliHistoryBusiness
    PUBLIC
        Qt6::Core
        Qt6::Network
    PRIVATE
        BiliHistoryCore
        BiliHistoryNetwork
)
