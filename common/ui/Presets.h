// Preset system shared by every PatSuite plugin.
//
//   - Factory presets are data in each plugin (name, category path such as "Vocals/Rap" or
//     "Producers/Pat Osk", and the parameter values that differ from the defaults).
//   - User presets are XML files in Documents\PatSuite\<Plugin>\Presets (sub-folders become
//     categories). Presets shared inside the plugin (BinaryData) appear as factory presets.
//   - Extra state that is not a parameter (a drawn curve, a custom scale) travels as "extras":
//     the plugin provides callbacks to write and read them.
//   - The current preset name is stored in the plugin state, so a FL Studio project remembers it.
#pragma once

#include "common/ui/Theme.h"

#include <juce_audio_processors/juce_audio_processors.h>

#include <functional>
#include <vector>

namespace pat::ui {

struct FactoryPreset {
    juce::String name, category;
    std::vector<std::pair<juce::String, float>> values;  // parameter ID -> value (real units)
    std::vector<std::pair<juce::String, juce::String>> extras = {};
};

class PresetLibrary {
public:
    struct Entry {
        juce::String name, category;
        int factory = -1;      // index in the factory list, or -1
        juce::File file;       // user preset
        juce::String embedded; // XML of a shared preset built into the plugin
    };

    PresetLibrary(juce::AudioProcessorValueTreeState& state, const juce::String& pluginName, std::vector<FactoryPreset> factory);

    // Extras: write the plugin's non-parameter state into a preset / read it back.
    std::function<void(juce::XmlElement& preset)> saveExtras;
    std::function<void(const juce::String& key, const juce::String& value)> loadExtra;

    void rescan();
    const std::vector<Entry>& entries() const noexcept { return entries_; }
    int numFactory() const noexcept { return numFactory_; }
    bool load(int index);
    void step(int delta);
    bool saveUser(const juce::String& name);
    bool deleteUser(int index);
    int currentIndex() const;
    juce::String currentName() const;
    juce::File userDirectory() const;
    // Development check: factory presets that name unknown parameters or values out of range.
    juce::StringArray validate() const;

private:
    void apply(const juce::XmlElement& preset);
    void resetToDefaults();
    void setCurrent(const juce::String& name);

    juce::AudioProcessorValueTreeState& state_;
    juce::String plugin_;
    std::vector<FactoryPreset> factory_;
    std::vector<std::pair<juce::String, juce::String>> embedded_;  // name, xml
    std::vector<Entry> entries_;
    int numFactory_ = 0;
};

// [<]  [ preset name  v ]  [>]  [SAVE]  for the editor header.
class PresetBar final : public juce::Component {
public:
    PresetBar(PresetLibrary& lib, juce::Colour accent);
    void resized() override;
    void refresh();  // call from the editor clock: follows preset changes

private:
    void showMenu();
    void save();
    PresetLibrary& lib_;
    juce::TextButton prev_{"<"}, next_{">"}, name_, save_{"SAVE"};
};

}  // namespace pat::ui
