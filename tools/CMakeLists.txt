add_executable(BiliHistoryKeygen keygen.cpp)
target_link_libraries(BiliHistoryKeygen PRIVATE Qt6::Core OpenSSL::SSL OpenSSL::Crypto)

add_executable(BiliHistoryIssueLicense issue_license.cpp)
target_link_libraries(BiliHistoryIssueLicense PRIVATE Qt6::Core OpenSSL::SSL OpenSSL::Crypto)
