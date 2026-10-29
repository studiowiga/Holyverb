cmake_minimum_required(VERSION 3.22)
project(Holyverb VERSION 1.4.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows : runtime C++ statique (le VST3 fonctionne sans installer de redistribuable Visual C++)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE : utilise un dossier local si JUCE_DIR est fourni, sinon téléchargement automatique
set(JUCE_DIR "" CACHE PATH "Chemin vers une copie locale de JUCE (optionnel)")
if(JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

juce_add_plugin(Holyverb
    COMPANY_NAME "Studio Wiga"
    PLUGIN_MANUFACTURER_CODE Wiga
    PLUGIN_CODE Hlyv
    BUNDLE_ID com.studiowiga.holyverb
    FORMATS VST3
    PRODUCT_NAME "Holyverb"
    VERSION 1.4.0
    DESCRIPTION "Reverb 8 modes - format pedale"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Reverb)

juce_add_binary_data(HolyverbAssets SOURCES
    Assets/background.jpg
    Assets/knob_strip.png
    Assets/mode_strip.png
    Assets/toggle_strip.png
    Assets/logo_studio_wiga.png
    Assets/HolyverbSans.ttf)

target_sources(Holyverb PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

# Sources en UTF-8 (commentaires accentués)
if(MSVC)
    target_compile_options(Holyverb PRIVATE /utf-8)
endif()

target_compile_definitions(Holyverb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(Holyverb
    PRIVATE
        HolyverbAssets
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)



