cmake_minimum_required(VERSION 3.25)

# JUCE builds helper programs using both C and C++.
project(RCABA6A VERSION 0.6.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(RCABA6A_BUILD_PLUGIN "Build JUCE VST3, AU and Standalone formats" ON)
include(CTest)

set(RCABA6A_DSP_FILES
    Source/DSP/BA6AEngine.h
    Source/DSP/ControlCalibration.h
    Source/DSP/BA6AEngine.cpp
)
set(RCABA6A_PLUGIN_FILES
    Source/PluginProcessor.h
    Source/PluginProcessor.cpp
    Source/PluginEditor.h
    Source/PluginEditor.cpp
)
set(RCABA6A_ASSETS
    assets/panel.jpg
    assets/knob_input.png
    assets/knob_meter.png
    assets/knob_mode.png
    assets/knob_output.png
)
set(RCABA6A_TEST_FILES
    Tests/EngineTests.cpp
    Tests/ControlCalibrationTests.cpp
    Tests/GUIAssetTests.cpp
)

# Fail at configure time if a referenced source or runtime asset is missing.
foreach(file IN LISTS RCABA6A_DSP_FILES RCABA6A_PLUGIN_FILES RCABA6A_ASSETS RCABA6A_TEST_FILES)
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${file}")
        message(FATAL_ERROR "Missing required file: ${file}")
    endif()
endforeach()

add_library(BA6A_DSP STATIC ${RCABA6A_DSP_FILES})
target_include_directories(BA6A_DSP PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Source")
if(MSVC)
    target_compile_options(BA6A_DSP PRIVATE /W4)
else()
    target_compile_options(BA6A_DSP PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    add_executable(BA6A_Tests Tests/EngineTests.cpp)
    target_link_libraries(BA6A_Tests PRIVATE BA6A_DSP)
    add_test(NAME DSP_Regression COMMAND BA6A_Tests)

    add_executable(BA6A_ControlTests Tests/ControlCalibrationTests.cpp)
    target_link_libraries(BA6A_ControlTests PRIVATE BA6A_DSP)
    add_test(NAME Control_Detents_Regression COMMAND BA6A_ControlTests)

    add_executable(BA6A_GUI_AssetTests Tests/GUIAssetTests.cpp)
    add_test(NAME GUI_AssetIntegrity
        COMMAND BA6A_GUI_AssetTests "${CMAKE_CURRENT_SOURCE_DIR}/assets")

    # Optional visual regression; requires Pillow only for this development test.
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        execute_process(
            COMMAND "${Python3_EXECUTABLE}" -c "from PIL import Image"
            RESULT_VARIABLE RCABA6A_PIL_AVAILABLE
            OUTPUT_QUIET ERROR_QUIET)
        if(RCABA6A_PIL_AVAILABLE EQUAL 0)
            add_test(NAME StaticBezel_Regression
                COMMAND "${Python3_EXECUTABLE}"
                        "${CMAKE_CURRENT_SOURCE_DIR}/Tests/StaticBezelTests.py"
                        "${CMAKE_CURRENT_SOURCE_DIR}/assets")
        endif()
    endif()
endif()

if(RCABA6A_BUILD_PLUGIN)
    # Exact stable JUCE 8 tag (no system JUCE installation is required).
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(JUCE)

    set(RCABA6A_FORMATS VST3 Standalone)
    if(APPLE)
        list(APPEND RCABA6A_FORMATS AU)
    endif()

    set(RCABA6A_ICON_ARGS)
    if(APPLE AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/assets/app_icon.png")
        list(APPEND RCABA6A_ICON_ARGS ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/app_icon.png")
    endif()
    juce_add_plugin(RCABA6A
        VERSION 0.6.0
        COMPANY_NAME "Independent Audio"
        BUNDLE_ID "audio.independent.rcaba6a"
        PLUGIN_MANUFACTURER_CODE Iaau
        PLUGIN_CODE Rb6a
        FORMATS ${RCABA6A_FORMATS}
        PRODUCT_NAME "rcaba6a"
        PLUGIN_NAME "RCA BA-6A"
        ${RCABA6A_ICON_ARGS}
        DESCRIPTION "RCA BA-6A inspired variable-gain tube limiting amplifier"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
    )

    juce_generate_juce_header(RCABA6A)
    juce_add_binary_data(RCABA6A_Assets SOURCES ${RCABA6A_ASSETS})
    target_sources(RCABA6A PRIVATE ${RCABA6A_PLUGIN_FILES})
    target_link_libraries(RCABA6A PRIVATE
        BA6A_DSP
        RCABA6A_Assets
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_extra
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
    )
    target_compile_definitions(RCABA6A PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )
endif()
