set(GUI_SOURCES
    theme.cpp
    animation_utils.cpp
    history_table_model.cpp
    image_loader.cpp
    following_page.cpp
    favorites_page.cpp
    main_window.cpp
    settings_dialog.cpp
    activation_dialog.cpp
    trial_expired_dialog.cpp
    stats_page.cpp
    profile_page.cpp
    filter_dialog.cpp
)

set(GUI_HEADERS
    theme.h
    animation_utils.h
    history_table_model.h
    image_loader.h
    following_page.h
    favorites_page.h
    main_window.h
    settings_dialog.h
    activation_dialog.h
    trial_expired_dialog.h
    stats_page.h
    profile_page.h
    filter_dialog.h
)

add_library(BiliHistoryGui STATIC ${GUI_SOURCES} ${GUI_HEADERS})

target_include_directories(BiliHistoryGui
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
)

target_link_libraries(BiliHistoryGui
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
    PRIVATE
        BiliHistoryCore
        BiliHistoryNetwork
        BiliHistoryBusiness
)
