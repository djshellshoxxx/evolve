cmake_minimum_required(VERSION 3.22)

project(MUTAGEN VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

option(MUTAGEN_BUILD_STANDALONE "Build the standalone application target" ON)
set(MUTAGEN_JUCE_TAG "8.0.6" CACHE STRING "JUCE git tag to fetch")

# ---------------------------------------------------------------------------
# JUCE
# ---------------------------------------------------------------------------
include(FetchContent)

# Allow pointing at a local JUCE checkout to avoid a network fetch:
#   cmake -B build -DJUCE_SOURCE_DIR=/path/to/JUCE
if(DEFINED JUCE_SOURCE_DIR AND EXISTS "${JUCE_SOURCE_DIR}/CMakeLists.txt")
    message(STATUS "MUTAGEN: using local JUCE at ${JUCE_SOURCE_DIR}")
    add_subdirectory("${JUCE_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/JUCE" EXCLUDE_FROM_ALL)
else()
    message(STATUS "MUTAGEN: fetching JUCE ${MUTAGEN_JUCE_TAG} via FetchContent")
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        ${MUTAGEN_JUCE_TAG}
        GIT_SHALLOW    TRUE
        GIT_PROGRESS   TRUE
    )
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Plugin target
# ---------------------------------------------------------------------------
set(MUTAGEN_FORMATS VST3 AU)
if(MUTAGEN_BUILD_STANDALONE)
    list(APPEND MUTAGEN_FORMATS Standalone)
endif()
if(NOT APPLE)
    list(REMOVE_ITEM MUTAGEN_FORMATS AU)
endif()

juce_add_plugin(MUTAGEN
    COMPANY_NAME              "Colonyworks"
    COMPANY_WEBSITE           "https://github.com/djshellshoxxx/mutagen"
    BUNDLE_ID                 com.colonyworks.mutagen
    PLUGIN_MANUFACTURER_CODE  Cwrx
    PLUGIN_CODE               Mutg
    FORMATS                   ${MUTAGEN_FORMATS}
    IS_SYNTH                  TRUE
    NEEDS_MIDI_INPUT          TRUE
    NEEDS_MIDI_OUTPUT         FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD   TRUE
    VST3_CATEGORIES           "Instrument" "Synth" "Fx"
    AU_MAIN_TYPE              kAudioUnitType_MusicDevice
    PRODUCT_NAME             "MUTAGEN")

juce_generate_juce_header(MUTAGEN)

target_sources(MUTAGEN PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Parameters.cpp

    Source/Engine/Entropy.cpp
    Source/Engine/Descriptors.cpp
    Source/Engine/Genome.cpp
    Source/Engine/Cells.cpp
    Source/Engine/SourceAnalyzer.cpp
    Source/Engine/Colony.cpp
    Source/Engine/EvolutionHistory.cpp
    Source/Engine/BreedingLab.cpp
    Source/Engine/OrganismSerialization.cpp
    Source/Engine/PostChain.cpp
    Source/Engine/RenderEngine.cpp
    Source/Engine/ScoreSystem.cpp
    Source/Engine/Ingest.cpp

    Source/GUI/MutagenLookAndFeel.cpp
    Source/GUI/Widgets.cpp
    Source/GUI/CultureChamber.cpp
    Source/GUI/WaveField.cpp
    Source/GUI/ScoreHud.cpp
    Source/GUI/GameBar.cpp
    Source/GUI/GerminationPanel.cpp
    Source/GUI/EnvironmentPanel.cpp
    Source/GUI/GenomeInspector.cpp
    Source/GUI/EvolutionTimeline.cpp
    Source/GUI/BreedingLabView.cpp
    Source/GUI/PerformanceView.cpp
    Source/GUI/FxRackView.cpp
    Source/GUI/TopBar.cpp)

target_include_directories(MUTAGEN PRIVATE Source)

target_compile_definitions(MUTAGEN PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_REPORT_APP_USAGE=0
    JUCE_STRICT_REFCOUNTEDPOINTER=1
    _CRT_SECURE_NO_WARNINGS=1)

target_link_libraries(MUTAGEN PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_extra
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

# NB: link-time optimisation (juce_recommended_lto_flags) is deliberately not
# used - it triggers an MSVC internal compiler error in juce_audio_utils on
# some toolchain versions and buys little here.

if(MSVC)
    target_compile_options(MUTAGEN PRIVATE /fp:fast)
else()
    target_compile_options(MUTAGEN PRIVATE -ffast-math)
endif()
