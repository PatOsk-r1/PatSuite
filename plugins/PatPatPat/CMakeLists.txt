# PatPatPat: stereo / ping-pong delay.
juce_add_plugin(PatPatPat
    PRODUCT_NAME "PatPatPat"
    COMPANY_NAME "PatSuite"
    BUNDLE_ID "com.patsuite.patpatpat"
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Ptst
    PLUGIN_CODE Ppp3
    FORMATS ${PAT_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Delay
    COPY_PLUGIN_AFTER_BUILD ${PATSUITE_COPY_VST3}
    VST3_COPY_DIR "${PATSUITE_VST3_DIR}")

target_sources(PatPatPat PRIVATE
    Processor.cpp
    Presets.cpp
    ${CMAKE_SOURCE_DIR}/ui/delay/PatPatPatEditor.cpp)
pat_plugin_setup(PatPatPat)
