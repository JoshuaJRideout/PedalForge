cmake_minimum_required(VERSION 3.22)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
project(PedalForge VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── Default Build Type ───────────────────────────────────────────
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  message(STATUS "Setting build type to 'Release' as none was specified.")
  set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()


# ─── Fetch JUCE ────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

# ─── Fetch NAM Core ────────────────────────────────────────────────
FetchContent_Declare(
    NAMCore
    GIT_REPOSITORY https://github.com/sdatkinson/NeuralAmpModelerCore.git
    GIT_TAG        v0.5.2
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(NAMCore)

file(GLOB_RECURSE NAM_SOURCES "${namcore_SOURCE_DIR}/NAM/*.cpp")

# ─── FAUST Compilation ────────────────────────────────────────────
include(cmake/FaustCompile.cmake)

# ─── Define the plugin ─────────────────────────────────────────────
juce_add_plugin(PedalForge
    PRODUCT_NAME            "PedalForge"
    COMPANY_NAME            "PedalForge"
    BUNDLE_ID               "com.pedalforge.app"
    PLUGIN_MANUFACTURER_CODE JshF
    PLUGIN_CODE              PdFg
    FORMATS                  AU VST3 Standalone
    IS_SYNTH                 FALSE
    NEEDS_MIDI_INPUT         TRUE
    NEEDS_MIDI_OUTPUT        FALSE
    IS_MIDI_EFFECT           FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD  TRUE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "PedalForge needs audio input to process your instrument signal."
)

# ─── Source files ──────────────────────────────────────────────────
target_sources(PedalForge PRIVATE
    # Core
    source/PluginProcessor.h
    source/PluginProcessor.cpp
    source/PluginEditor.h
    source/PluginEditor.cpp
    source/CustomStandaloneApp.cpp

    # Engine
    source/engine/AudioGraphEngine.h
    source/engine/AudioGraphEngine.cpp
    source/engine/FaustPedal.h
    source/engine/FaustPedal.cpp
    source/engine/PedalInstance.h
    source/engine/FaustTypes.h

    # Pedals
    source/pedals/PedalRegistry.h
    source/pedals/FactoryDesigns.h

    # UI
    source/ui/AutocompletePanelComponent.h
    source/ui/AutocompletePanelComponent.cpp
    source/ui/LookAndFeel.h
    source/ui/LookAndFeel.cpp
    source/ui/PedalboardGrid.h
    source/ui/PedalboardGrid.cpp
    source/ui/BoardComponent.h
    source/ui/BoardComponent.cpp
    source/ui/BoardWindow.cpp
    source/ui/PlayTabComponent.h
    source/ui/PlayTabComponent.cpp
    source/ui/TuringRenderer.cpp
    source/ui/MidiSettingsPanel.cpp
    source/ui/ToastOverlay.h
    source/ui/ToastOverlay.cpp
    source/ui/AudioStatusBar.h
    source/ui/AudioStatusBar.cpp
    source/util/AppPaths.h
    source/util/SnapshotManager.h
    source/util/SnapshotManager.cpp

    # AI assistant (#64)
    source/ai/AiTypes.h
    source/ai/AiProvider.h
    source/ai/ToolHost.h
    source/ai/SecureStore.h
    source/ai/SecureStore.cpp
    source/ai/AiTools.h
    source/ai/AiTools.cpp
    source/ai/ClaudeProvider.h
    source/ai/ClaudeProvider.cpp
    source/ai/ClaudeCodeProvider.h
    source/ai/ClaudeCodeProvider.cpp
    source/ai/AiAgent.h
    source/ai/AiAgent.cpp
    source/ui/AiAssistantPanel.h
    source/ui/AiAssistantPanel.cpp

    # Secondary display peripherals (Turing 3.5" V2 and future HDMI/etc.)
    source/peripherals/displays/SecondaryDisplay.h
    source/peripherals/displays/DisplayMode.h
    source/peripherals/displays/DisplayManager.h
    source/peripherals/displays/DisplayManager.cpp
    source/peripherals/displays/TuringDisplay.h
    source/peripherals/displays/TuringDisplay.cpp
    source/peripherals/displays/modes/MidiMonitorMode.h
    source/peripherals/displays/modes/MidiMonitorMode.cpp
    source/ui/BoardCanvas.h
    source/ui/BoardCanvas.cpp
    source/ui/PedalComponent.h
    source/ui/PedalComponent.cpp
    source/ui/RouteOverlay.h
    source/ui/RouteOverlay.cpp
    source/ui/LibraryComponent.cpp
    source/ui/PedalDetailPanel.h
    source/ui/PedalDetailPanel.cpp
    source/ui/PedalPalette.h
    source/ui/PedalPalette.cpp
    source/ui/InventoryOverlay.h
    source/ui/InventoryOverlay.cpp
    source/ui/PedalPainter.h
    source/ui/PedalPainter.cpp
    source/ui/PedalDesignerComponent.h
    source/ui/PedalDesignerComponent.cpp
    source/ui/HardwareDrawing.h
    source/ui/NodeGraphEditor.h
    source/ui/NodeGraphEditor.cpp
    source/ui/RoutingGraphEditor.h
    source/ui/RoutingGraphEditor.cpp

    # DSP Node Graph Engine
    source/dsp/DSPNode.h
    source/dsp/DSPNodeLibrary.h
    source/dsp/DSPGraph.h
    source/dsp/GraphPedalProcessor.h
    source/dsp/FaustDSPNode.h
    source/dsp/PedalDesign.h
    source/dsp/PluginHostNode.h

    # Presets
    source/preset/PresetManager.h
    source/preset/PresetManager.cpp
    source/preset/PresetBrowser.h
    source/preset/PresetBrowser.cpp

    # MIDI
    source/midi/MidiLearn.h
    source/midi/MidiLearn.cpp

    # Network
    source/dsp/ControlSurfaceSync.h
    source/dsp/NodeCatalog.h
    source/network/Tone3000Client.h
    source/network/Tone3000Client.cpp
    source/network/Tone3000OAuth.h
    source/network/Tone3000OAuth.cpp

    # NAM
    ${NAM_SOURCES}
)

# ─── Compile FAUST DSP files ──────────────────────────────────────
set(FAUST_ARCH "${CMAKE_CURRENT_SOURCE_DIR}/faust/architecture/pedalforge.arch")
set(FAUST_OUT  "${CMAKE_CURRENT_SOURCE_DIR}/faust/generated")

faust_compile(DSP_FILE faust/factory/overdrive.dsp
              OUTPUT_DIR ${FAUST_OUT} ARCH_FILE ${FAUST_ARCH}
              TARGET PedalForge)

faust_compile(DSP_FILE faust/factory/delay.dsp
              OUTPUT_DIR ${FAUST_OUT} ARCH_FILE ${FAUST_ARCH}
              TARGET PedalForge)

faust_compile(DSP_FILE faust/factory/reverb.dsp
              OUTPUT_DIR ${FAUST_OUT} ARCH_FILE ${FAUST_ARCH}
              TARGET PedalForge)

faust_compile(DSP_FILE faust/factory/eq.dsp
              OUTPUT_DIR ${FAUST_OUT} ARCH_FILE ${FAUST_ARCH}
              TARGET PedalForge)

faust_compile(DSP_FILE faust/factory/compressor.dsp
              OUTPUT_DIR ${FAUST_OUT} ARCH_FILE ${FAUST_ARCH}
              TARGET PedalForge)

# ─── Include path for generated FAUST headers ─────────────────────
target_include_directories(PedalForge PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${namcore_SOURCE_DIR}
    ${namcore_SOURCE_DIR}/Dependencies/eigen
    ${namcore_SOURCE_DIR}/Dependencies/nlohmann
)

# ─── Compile definitions ──────────────────────────────────────────
target_compile_definitions(PedalForge PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    NAM_SAMPLE_FLOAT=1
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1
    JUCE_USE_CUSTOM_PLUGIN_STANDALONE_APP=1
)

# ─── Link libraries ───────────────────────────────────────────────
target_link_libraries(PedalForge
    PRIVATE
        juce::juce_audio_utils
        juce::juce_cryptography
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)

# ─── Wiki reference generator ────────────────────────────────────
# Tiny tool that regenerates docs/wiki/reference/expression-vm-functions.md from
# ExpressionVM::dumpFunctionsAsMarkdown(). Run on demand with:
#   cmake --build build --target wiki-reference
# Not tied to the default build to avoid touching the docs file on every compile.
add_executable(dump_wiki_reference EXCLUDE_FROM_ALL tools/dump_wiki_reference.cpp)
target_link_libraries(dump_wiki_reference
    PRIVATE
        juce::juce_core
        juce::juce_gui_basics
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags
)
target_compile_definitions(dump_wiki_reference PRIVATE
    JUCE_STANDALONE_APPLICATION=1
    JUCE_USE_CURL=0
)

add_custom_target(wiki-reference
    COMMAND dump_wiki_reference "${CMAKE_SOURCE_DIR}/docs/wiki/reference/expression-vm-functions.md"
    DEPENDS dump_wiki_reference
    COMMENT "Regenerating docs/wiki/reference/expression-vm-functions.md from ExpressionVM registry"
    VERBATIM
)

# ─── UTF-8 literal guard ───────────────────────────────────────────
# Prevents regressions of the mojibake bug class: juce::String(const char*)
# decodes as ASCII/Latin-1, so a bare UTF-8 literal like "…" renders as "â€¦".
# The guard FAILS the build only on NEW un-wrapped non-ASCII literals; the
# pre-existing debt is baselined in cmake/utf8_baseline.txt (burn it down and
# regenerate with --write-baseline). Skipped silently if Python isn't found.
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_FOUND)
    add_custom_target(utf8-guard ALL
        COMMAND ${Python3_EXECUTABLE}
                "${CMAKE_SOURCE_DIR}/cmake/check_utf8_literals.py"
                source
                --baseline cmake/utf8_baseline.txt
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"   # relative paths must match baseline sigs
        COMMENT "Checking for un-wrapped non-ASCII string literals (mojibake guard)"
        VERBATIM
    )
    add_dependencies(PedalForge utf8-guard)
else()
    message(STATUS "Python3 not found — skipping UTF-8 literal guard.")
endif()
