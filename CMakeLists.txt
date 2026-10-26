cmake_minimum_required(VERSION 3.22)

project(GoldenEra VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# ---------------------------------------------------------------------------
# JUCE: use a local checkout if JUCE_SOURCE_DIR is given, otherwise download it.
# ---------------------------------------------------------------------------
set(JUCE_SOURCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(JUCE_SOURCE_DIR)
    add_subdirectory(${JUCE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# Plug-in formats per platform (override with -DERA_FORMATS="VST3;Standalone")
if(NOT ERA_FORMATS)
    if(APPLE)
        set(ERA_FORMATS VST3 AU)
    elseif(WIN32)
        set(ERA_FORMATS VST3)
    else()
        set(ERA_FORMATS VST3 Standalone)
    endif()
endif()

set(ERA_SHARED_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/EraLookAndFeel.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/EraProcessorBase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/PresetManager.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/EraWidgets.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/EraEditorBase.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/shared/SpectrumAnalyzer.cpp)

# era_add_plugin(<target> <product name> <4-char code> <sources...>)
function(era_add_plugin target productName pluginCode)
    juce_add_plugin(${target}
        COMPANY_NAME "8cee"
        COMPANY_WEBSITE "https://github.com/cubale92"
        BUNDLE_ID "com.8cee.${target}"
        PLUGIN_MANUFACTURER_CODE Ecee
        PLUGIN_CODE ${pluginCode}
        PRODUCT_NAME "${productName}"
        DESCRIPTION "8cee Golden Era Bundle"
        FORMATS ${ERA_FORMATS}
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        VST3_CATEGORIES Fx
        AU_MAIN_TYPE kAudioUnitType_Effect)

    target_sources(${target} PRIVATE ${ARGN} ${ERA_SHARED_SOURCES})
    target_include_directories(${target} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/shared
        ${CMAKE_CURRENT_SOURCE_DIR}/plugins/${target})

    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        ERA_VERSION_STRING="${PROJECT_VERSION}")

    target_link_libraries(${target}
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()

set(ERA_PLUGINS "EraQ;EraC;EraS;EraL" CACHE STRING "Which plug-ins to build")
foreach(P IN LISTS ERA_PLUGINS)
    string(SUBSTRING ${P} 3 1 letter)
    era_add_plugin(${P} "8cee Era-${letter}" ${P}
        plugins/${P}/${P}Processor.cpp plugins/${P}/${P}Editor.cpp plugins/${P}/${P}Presets.cpp)
endforeach()

# Offline test harness (renders test signals through every plug-in + every preset)
option(ERA_BUILD_TESTS "Build the offline DSP test harness" OFF)
if(ERA_BUILD_TESTS)
    add_subdirectory(tests)
endif()
