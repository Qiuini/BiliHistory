file(GLOB LICENSING_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB LICENSING_HEADERS ${CMAKE_CURRENT_SOURCE_DIR}/*.h)

add_library(BiliHistoryLicensing STATIC ${LICENSING_SOURCES} ${LICENSING_HEADERS})

target_include_directories(BiliHistoryLicensing
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(BiliHistoryLicensing
    PUBLIC
        Qt6::Core
        Qt6::Network
        OpenSSL::SSL
        OpenSSL::Crypto
)
