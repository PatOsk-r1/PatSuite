#include "common/ui/PerfSettings.h"

#include <juce_core/juce_core.h>

#if JUCE_WINDOWS
 #include <dxgi.h>
#endif

namespace pat::ui {

PerfSettings::PerfSettings() {
    detectHardware();
    juce::PropertiesFile::Options o;
    o.applicationName = "settings";
    o.filenameSuffix = ".xml";
    o.folderName = "PatSuite";
    o.osxLibrarySubFolder = "Application Support";
    o.storageFormat = juce::PropertiesFile::storeAsXML;
    file_ = std::make_unique<juce::PropertiesFile>(o);
    choice_ = static_cast<Choice>(juce::jlimit(0, 3, file_->getIntValue("perfLevel", 0)));
    neonFixed_ = static_cast<NeonFixed>(juce::jlimit(0, 2, file_->getIntValue("neonFixed", 0)));
}

void PerfSettings::detectHardware() {
    hw_.physicalCores = juce::SystemStats::getNumPhysicalCpus();
    hw_.memoryMB = juce::SystemStats::getMemorySizeInMegabytes();
    hw_.gpuMemoryMB = 0;
#if JUCE_WINDOWS
    // Largest dedicated video memory among hardware adapters (skips the software renderer).
    IDXGIFactory1* factory = nullptr;
    if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory1), reinterpret_cast<void**>(&factory))) && factory) {
        IDXGIAdapter1* adapter = nullptr;
        for (UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
            DXGI_ADAPTER_DESC1 desc{};
            if (SUCCEEDED(adapter->GetDesc1(&desc)) && !(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)) {
                const int mb = static_cast<int>(desc.DedicatedVideoMemory / (1024 * 1024));
                // Integrated GPUs report a small carve-out (<= 512 MB); treat them as "no dedicated GPU".
                if (mb > 512 && mb > hw_.gpuMemoryMB) {
                    hw_.gpuMemoryMB = mb;
                    gpuName_ = juce::String(desc.Description);
                }
            }
            adapter->Release();
        }
        factory->Release();
    }
#endif
    detected_ = chooseTier(hw_);
}

void PerfSettings::setChoice(Choice c) {
    choice_ = c;
    save();
    sendChangeMessage();
}

void PerfSettings::setNeonFixed(NeonFixed n) {
    neonFixed_ = n;
    save();
    sendChangeMessage();
}

PerfTier PerfSettings::tier() const noexcept {
    switch (choice_) {
        case Choice::Low: return PerfTier::Low;
        case Choice::Medium: return PerfTier::Medium;
        case Choice::High: return PerfTier::High;
        case Choice::Auto: break;
    }
    return detected_;
}

bool PerfSettings::neonFixed() const noexcept {
    if (neonFixed_ == NeonFixed::On) return true;
    if (neonFixed_ == NeonFixed::Off) return false;
    return tier() == PerfTier::Low;
}

void PerfSettings::forceForSnapshot(PerfTier t, bool neon) {
    choice_ = static_cast<Choice>(static_cast<int>(t) + 1);
    neonFixed_ = neon ? NeonFixed::On : NeonFixed::Off;
    sendSynchronousChangeMessage();
}

void PerfSettings::save() {
    file_->setValue("perfLevel", static_cast<int>(choice_));
    file_->setValue("neonFixed", static_cast<int>(neonFixed_));
    file_->saveIfNeeded();
}

}  // namespace pat::ui
