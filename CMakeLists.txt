cmake_minimum_required(VERSION 3.19)

project(qt_4ch_viewer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network Widgets)

if(NOT DEFINED VCPKG_INSTALLED_DIR)
    set(VCPKG_INSTALLED_DIR "C:/dev/vcpkg/installed" CACHE PATH "vcpkg installed directory")
endif()
if(NOT DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_TARGET_TRIPLET "x64-mingw-dynamic" CACHE STRING "vcpkg target triplet")
endif()

set(VCPKG_TRIPLET_ROOT "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
set(FFMPEG_INCLUDE_DIRS "${VCPKG_TRIPLET_ROOT}/include")
set(FFMPEG_LIBRARY_DIRS "${VCPKG_TRIPLET_ROOT}/lib")

find_library(FFMPEG_AVFORMAT_LIBRARY NAMES avformat libavformat PATHS "${FFMPEG_LIBRARY_DIRS}" NO_DEFAULT_PATH REQUIRED)
find_library(FFMPEG_AVCODEC_LIBRARY NAMES avcodec libavcodec PATHS "${FFMPEG_LIBRARY_DIRS}" NO_DEFAULT_PATH REQUIRED)
find_library(FFMPEG_AVUTIL_LIBRARY NAMES avutil libavutil PATHS "${FFMPEG_LIBRARY_DIRS}" NO_DEFAULT_PATH REQUIRED)
find_library(FFMPEG_SWSCALE_LIBRARY NAMES swscale libswscale PATHS "${FFMPEG_LIBRARY_DIRS}" NO_DEFAULT_PATH REQUIRED)
find_library(FFMPEG_SWRESAMPLE_LIBRARY NAMES swresample libswresample PATHS "${FFMPEG_LIBRARY_DIRS}" NO_DEFAULT_PATH REQUIRED)

set(FFMPEG_LIBRARIES
    ${FFMPEG_AVFORMAT_LIBRARY}
    ${FFMPEG_AVCODEC_LIBRARY}
    ${FFMPEG_AVUTIL_LIBRARY}
    ${FFMPEG_SWSCALE_LIBRARY}
    ${FFMPEG_SWRESAMPLE_LIBRARY}
)

qt_standard_project_setup()

qt_add_executable(qt_4ch_viewer
    WIN32 MACOSX_BUNDLE
    main.cpp
    app_logging.cpp
    app_logging.h
    legal_notice_widget.cpp
    legal_notice_widget.h
    playback_api_client.cpp
    playback_api_client.h
    playback_types.h
    parking_types.h
    stm_types.h
    stm_api_client.cpp
    stm_api_client.h
    parking_event_types.h
    parking_event_api_client.cpp
    parking_event_api_client.h
    parking_event_store.cpp
    parking_event_store.h
    parking_event_card_widget.cpp
    parking_event_card_widget.h
    stm_fire_response_dialog.cpp
    stm_fire_response_dialog.h
    stm_device_list_widget.cpp
    stm_device_list_widget.h
    parking_zone_canvas.cpp
    parking_zone_canvas.h
    parking_zone_editor.cpp
    parking_zone_editor.h
    timeline_widget.cpp
    timeline_widget.h
    theme.cpp
    theme.h
    main_window.cpp
    main_window.h
    stream_worker.cpp
    stream_worker.h
    video_panel.cpp
    video_panel.h
    resources.qrc
)

if(WIN32)
    target_sources(qt_4ch_viewer PRIVATE app_icon.rc)
endif()

target_include_directories(qt_4ch_viewer
    PRIVATE
        ${FFMPEG_INCLUDE_DIRS}
)

target_link_directories(qt_4ch_viewer
    PRIVATE
        ${FFMPEG_LIBRARY_DIRS}
)

target_compile_definitions(qt_4ch_viewer
    PRIVATE VEDA_VMS_VERSION=\"${PROJECT_VERSION}\"
)

target_link_libraries(qt_4ch_viewer
    PRIVATE
        Qt::Core
        Qt::Network
        Qt::Widgets
        ${FFMPEG_LIBRARIES}
        ${FFMPEG_SWSCALE_LIBRARY}
        ${FFMPEG_SWRESAMPLE_LIBRARY}
)

if(WIN32 AND DEFINED VCPKG_INSTALLED_DIR)
    set(VCPKG_FFMPEG_BIN_DIR "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/bin")
    if(EXISTS "${VCPKG_FFMPEG_BIN_DIR}")
        add_custom_command(TARGET qt_4ch_viewer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${VCPKG_FFMPEG_BIN_DIR}"
                    "$<TARGET_FILE_DIR:qt_4ch_viewer>"
            COMMENT "Copying vcpkg runtime DLLs"
        )
    endif()
endif()

install(TARGETS qt_4ch_viewer
    BUNDLE DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

install(FILES third_party/pretendard/LICENSE
    DESTINATION licenses/pretendard
)

qt_generate_deploy_app_script(
    TARGET qt_4ch_viewer
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)
install(SCRIPT ${deploy_script})



