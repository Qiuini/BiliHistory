cmake_minimum_required(VERSION 3.21)
project(BiliHistory VERSION 1.0.12 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 统一输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Qt6 自动处理
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets)

# Phase 3 授权加密层强制依赖 OpenSSL 3.x
# 优先使用 vcpkg 提供的 OpenSSL，避免系统/Strawberry 等冲突版本
set(_BILIHISTORY_VCPKG_OPENSSL_ROOT "")
if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
    set(_BILIHISTORY_VCPKG_OPENSSL_ROOT "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
elseif(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg_installed")
    file(GLOB _BILIHISTORY_VCPKG_TRIPLETS RELATIVE "${CMAKE_SOURCE_DIR}/vcpkg_installed" "${CMAKE_SOURCE_DIR}/vcpkg_installed/*-windows*")
    foreach(_triplet ${_BILIHISTORY_VCPKG_TRIPLETS})
        if(EXISTS "${CMAKE_SOURCE_DIR}/vcpkg_installed/${_triplet}/include/openssl/opensslv.h")
            set(_BILIHISTORY_VCPKG_OPENSSL_ROOT "${CMAKE_SOURCE_DIR}/vcpkg_installed/${_triplet}")
            break()
        endif()
    endforeach()
endif()

if(EXISTS "${_BILIHISTORY_VCPKG_OPENSSL_ROOT}/include/openssl/opensslv.h")
    # 清除旧缓存，防止之前配置中指向了系统/Strawberry OpenSSL
    unset(OPENSSL_ROOT_DIR CACHE)
    unset(OPENSSL_INCLUDE_DIR CACHE)
    unset(OPENSSL_CRYPTO_LIBRARY CACHE)
    unset(OPENSSL_SSL_LIBRARY CACHE)
    unset(OPENSSL_LIBRARIES CACHE)
    unset(LIB_EAY CACHE)
    unset(SSL_EAY CACHE)
    set(OPENSSL_ROOT_DIR "${_BILIHISTORY_VCPKG_OPENSSL_ROOT}")
    find_package(OpenSSL 3.0 REQUIRED)
else()
    find_package(OpenSSL 3.0 REQUIRED)
endif()
message(STATUS "OpenSSL found: ${OPENSSL_VERSION} at ${OPENSSL_INCLUDE_DIR}")

# GTest via FetchContent，避免本地未安装 GoogleTest
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

enable_testing()

# Licensing 授权加密层（Core 依赖它）
add_subdirectory(src/licensing)

# Core 核心静态库
add_subdirectory(src/core)

# Network 网络层
add_subdirectory(src/network)

# Business 业务层
add_subdirectory(src/business)

# GUI 层
add_subdirectory(src/gui)

# 主程序入口（Phase 5 完整实现）
add_executable(BiliHistory src/main.cpp)
target_link_libraries(BiliHistory PRIVATE BiliHistoryCore BiliHistoryNetwork BiliHistoryBusiness BiliHistoryGui Qt6::Core Qt6::Widgets)

# Windows：部署 Qt / OpenSSL DLL 到可执行文件目录，便于 CPack 打包
if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt6 HINTS "${QT6_INSTALL_PREFIX}/bin" "${Qt6Core_DIR}/../../../bin")
    if(NOT WINDEPLOYQT_EXECUTABLE)
        find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${QT6_INSTALL_PREFIX}/bin" "${Qt6Core_DIR}/../../../bin")
    endif()

    # 从已找到的 OpenSSL 库路径推导 DLL 目录（优先 bin/，其次 vcpkg 安装目录）
    set(_BILIHISTORY_OPENSSL_DLL_DIR "")
    if(OPENSSL_CRYPTO_LIBRARY)
        get_filename_component(_BILIHISTORY_OPENSSL_LIB_DIR "${OPENSSL_CRYPTO_LIBRARY}" DIRECTORY)
        get_filename_component(_BILIHISTORY_OPENSSL_ROOT_DIR "${_BILIHISTORY_OPENSSL_LIB_DIR}" DIRECTORY)
        if(EXISTS "${_BILIHISTORY_OPENSSL_ROOT_DIR}/bin")
            set(_BILIHISTORY_OPENSSL_DLL_DIR "${_BILIHISTORY_OPENSSL_ROOT_DIR}/bin")
        endif()
    endif()
    if(NOT _BILIHISTORY_OPENSSL_DLL_DIR AND DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
        set(_BILIHISTORY_OPENSSL_DLL_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
    endif()

    set(OPENSSL_DLLS "")
    if(_BILIHISTORY_OPENSSL_DLL_DIR)
        file(GLOB OPENSSL_DLLS
            "${_BILIHISTORY_OPENSSL_DLL_DIR}/libcrypto-*.dll"
            "${_BILIHISTORY_OPENSSL_DLL_DIR}/libssl-*.dll"
        )
    endif()
    # 过滤掉异常文件名（如 Strawberry 的 __ 后缀）
    set(_BILIHISTORY_OPENSSL_DLLS "")
    foreach(dll ${OPENSSL_DLLS})
        if(dll MATCHES "lib(crypto|ssl)-[0-9]+-x64\\.dll$")
            list(APPEND _BILIHISTORY_OPENSSL_DLLS "${dll}")
        endif()
    endforeach()
    set(OPENSSL_DLLS "${_BILIHISTORY_OPENSSL_DLLS}")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET BiliHistory POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}" --no-compiler-runtime --no-translations "$<TARGET_FILE:BiliHistory>"
            COMMENT "Deploying Qt dependencies for BiliHistory..."
        )
    endif()

    foreach(dll ${OPENSSL_DLLS})
        add_custom_command(TARGET BiliHistory POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${dll}" "$<TARGET_FILE_DIR:BiliHistory>"
        )
    endforeach()

    if(NOT WINDEPLOYQT_EXECUTABLE)
        message(WARNING "windeployqt not found; Windows package may be incomplete")
    endif()
    if(NOT OPENSSL_DLLS)
        message(WARNING "OpenSSL runtime DLLs not found; package may fail to run on clean systems")
    else()
        message(STATUS "OpenSSL runtime DLLs: ${OPENSSL_DLLS}")
    endif()
endif()

# 安装主程序
install(TARGETS BiliHistory
    RUNTIME DESTINATION .
    BUNDLE DESTINATION .
)

# Windows：安装阶段再次部署 Qt / OpenSSL（供 CPack 使用）
if(WIN32)
    # 安装脚本在 install 阶段执行，CMAKE_INSTALL_PREFIX 可正常展开；
    # 可执行文件路径在配置阶段已知，避免使用 generator expression
    set(BILIHISTORY_EXECUTABLE "${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/BiliHistory.exe")
    # 使用 configure_file + install(SCRIPT) 避免 install(CODE) 的复杂转义问题
    configure_file(
        "${CMAKE_SOURCE_DIR}/cmake/windeployqt_install.cmake.in"
        "${CMAKE_BINARY_DIR}/windeployqt_install.cmake"
        @ONLY
    )
    install(SCRIPT "${CMAKE_BINARY_DIR}/windeployqt_install.cmake")
endif()

# 开发者工具（密钥生成、激活码签发）
add_subdirectory(tools)

# 测试
add_subdirectory(tests)

# CPack 打包配置
set(CPACK_PACKAGE_NAME "BiliHistory")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "BiliHistory")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "B站历史记录管理工具")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "BiliHistory")
set(CPACK_PACKAGE_EXECUTABLES "BiliHistory;BiliHistory")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

# 统一 Linux 包架构名称：amd64 / arm64
if(CMAKE_SYSTEM_PROCESSOR MATCHES "AMD64|x86_64|x64")
    set(BILIHISTORY_PACKAGE_ARCH "amd64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|ARM64|arm64")
    set(BILIHISTORY_PACKAGE_ARCH "arm64")
else()
    set(BILIHISTORY_PACKAGE_ARCH "${CMAKE_SYSTEM_PROCESSOR}")
endif()

# 生成统一的包文件名
if(WIN32)
    set(CPACK_PACKAGE_FILE_NAME "BiliHistory-${PROJECT_VERSION}-x64")
else()
    set(CPACK_PACKAGE_FILE_NAME "BiliHistory-${PROJECT_VERSION}-${BILIHISTORY_PACKAGE_ARCH}")
endif()

if(WIN32)
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_PACKAGE_NAME "BiliHistory")
    set(CPACK_NSIS_DISPLAY_NAME "BiliHistory")
    set(CPACK_NSIS_EXECUTABLES_DIRECTORY ".")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/favicon.ico")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "BiliHistory.exe")
elseif(UNIX AND NOT APPLE)
    set(CPACK_GENERATOR "DEB;RPM;TGZ")
    set(CPACK_DEBIAN_PACKAGE_NAME "bilihistory")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "BiliHistory <you@example.com>")
    set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libqt6core6, libqt6network6, libqt6widgets6, libssl3")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_RPM_PACKAGE_NAME "bilihistory")
    set(CPACK_RPM_PACKAGE_LICENSE "MIT")
    set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase >= 6.0, openssl-libs >= 3.0")
endif()

include(CPack)
