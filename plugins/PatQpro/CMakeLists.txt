# PatQpro: pro equalizer (24 bands, dynamic, linear phase, match, collisions).
juce_add_plugin(PatQpro
    PRODUCT_NAME "PatQpro"
    COMPANY_NAME "PatSuite"
    BUNDLE_ID "com.patsuite.patqpro"
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Ptst
    PLUGIN_CODE Pqpr
    FORMATS ${PAT_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx EQ
    COPY_PLUGIN_AFTER_BUILD ${PATSUITE_COPY_VST3}
    VST3_COPY_DIR "${PATSUITE_VST3_DIR}")

target_sources(PatQpro PRIVATE
    Processor.cpp
    Presets.cpp
    ${CMAKE_SOURCE_DIR}/ui/eq/QproEditor.cpp)
pat_plugin_setup(PatQpro)
