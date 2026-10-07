cmake_minimum_required(VERSION 3.22)

project(MUTAGEN VERSION 0.2.0 LANGUAGES C CXX)

# Static MSVC runtime: the Windows standalone .exe runs without a VC++ redistributable.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

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
    # Every plugin in the range carries its own mark: MUTAGEN's is the petri
    # dish and colony in Resources/icon.png, cut by Resources/make_icon.py.
    ICON_BIG                  "${CMAKE_CURRENT_SOURCE_DIR}/Resources/icon.png"
    PRODUCT_NAME             "MUTAGEN")

juce_generate_juce_header(MUTAGEN)

# ---------------------------------------------------------------------------
# CLAP, via clap-juce-extensions (JUCE 8 has no native CLAP wrapper).
#   cmake -B build -DCLAP_JUCE_EXTENSIONS_DIR=/path/to/checkout  to skip the fetch
# ---------------------------------------------------------------------------
option(MUTAGEN_BUILD_CLAP "Build the CLAP plugin" ON)
set(MUTAGEN_CLAP_JUCE_EXT_TAG "7adee3a1bd4684d4caa5601100e364abccff4b4d" CACHE STRING
    "clap-juce-extensions commit to fetch")

if(MUTAGEN_BUILD_CLAP)
    if(DEFINED CLAP_JUCE_EXTENSIONS_DIR AND EXISTS "${CLAP_JUCE_EXTENSIONS_DIR}/CMakeLists.txt")
        add_subdirectory("${CLAP_JUCE_EXTENSIONS_DIR}" "${CMAKE_BINARY_DIR}/clap-juce-extensions" EXCLUDE_FROM_ALL)
    else()
        FetchContent_Declare(
            clap-juce-extensions
            GIT_REPOSITORY https://github.com/free-audio/clap-juce-extensions.git
            GIT_TAG        ${MUTAGEN_CLAP_JUCE_EXT_TAG}
            GIT_SUBMODULES_RECURSE TRUE)
        FetchContent_MakeAvailable(clap-juce-extensions)
    endif()

    clap_juce_extensions_plugin(TARGET MUTAGEN
        CLAP_ID "com.colonyworks.mutagen"
        CLAP_FEATURES instrument synthesizer audio-effect stereo)
endif()

target_sources(MUTAGEN PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/LabGameFlow.cpp
    Source/Parameters.cpp
    Source/ParamHelp.cpp

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
    Source/Engine/ProgressionSystem.cpp
    Source/Engine/PhysicalOddities.cpp
    Source/Engine/Ingest.cpp
    Source/Engine/PresetManager.cpp
    Source/Engine/SoundCollection.cpp

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
    Source/GUI/TopBar.cpp
    Source/GUI/ParamControl.cpp
    Source/GUI/HelpView.cpp
    Source/GUI/OptionsView.cpp
    Source/GUI/ProgressionView.cpp
    Source/GUI/HauntedOverlay.cpp
    Source/GUI/LabGameOverlay.cpp
    Source/GUI/StoryPanel.cpp
    Source/GUI/SecretLayer.cpp
    Source/GUI/IntroOverlay.cpp
    Source/GUI/DeviceSetupHook.cpp)

target_include_directories(MUTAGEN PRIVATE Source)

# The options page shows the audio/MIDI device selector only where there is a
# device manager to show - i.e. in the standalone app. The shared code cannot
# see it (it is compiled once, with JucePlugin_Build_Standalone = 0), so the
# standalone target compiles one extra file that installs a factory into
# GUI/DeviceSetupHook.h. In a plugin build that factory stays empty and the
# page says the host owns the soundcard, which there it does.
if(MUTAGEN_BUILD_STANDALONE AND TARGET MUTAGEN_Standalone)
    target_sources(MUTAGEN_Standalone PRIVATE Source/Standalone/DeviceSetup.cpp)
    target_include_directories(MUTAGEN_Standalone PRIVATE Source)
endif()

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

if(WIN32)
    target_link_libraries(MUTAGEN PRIVATE winmm)
endif()

# NB: link-time optimisation (juce_recommended_lto_flags) is deliberately not
# used - it triggers an MSVC internal compiler error in juce_audio_utils on
# some toolchain versions and buys little here.

if(MSVC)
    # /utf-8: the polyglot events carry Japanese, Hindi, Korean and Icelandic text.
    target_compile_options(MUTAGEN PRIVATE /fp:fast /utf-8)
else()
    target_compile_options(MUTAGEN PRIVATE -ffast-math)
endif()

# ---------------------------------------------------------------------------
# Headless divergence test
#
# The overhaul exists to answer "they all evolve to sound almost the same".
# That is measurable, so this target measures it: several colonies run offline
# with no user input, and the report says how far apart they end up, whether
# each one keeps moving, and whether any of them parks in noise.
# ---------------------------------------------------------------------------
option(MUTAGEN_BUILD_TESTS "Build the headless divergence test" ON)

if(MUTAGEN_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(MutagenDivergenceTest PRODUCT_NAME "MutagenDivergenceTest")

    target_sources(MutagenDivergenceTest PRIVATE
        Tests/DivergenceTest.cpp
        Source/Parameters.cpp
        Source/Engine/Entropy.cpp
        Source/Engine/Genome.cpp
        Source/Engine/Cells.cpp
        Source/Engine/SourceAnalyzer.cpp
        Source/Engine/Colony.cpp
        Source/Engine/Descriptors.cpp
        # The reward check drives a real ScoreSystem off a real snapshot, so
        # "rare" is a number this test prints rather than something a human
        # has to sit and wait for.
        Source/Engine/ScoreSystem.cpp)

    target_include_directories(MutagenDivergenceTest PRIVATE Source)

    target_compile_definitions(MutagenDivergenceTest PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
        JUCE_REPORT_APP_USAGE=0
        _CRT_SECURE_NO_WARNINGS=1)

    target_link_libraries(MutagenDivergenceTest PRIVATE
        juce::juce_audio_formats
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_gui_basics
        PUBLIC
        juce::juce_recommended_config_flags)

    if(MSVC)
        target_compile_options(MutagenDivergenceTest PRIVATE /fp:fast)
    else()
        target_compile_options(MutagenDivergenceTest PRIVATE -ffast-math)
    endif()

    # Small GUI regression target. This stays offscreen and checks that the
    # floating score HUD only captures clicks in its own interactive regions;
    # the rest of the culture chamber must remain playable underneath it.
    juce_add_console_app(MutagenGuiSmokeTest PRODUCT_NAME "MutagenGuiSmokeTest")

    target_sources(MutagenGuiSmokeTest PRIVATE
        Tests/GuiSmokeTest.cpp
        Source/GUI/ScoreHud.cpp
        Source/Engine/ScoreSystem.cpp)

    target_include_directories(MutagenGuiSmokeTest PRIVATE Source)

    target_compile_definitions(MutagenGuiSmokeTest PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
        JUCE_REPORT_APP_USAGE=0
        _CRT_SECURE_NO_WARNINGS=1)

    target_link_libraries(MutagenGuiSmokeTest PRIVATE
        juce::juce_gui_extra
        PUBLIC
        juce::juce_recommended_config_flags)

    add_test(NAME MutagenGuiSmokeTest COMMAND MutagenGuiSmokeTest)

    juce_add_console_app(MutagenSoundCollectionTest PRODUCT_NAME "MutagenSoundCollectionTest")
    target_sources(MutagenSoundCollectionTest PRIVATE
        Tests/SoundCollectionTest.cpp
        Source/Engine/SoundCollection.cpp)
    target_include_directories(MutagenSoundCollectionTest PRIVATE Source)
    target_compile_definitions(MutagenSoundCollectionTest PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_STANDALONE_APPLICATION=1
        JUCE_REPORT_APP_USAGE=0
        _CRT_SECURE_NO_WARNINGS=1)
    target_link_libraries(MutagenSoundCollectionTest PRIVATE
        juce::juce_audio_formats
        PUBLIC
        juce::juce_recommended_config_flags)
    add_test(NAME MutagenSoundCollectionTest COMMAND MutagenSoundCollectionTest)

    add_executable(MutagenProgressionTest Tests/ProgressionTest.cpp)
    target_include_directories(MutagenProgressionTest PRIVATE Source)
    add_test(NAME MutagenProgressionTest COMMAND MutagenProgressionTest)
endif()
