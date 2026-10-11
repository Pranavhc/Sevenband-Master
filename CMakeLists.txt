cmake_minimum_required(VERSION 3.22)
project(SevenBandMaster VERSION 0.2.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4)
FetchContent_MakeAvailable(JUCE)

# VST3 everywhere; AU on macOS (Logic Pro only hosts Audio Units, not VST3)
set(PLUGIN_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND PLUGIN_FORMATS AU)
endif()

juce_add_plugin(SevenBandMaster
    COMPANY_NAME "YourName"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Ynam
    PLUGIN_CODE Sbm1
    FORMATS ${PLUGIN_FORMATS}
    PRODUCT_NAME "Seven Band Master"
    VST3_CATEGORIES "Fx" "Dynamics"
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD TRUE)

juce_generate_juce_header(SevenBandMaster)

# Find the sources whether they live in Source/ or sit next to this file
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Source/PluginProcessor.cpp")
    set(SBM_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/Source")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/PluginProcessor.cpp")
    set(SBM_SRC_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
else()
    message(FATAL_ERROR
        "PluginProcessor.cpp not found. Put PluginProcessor.cpp/.h and "
        "PluginEditor.cpp/.h either in '${CMAKE_CURRENT_SOURCE_DIR}/Source' "
        "or directly in '${CMAKE_CURRENT_SOURCE_DIR}'.")
endif()
message(STATUS "Using sources from: ${SBM_SRC_DIR}")

target_sources(SevenBandMaster PRIVATE
    ${SBM_SRC_DIR}/PluginProcessor.cpp
    ${SBM_SRC_DIR}/PluginProcessor.h
    ${SBM_SRC_DIR}/PluginEditor.cpp
    ${SBM_SRC_DIR}/PluginEditor.h
    ${SBM_SRC_DIR}/Dsp.h
    ${SBM_SRC_DIR}/Genres.h)

target_include_directories(SevenBandMaster PRIVATE ${SBM_SRC_DIR})

target_compile_definitions(SevenBandMaster PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(SevenBandMaster
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
