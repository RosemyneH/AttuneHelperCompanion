cmake_minimum_required(VERSION 3.24)

project(AttuneHelperCompanion VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)

option(AHC_BUILD_APP "Build the native companion application" ON)
option(AHC_BUILD_TESTS "Build tests" ON)
option(AHC_BUILD_BENCH "Build parse benchmark tool" ON)
option(AHC_ENABLE_MASM "Use MASM ahc_memchr on MSVC x64 (else C memchr)" OFF)
set(AHC_OPT_PROFILE "default" CACHE STRING "Optimization profile: default, release, lto, pgo_instrument, sanitize")
set_property(CACHE AHC_OPT_PROFILE PROPERTY STRINGS default release lto pgo_instrument sanitize)

include(GNUInstallDirs)

if(AHC_ENABLE_MASM AND MSVC)
    if(NOT (CMAKE_SIZEOF_VOID_P EQUAL 8))
        set(AHC_ENABLE_MASM OFF CACHE BOOL "" FORCE)
    else()
        enable_language(ASM_MASM)
    endif()
endif()

if(AHC_ENABLE_MASM AND MSVC AND (CMAKE_SIZEOF_VOID_P EQUAL 8))
    set(AHC_MEMCHR_SRC src/ahc/ahc_memchr_masm64.asm)
    set_source_files_properties(
        src/ahc/ahc_memchr_masm64.asm
        PROPERTIES
        LANGUAGE ASM_MASM
    )
else()
    set(AHC_MEMCHR_SRC src/ahc/ahc_memchr_dispatch.c)
endif()

set(AHC_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(AHC_GENERATED_ADDON_CATALOG "${AHC_GENERATED_DIR}/addon_catalog_generated.c")
add_custom_command(
    OUTPUT "${AHC_GENERATED_ADDON_CATALOG}"
    COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_addon_catalog.py"
        --input "${CMAKE_CURRENT_SOURCE_DIR}/manifest/addons.json"
        --output "${AHC_GENERATED_ADDON_CATALOG}"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/manifest/addons.json"
        "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_addon_catalog.py"
    COMMENT "Generating baked addon catalog"
    VERBATIM
)

add_library(ahc_core
    "${AHC_GENERATED_ADDON_CATALOG}"
    src/addons/addon_manifest.c
    src/attune/attune_snapshot.c
    src/ahc/ahc_arena.c
    src/ahc/ahc_mem_telemetry.c
    src/ahc/ahc_find_literal.c
    ${AHC_MEMCHR_SRC}
)

target_include_directories(ahc_core
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

if(WIN32)
    target_link_libraries(ahc_core PRIVATE psapi)
endif()

if(MSVC)
    target_compile_options(ahc_core PRIVATE /W4)
    if(AHC_OPT_PROFILE STREQUAL "release" OR AHC_OPT_PROFILE STREQUAL "lto" OR AHC_OPT_PROFILE STREQUAL "pgo_instrument")
        target_compile_options(ahc_core PRIVATE /O2)
    endif()
    if(AHC_OPT_PROFILE STREQUAL "lto" OR AHC_OPT_PROFILE STREQUAL "pgo_instrument")
        target_compile_options(ahc_core PRIVATE /GL)
        set_target_properties(ahc_core PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
    endif()
else()
    target_compile_options(ahc_core PRIVATE -Wall -Wextra -Wpedantic)
    if(AHC_OPT_PROFILE STREQUAL "release" OR AHC_OPT_PROFILE STREQUAL "lto" OR AHC_OPT_PROFILE STREQUAL "pgo_instrument")
        target_compile_options(ahc_core PRIVATE -O2)
    endif()
    if(AHC_OPT_PROFILE STREQUAL "lto" OR AHC_OPT_PROFILE STREQUAL "pgo_instrument")
        target_compile_options(ahc_core PRIVATE -flto)
        set_target_properties(ahc_core PROPERTIES INTERPROCEDURAL_OPTIMIZATION ON)
    endif()
    if(AHC_OPT_PROFILE STREQUAL "sanitize")
        target_compile_options(ahc_core PRIVATE -fsanitize=address,undefined -g)
        target_link_options(ahc_core PRIVATE -fsanitize=address,undefined)
    endif()
endif()

if(AHC_BUILD_BENCH)
    add_executable(ahc_parse_bench apps/parse_bench.c)
    target_link_libraries(ahc_parse_bench PRIVATE ahc_core)
    if(WIN32)
        target_link_libraries(ahc_parse_bench PRIVATE psapi)
    endif()
    if(MSVC)
        target_compile_options(ahc_parse_bench PRIVATE /W4)
    else()
        target_compile_options(ahc_parse_bench PRIVATE -Wall -Wextra -Wpedantic)
    endif()
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
        apps/companion/ahc_tray.c
        apps/companion/ahc_process.c
    )
    if(WIN32)
        set_target_properties(attune_helper_companion PROPERTIES WIN32_EXECUTABLE TRUE)
        if(MSVC)
            target_link_options(attune_helper_companion PRIVATE /ENTRY:mainCRTStartup)
        endif()
    endif()

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
            Threads::Threads
    )
    if(WIN32)
        target_link_libraries(attune_helper_companion PRIVATE shell32 user32)
    endif()

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

    install(
        TARGETS attune_helper_companion
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    )
    install(
        DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/manifest/"
        DESTINATION "${CMAKE_INSTALL_BINDIR}/manifest"
        FILES_MATCHING
            PATTERN "*"
    )
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

    if(AHC_OPT_PROFILE STREQUAL "sanitize" AND NOT MSVC)
        target_compile_options(attune_snapshot_test PRIVATE -fsanitize=address,undefined -g)
        target_link_options(attune_snapshot_test PRIVATE -fsanitize=address,undefined)
        target_compile_options(addon_manifest_test PRIVATE -fsanitize=address,undefined -g)
        target_link_options(addon_manifest_test PRIVATE -fsanitize=address,undefined)
    endif()

    add_test(NAME attune_snapshot_test COMMAND attune_snapshot_test)
    add_test(NAME addon_manifest_test COMMAND addon_manifest_test)
endif()
