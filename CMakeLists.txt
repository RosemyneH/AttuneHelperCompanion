cmake_minimum_required(VERSION 3.24)

project(AttuneHelperCompanion VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(AHC_BUILD_APP "Build the native companion application" ON)
option(AHC_BUILD_TESTS "Build tests" ON)

add_library(ahc_core
    src/addons/addon_catalog.c
    src/addons/addon_manifest.c
    src/attune/attune_snapshot.c
)

target_include_directories(ahc_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(MSVC)
    target_compile_options(ahc_core PRIVATE /W4)
else()
    target_compile_options(ahc_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(AHC_BUILD_APP)
    include(FetchContent)
    set(FETCHCONTENT_QUIET OFF)

    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

    FetchContent_Declare(
        raylib
        GIT_REPOSITORY https://github.com/raysan5/raylib.git
        GIT_TAG 5.5
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
        GIT_CONFIG http.version=HTTP/1.1
    )

    FetchContent_Declare(
        raygui
        GIT_REPOSITORY https://github.com/raysan5/raygui.git
        GIT_TAG 4.0
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
        GIT_CONFIG http.version=HTTP/1.1
    )

    FetchContent_MakeAvailable(raylib raygui)

    add_executable(attune_helper_companion
        apps/companion/main.c
    )

    target_include_directories(attune_helper_companion
        PRIVATE
            ${raygui_SOURCE_DIR}/src
    )
    target_compile_definitions(attune_helper_companion
        PRIVATE
            AHC_SOURCE_MANIFEST_PATH="${CMAKE_CURRENT_SOURCE_DIR}/manifest/addons.json"
    )

    target_link_libraries(attune_helper_companion
        PRIVATE
            ahc_core
            raylib
    )

    add_custom_command(TARGET attune_helper_companion POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/manifest
            $<TARGET_FILE_DIR:attune_helper_companion>/manifest
    )

    if(MSVC)
        target_compile_options(attune_helper_companion PRIVATE /W4)
    else()
        target_compile_options(attune_helper_companion PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

if(AHC_BUILD_TESTS)
    include(CTest)

    add_executable(attune_snapshot_test
        tests/attune_snapshot_test.c
    )
    add_executable(addon_manifest_test
        tests/addon_manifest_test.c
    )

    target_link_libraries(attune_snapshot_test
        PRIVATE
            ahc_core
    )
    target_link_libraries(addon_manifest_test
        PRIVATE
            ahc_core
    )

    target_compile_definitions(attune_snapshot_test
        PRIVATE
            AHC_TEST_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
    )
    target_compile_definitions(addon_manifest_test
        PRIVATE
            AHC_TEST_MANIFEST_PATH="${CMAKE_CURRENT_SOURCE_DIR}/manifest/addons.json"
    )

    if(MSVC)
        target_compile_options(attune_snapshot_test PRIVATE /W4)
        target_compile_options(addon_manifest_test PRIVATE /W4)
    else()
        target_compile_options(attune_snapshot_test PRIVATE -Wall -Wextra -Wpedantic)
        target_compile_options(addon_manifest_test PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME attune_snapshot_test COMMAND attune_snapshot_test)
    add_test(NAME addon_manifest_test COMMAND addon_manifest_test)
endif()
