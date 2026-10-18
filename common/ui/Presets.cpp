#include "common/ui/Presets.h"

#include <PresetData.h>  // presets shared inside this plugin (tools/collect-presets.ps1)

#include <map>

namespace pat::ui {

PresetLibrary::PresetLibrary(juce::AudioProcessorValueTreeState& state, const juce::String& pluginName, std::vector<FactoryPreset> factory)
    : state_(state), plugin_(pluginName), factory_(std::move(factory)) {
    for (int i = 0; i < PresetData::namedResourceListSize; ++i) {
        const juce::String original(PresetData::originalFilenames[i]);
        if (!original.endsWithIgnoreCase(".patpreset")) continue;
        int size = 0;
        if (const char* data = PresetData::getNamedResource(PresetData::namedResourceList[i], size))
            embedded_.push_back({original.upToLastOccurrenceOf(".", false, false), juce::String::fromUTF8(data, size)});
    }
    rescan();
}

juce::File PresetLibrary::userDirectory() const {
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("PatSuite").getChildFile(plugin_).getChildFile("Presets");
}

void PresetLibrary::rescan() {
    entries_.clear();
    for (int i = 0; i < static_cast<int>(factory_.size()); ++i) {
        Entry e;
        e.name = factory_[static_cast<std::size_t>(i)].name;
        e.category = factory_[static_cast<std::size_t>(i)].category;
        e.factory = i;
        entries_.push_back(e);
    }
    const auto dir = userDirectory();
    auto files = dir.findChildFiles(juce::File::findFiles, true, "*.patpreset");
    std::sort(files.begin(), files.end(), [&dir](const juce::File& a, const juce::File& b) {
        return a.getRelativePathFrom(dir).compareNatural(b.getRelativePathFrom(dir)) < 0;
    });
    juce::StringArray userNames;
    for (const auto& f : files) userNames.add(f.getFileNameWithoutExtension());
    for (const auto& [name, xml] : embedded_) {
        auto el = juce::XmlDocument::parse(xml);
        if (!el) continue;
        Entry e;
        e.name = el->getStringAttribute("name", name);
        if (userNames.contains(e.name)) continue;  // the author's own copy wins
        e.category = el->getStringAttribute("category", "Community");
        e.embedded = xml;
        entries_.push_back(e);
    }
    numFactory_ = static_cast<int>(entries_.size());
    for (const auto& f : files) {
        Entry e;
        e.name = f.getFileNameWithoutExtension();
        e.category = "User";
        if (f.getParentDirectory() != dir) e.category << "/" << f.getParentDirectory().getRelativePathFrom(dir).replaceCharacter('\\', '/');
        e.file = f;
        entries_.push_back(e);
    }
}

void PresetLibrary::resetToDefaults() {
    for (auto* p : state_.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p)) rp->setValueNotifyingHost(rp->getDefaultValue());
}

void PresetLibrary::apply(const juce::XmlElement& preset) {
    resetToDefaults();
    for (auto* el : preset.getChildWithTagNameIterator("Param"))
        if (auto* p = state_.getParameter(el->getStringAttribute("id")))
            p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(el->getDoubleAttribute("value"))));
    for (auto* el : preset.getChildWithTagNameIterator("Extra"))
        if (loadExtra) loadExtra(el->getStringAttribute("key"), el->getStringAttribute("value"));
}

bool PresetLibrary::load(int index) {
    if (index < 0 || index >= static_cast<int>(entries_.size())) return false;
    const auto& e = entries_[static_cast<std::size_t>(index)];
    if (e.factory >= 0) {
        juce::XmlElement x("PatPreset");
        const auto& f = factory_[static_cast<std::size_t>(e.factory)];
        for (const auto& [id, v] : f.values) {
            auto* p = x.createNewChildElement("Param");
            p->setAttribute("id", id);
            p->setAttribute("value", v);
        }
        for (const auto& [k, v] : f.extras) {
            auto* p = x.createNewChildElement("Extra");
            p->setAttribute("key", k);
            p->setAttribute("value", v);
        }
        apply(x);
    } else {
        std::unique_ptr<juce::XmlElement> x = e.embedded.isNotEmpty() ? juce::XmlDocument::parse(e.embedded) : juce::XmlDocument::parse(e.file);
        if (!x || !x->hasTagName("PatPreset")) return false;
        apply(*x);
    }
    setCurrent(e.name);
    return true;
}

void PresetLibrary::step(int delta) {
    const int n = static_cast<int>(entries_.size());
    if (n == 0) return;
    const int cur = currentIndex();
    load(((cur < 0 ? 0 : cur) + delta + n) % n);
}

bool PresetLibrary::saveUser(const juce::String& rawName) {
    const auto name = juce::File::createLegalFileName(rawName.trim());
    if (name.isEmpty() || !userDirectory().createDirectory()) return false;
    juce::XmlElement x("PatPreset");
    x.setAttribute("plugin", plugin_);
    x.setAttribute("name", name);
    for (auto* p : state_.processor.getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p)) {
            auto* el = x.createNewChildElement("Param");
            el->setAttribute("id", rp->getParameterID());
            el->setAttribute("value", rp->convertFrom0to1(rp->getValue()));
        }
    if (saveExtras) saveExtras(x);
    if (!x.writeTo(userDirectory().getChildFile(name + ".patpreset"))) return false;
    rescan();
    setCurrent(name);
    return true;
}

bool PresetLibrary::deleteUser(int index) {
    if (index < numFactory_ || index >= static_cast<int>(entries_.size())) return false;
    const bool ok = entries_[static_cast<std::size_t>(index)].file.moveToTrash();
    rescan();
    return ok;
}

void PresetLibrary::setCurrent(const juce::String& name) { state_.state.setProperty("presetName", name, nullptr); }

juce::String PresetLibrary::currentName() const { return state_.state.getProperty("presetName", "Default").toString(); }

int PresetLibrary::currentIndex() const {
    const auto name = currentName();
    for (int i = 0; i < static_cast<int>(entries_.size()); ++i)
        if (entries_[static_cast<std::size_t>(i)].name == name) return i;
    return -1;
}

juce::StringArray PresetLibrary::validate() const {
    juce::StringArray problems;
    juce::StringArray names;
    for (const auto& f : factory_) {
        if (names.contains(f.category + "/" + f.name)) problems.add("duplicate: " + f.category + "/" + f.name);
        names.add(f.category + "/" + f.name);
        for (const auto& [id, v] : f.values) {
            auto* p = state_.getParameter(id);
            if (!p) problems.add(f.name + ": unknown parameter " + id);
            else if (v < p->getNormalisableRange().start - 1.0e-4f || v > p->getNormalisableRange().end + 1.0e-4f)
                problems.add(f.name + ": " + id + " out of range (" + juce::String(v) + ")");
        }
    }
    return problems;
}

// ------------------------------------------------------------------------------------------- bar

PresetBar::PresetBar(PresetLibrary& lib, juce::Colour accent) : lib_(lib) {
    for (auto* b : {&prev_, &next_, &name_, &save_}) {
        b->setColour(juce::TextButton::buttonOnColourId, accent);
        b->setWantsKeyboardFocus(false);
        b->setMouseClickGrabsKeyboardFocus(false);
        addAndMakeVisible(b);
    }
    prev_.onClick = [this] {
        lib_.step(-1);
        refresh();
    };
    next_.onClick = [this] {
        lib_.step(1);
        refresh();
    };
    name_.onClick = [this] { showMenu(); };
    save_.onClick = [this] { save(); };
    name_.setTooltip("Browse presets");
    save_.setTooltip("Save the current settings as your own preset");
    refresh();
}

void PresetBar::resized() {
    auto r = getLocalBounds();
    save_.setBounds(r.removeFromRight(52));
    r.removeFromRight(4);
    prev_.setBounds(r.removeFromLeft(26));
    next_.setBounds(r.removeFromRight(26));
    name_.setBounds(r.reduced(3, 0));
}

void PresetBar::refresh() {
    const auto n = lib_.currentName();
    if (name_.getButtonText() != n) name_.setButtonText(n);
}

void PresetBar::showMenu() {
    lib_.rescan();
    struct Node {
        std::map<juce::String, Node> kids;
        juce::StringArray order;
        juce::Array<int> items;
    };
    Node root;
    const auto& e = lib_.entries();
    for (int i = 0; i < static_cast<int>(e.size()); ++i) {
        Node* n = &root;
        for (const auto& part : juce::StringArray::fromTokens(e[static_cast<std::size_t>(i)].category, "/", ""))
            if (part.isNotEmpty()) {
                if (!n->kids.count(part)) n->order.add(part);
                n = &n->kids[part];
            }
        n->items.add(i);
    }
    const int current = lib_.currentIndex();
    std::function<juce::PopupMenu(const Node&)> build = [&](const Node& n) {
        juce::PopupMenu m;
        for (const auto& k : n.order) m.addSubMenu(k, build(n.kids.at(k)));
        if (!n.order.isEmpty() && !n.items.isEmpty()) m.addSeparator();
        for (int i : n.items) m.addItem(1 + i, e[static_cast<std::size_t>(i)].name, true, i == current);
        return m;
    };
    juce::PopupMenu menu = build(root);
    menu.addSeparator();
    menu.addItem(100001, "Save as...");
    menu.addItem(100002, "Open presets folder");
    if (current >= lib_.numFactory()) menu.addItem(100003, "Delete \"" + lib_.currentName() + "\"");
    juce::Component::SafePointer<PresetBar> safe(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&name_), [safe](int r) {
        if (!safe || r == 0) return;
        auto& lib = safe->lib_;
        if (r == 100001) safe->save();
        else if (r == 100002) {
            lib.userDirectory().createDirectory();
            lib.userDirectory().startAsProcess();
        } else if (r == 100003) lib.deleteUser(lib.currentIndex());
        else lib.load(r - 1);
        safe->refresh();
    });
}

void PresetBar::save() {
    auto* w = new juce::AlertWindow("Save preset", "Name for the new preset:", juce::MessageBoxIconType::NoIcon, this);
    w->addTextEditor("name", lib_.currentName());
    w->addButton("Save", 1, juce::KeyPress(juce::KeyPress::returnKey));
    w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
    juce::Component::SafePointer<PresetBar> safe(this);
    w->enterModalState(true, juce::ModalCallbackFunction::create([safe, w](int result) {
        if (safe && result == 1) {
            if (!safe->lib_.saveUser(w->getTextEditorContents("name")))
                juce::AlertWindow::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon, "Save preset", "Could not save the preset.");
            safe->refresh();
        }
    }), true);
}

}  // namespace pat::ui
