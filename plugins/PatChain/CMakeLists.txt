# PatChain: tempo or kick-driven ducking (phase 3).
juce_add_plugin(PatChain
    PRODUCT_NAME "PatChain"
    COMPANY_NAME "PatSuite"
    BUNDLE_ID "com.patsuite.patchain"
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Ptst
    PLUGIN_CODE Psdc
    FORMATS VST3 Standalone
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Dynamics
    COPY_PLUGIN_AFTER_BUILD ${PATSUITE_COPY_VST3}
    VST3_COPY_DIR "${PATSUITE_VST3_DIR}")

target_sources(PatChain PRIVATE
    Processor.cpp
    Presets.cpp
    ${CMAKE_SOURCE_DIR}/ui/sidechain/SidechainEditor.cpp)
pat_plugin_setup(PatChain)
