cmake_minimum_required(VERSION 3.16)
project(HueZoneBrightnessTest LANGUAGES CXX)

enable_testing()
add_executable(hue_zone_brightness_test main.cpp)
target_compile_features(hue_zone_brightness_test PRIVATE cxx_std_20)
target_include_directories(hue_zone_brightness_test PRIVATE ../../include ../../external/linalg)
add_test(NAME hue_zone_brightness COMMAND hue_zone_brightness_test)
