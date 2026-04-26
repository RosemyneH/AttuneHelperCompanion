cmake_minimum_required(VERSION 3.22)
project(credentials_ndk C)

add_library(credentials_ndk SHARED
    src/main/cpp/credentials_jni.c
)
find_library(log_lib log)
target_link_libraries(credentials_ndk ${log_lib})
