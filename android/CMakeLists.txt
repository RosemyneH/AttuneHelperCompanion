# Optional: add_subdirectory from top with -DAHC_BUILD_ANDROID=ON for ahc_android_stub
if(NOT TARGET ahc_android_stub)
    add_library(ahc_android_stub INTERFACE)
    target_include_directories(ahc_android_stub INTERFACE "${CMAKE_CURRENT_LIST_DIR}/../src")
    target_compile_definitions(ahc_android_stub INTERFACE AHC_ANDROID=1)
endif()
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(
        STATUS
        "AHC: build android/ from the AttuneHelperCompanion project with -DAHC_BUILD_ANDROID=ON, or open android/ in Android Studio."
    )
endif()
