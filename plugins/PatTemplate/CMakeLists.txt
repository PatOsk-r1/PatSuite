# PatTemplate: empty phase-1 plugin that exercises the shared look and infrastructure.
# Not part of the installer.
juce_add_plugin(PatTemplate
    PRODUCT_NAME "PatTemplate"
    COMPANY_NAME "PatSuite"
    BUNDLE_ID "com.patsuite.template"
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Ptst
    PLUGIN_CODE Ptpl
    FORMATS ${PAT_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Tools
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(PatTemplate PRIVATE
    Processor.cpp
    ${CMAKE_SOURCE_DIR}/ui/template/TemplateEditor.cpp)
pat_plugin_setup(PatTemplate)
