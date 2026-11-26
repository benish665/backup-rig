cmake_minimum_required(VERSION 3.22)
project(BackupRig VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "")

# JUCE: use a local checkout if JUCE_DIR is given, otherwise download it
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.15 GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(RIG_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND RIG_FORMATS AU)
endif()

juce_add_plugin(BackupRig
    COMPANY_NAME "Backup Rig"
    BUNDLE_ID com.backuprig.plugin
    PLUGIN_MANUFACTURER_CODE Bkrg
    PLUGIN_CODE Brig
    FORMATS ${RIG_FORMATS}
    PRODUCT_NAME "Backup Rig"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Backup Rig needs audio input to process your guitar."
    VST3_CATEGORIES Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

juce_add_binary_data(BackupRigFonts SOURCES
    Fonts/Heebo.ttf
    Fonts/BarlowCondensed-SemiBold.ttf
    Fonts/BarlowCondensed-Bold.ttf
    Fonts/Yellowtail-Regular.ttf
    Fonts/JosefinSans.ttf
    Fonts/ArchivoBlack-Regular.ttf
    Fonts/Michroma-Regular.ttf)
set_target_properties(BackupRigFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(BackupRig PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)

target_compile_definitions(BackupRig PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
    target_compile_options(BackupRig PRIVATE /utf-8)
endif()
if(MINGW)
    target_link_options(BackupRig PRIVATE -static -static-libgcc -static-libstdc++)
endif()

target_link_libraries(BackupRig
    PRIVATE BackupRigFonts juce::juce_audio_utils juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

option(RIG_BUILD_TESTS "Build the offline render test" OFF)
if(RIG_BUILD_TESTS)
    juce_add_console_app(RigRender PRODUCT_NAME "RigRender")
    target_sources(RigRender PRIVATE tests/render.cpp Source/PluginProcessor.cpp)
    target_include_directories(RigRender PRIVATE Source)
    target_compile_definitions(RigRender PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 RIG_HEADLESS=1 JucePlugin_Name="Backup Rig")
    target_link_libraries(RigRender PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
if(RIG_BUILD_TESTS)
    juce_add_gui_app(RigSnap PRODUCT_NAME "RigSnap")
    target_sources(RigSnap PRIVATE tests/snap.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_include_directories(RigSnap PRIVATE Source)
    target_compile_definitions(RigSnap PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="Backup Rig")
    target_link_libraries(RigSnap PRIVATE BackupRigFonts juce::juce_audio_utils juce::juce_dsp juce::juce_recommended_config_flags)
endif()
if(RIG_BUILD_TESTS)
    juce_add_console_app(RigBidi PRODUCT_NAME "RigBidi")
    target_sources(RigBidi PRIVATE tests/bidi.cpp)
    target_link_libraries(RigBidi PRIVATE BackupRigFonts juce::juce_gui_basics juce::juce_recommended_config_flags)
endif()
if(RIG_BUILD_TESTS)
    juce_add_console_app(RigHost PRODUCT_NAME "RigHost")
    target_sources(RigHost PRIVATE tests/host.cpp)
    target_compile_definitions(RigHost PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(RigHost PRIVATE juce::juce_audio_processors juce::juce_recommended_config_flags)
endif()
