cmake_minimum_required(VERSION 3.22)

project(GlyphGridJUCE VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(JUCE CONFIG QUIET)

if (NOT JUCE_FOUND)
    if (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
        add_subdirectory(JUCE)
    elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../Orbits/JUCE/CMakeLists.txt")
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../Orbits/JUCE" JUCE)
    else()
        message(FATAL_ERROR "JUCE not found. Place JUCE in ./JUCE or set JUCE_DIR.")
    endif()
endif()

juce_add_gui_app(GlyphGridJUCE
    PRODUCT_NAME "GlyphGrid"
)

target_sources(GlyphGridJUCE PRIVATE
    Source/Main.cpp
    Source/MainComponent.h
    Source/MainComponent.cpp
    Source/ExpressionEvaluator.h
    Source/ExpressionEvaluator.cpp
)

target_compile_definitions(GlyphGridJUCE PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
)

target_link_libraries(GlyphGridJUCE PRIVATE
    juce::juce_gui_extra
    juce::juce_audio_utils
)

juce_generate_juce_header(GlyphGridJUCE)
