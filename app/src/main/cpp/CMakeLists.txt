cmake_minimum_required(VERSION 3.22.1)
project("linein")

# Find the Oboe package
find_package(oboe REQUIRED CONFIG)

# Create the native library
add_library(${CMAKE_PROJECT_NAME} SHARED
    jni_bridge.cpp
    PassthroughEngine.cpp
)

# Link against Oboe and other required libraries
target_link_libraries(${CMAKE_PROJECT_NAME}
    oboe::oboe
    android
    log
)

# Enable C++17
target_compile_features(${CMAKE_PROJECT_NAME} PRIVATE cxx_std_17)

# 16KB page size alignment for Android 15+ compatibility
target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "-Wl,-z,max-page-size=16384")
