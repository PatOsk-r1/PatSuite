// Factory presets of PatPatPat. Values in real units; parameters left out keep their default.
// Note lengths: 0 1/64, 1 1/32 tri, 2 1/32, 3 1/16 tri, 4 1/16, 5 1/16 dot, 6 1/8 tri, 7 1/8,
// 8 1/8 dot, 9 1/4 tri, 10 1/4, 11 1/4 dot, 12 1/2 tri, 13 1/2, 14 1/2 dot, 15 1 bar.
#include "plugins/PatPatPat/Processor.h"

using namespace patpatpat;

std::vector<pat::ui::FactoryPreset> PatPatPatProcessor::factoryPresets() {
    const auto u = [](const char* s) { return juce::String::fromUTF8(s); };
    // mode: 0 mono, 1 stereo, 2 ping-pong, 3 cross. character: 0 clean, 1 tape, 2 analog, 3 lo-fi.
    struct P {
        const char* name;
        const char* category;
        float mode, character, sync, divL, divR, link, timeL, timeR, feedback, lowCut, highCut, drive, modRate, modDepth, duck, width,
            diffusion, mix;
    };
    const P list[] = {
        // name, category, mode, char, sync, divL, divR, link, msL, msR, fb, lowcut, highcut, drive, modRate, modDepth, duck, width, diff, mix
        {"Init", "", 2, 0, 1, 8, 8, 1, 375, 375, 0.45f, 80, 9000, 0, 0.6f, 0, 0, 1, 0, 0.35f},
        {"Ping-Pong 1/8 Dotted", "Ping-Pong", 2, 0, 1, 8, 8, 1, 375, 375, 0.5f, 120, 8000, 0, 0.6f, 0, 0, 1, 0, 0.3f},
        {"Ping-Pong 1/4", "Ping-Pong", 2, 0, 1, 10, 10, 1, 500, 500, 0.45f, 120, 9000, 0, 0.6f, 0, 0, 1, 0, 0.3f},
        {"Ping-Pong Triplets", "Ping-Pong", 2, 0, 1, 6, 6, 1, 250, 250, 0.5f, 150, 8000, 0, 0.6f, 0, 0, 1, 0, 0.3f},
        {"Ping-Pong Wide Dark", "Ping-Pong", 2, 2, 1, 8, 8, 1, 375, 375, 0.6f, 150, 5000, 0.2f, 0.4f, 0.15f, 0, 1.4f, 0, 0.3f},
        {"Ping-Pong Groove", "Ping-Pong", 2, 0, 1, 7, 8, 0, 250, 375, 0.5f, 120, 8500, 0, 0.6f, 0, 0, 1, 0, 0.3f},
        {"Cross Bouncer", "Ping-Pong", 3, 0, 1, 10, 8, 0, 500, 375, 0.55f, 120, 8000, 0, 0.6f, 0, 0, 1.2f, 0, 0.3f},
        {"Slapback", "Classic", 0, 1, 0, 8, 8, 1, 110, 110, 0.12f, 100, 7000, 0.3f, 0.6f, 0.05f, 0, 1, 0, 0.3f},
        {"Rockabilly Slap", "Classic", 0, 1, 0, 8, 8, 1, 140, 140, 0.25f, 150, 5500, 0.4f, 0.6f, 0.08f, 0, 1, 0, 0.35f},
        {"Quarter Note Echo", "Classic", 0, 0, 1, 10, 10, 1, 500, 500, 0.4f, 100, 9000, 0, 0.6f, 0, 0, 1, 0, 0.25f},
        {"Stereo Haas Widener", "Classic", 1, 0, 0, 8, 8, 0, 9, 21, 0.0f, 150, 16000, 0, 0.6f, 0, 0, 1, 0, 0.4f},
        {"Dual Times 1/4 + 1/8 dot", "Classic", 1, 0, 1, 10, 8, 0, 500, 375, 0.4f, 120, 9000, 0, 0.6f, 0, 0, 1, 0, 0.3f},
        {"Warm Tape Echo", "Tape", 0, 1, 1, 10, 10, 1, 500, 500, 0.55f, 120, 6000, 0.35f, 0.6f, 0.15f, 0, 1, 0, 0.3f},
        {"Space Echo Feel", "Tape", 1, 1, 1, 8, 10, 0, 375, 500, 0.65f, 150, 5000, 0.5f, 0.5f, 0.25f, 0, 1.1f, 0.2f, 0.3f},
        {"Worn Cassette", "Tape", 2, 1, 1, 8, 8, 1, 375, 375, 0.5f, 250, 4000, 0.4f, 0.8f, 0.6f, 0, 1, 0, 0.3f},
        {"Self Oscillation", "Tape", 0, 1, 1, 10, 10, 1, 500, 500, 0.98f, 200, 4000, 0.6f, 0.4f, 0.2f, 0, 1, 0, 0.3f},
        {"Analog Bucket", "Analog", 2, 2, 1, 8, 8, 1, 375, 375, 0.5f, 100, 4500, 0.3f, 0.5f, 0.2f, 0, 1, 0, 0.3f},
        {"Dark Analog Wash", "Analog", 1, 2, 1, 11, 10, 0, 750, 500, 0.7f, 150, 3500, 0.3f, 0.3f, 0.3f, 0, 1.3f, 0.4f, 0.3f},
        {"Lo-Fi Radio", "Lo-Fi", 0, 3, 1, 10, 10, 1, 500, 500, 0.45f, 400, 3000, 0.3f, 0.6f, 0.1f, 0, 1, 0, 0.35f},
        {"Bitcrushed Bounce", "Lo-Fi", 2, 3, 1, 7, 7, 1, 250, 250, 0.55f, 300, 3500, 0.2f, 0.6f, 0, 0, 1, 0, 0.3f},
        {"Vocal Throw 1/4", "Vocals", 2, 0, 1, 10, 10, 1, 500, 500, 0.4f, 300, 6000, 0, 0.6f, 0.1f, 0.6f, 1.2f, 0.1f, 0.3f},
        {"Vocal Ducked 1/8 dot", "Vocals", 2, 0, 1, 8, 8, 1, 375, 375, 0.45f, 250, 7000, 0, 0.6f, 0.1f, 0.8f, 1.1f, 0.15f, 0.35f},
        {"Rap Ad-lib Echo", "Vocals", 2, 0, 1, 7, 7, 1, 250, 250, 0.3f, 400, 6500, 0.1f, 0.6f, 0, 0.5f, 1.3f, 0, 0.3f},
        {"Ballad Long Tail", "Vocals", 1, 2, 1, 13, 11, 0, 1000, 750, 0.55f, 200, 5000, 0, 0.3f, 0.2f, 0.7f, 1.3f, 0.5f, 0.25f},
        {"Guitar Dotted Eighths", "Instruments", 1, 0, 1, 8, 10, 0, 375, 500, 0.5f, 150, 7000, 0, 0.6f, 0.05f, 0.3f, 1.2f, 0, 0.35f},
        {"Synth Lead Echo", "Instruments", 2, 0, 1, 8, 8, 1, 375, 375, 0.55f, 200, 9000, 0.1f, 0.6f, 0.1f, 0.4f, 1.3f, 0.2f, 0.3f},
        {"Pluck Rhythm 1/16", "Instruments", 2, 0, 1, 4, 4, 1, 125, 125, 0.45f, 200, 9000, 0, 0.6f, 0, 0.2f, 1.2f, 0, 0.3f},
        {"Snare Throw", "Drums", 2, 1, 1, 10, 10, 1, 500, 500, 0.35f, 300, 6000, 0.3f, 0.6f, 0.05f, 0, 1.2f, 0, 0.3f},
        {"Hat Flam 1/32", "Drums", 1, 0, 1, 2, 3, 0, 62, 83, 0.25f, 2000, 16000, 0, 0.6f, 0, 0, 1.4f, 0, 0.3f},
        {"Diffused Ambience", "Ambient", 1, 0, 0, 8, 8, 0, 180, 230, 0.6f, 150, 7000, 0, 0.3f, 0.3f, 0, 1.5f, 0.9f, 0.3f},
        {"Endless Shimmer", "Ambient", 1, 2, 1, 13, 14, 0, 1000, 1500, 0.8f, 200, 6000, 0, 0.2f, 0.4f, 0, 1.6f, 0.7f, 0.35f},
        {"Dub Siren Tunnel", "Ambient", 2, 1, 1, 11, 11, 1, 750, 750, 0.85f, 300, 3000, 0.5f, 0.4f, 0.3f, 0, 1.2f, 0.3f, 0.35f},
        // Producers: Uruguayan names that describe the sound.
        {"Eco del Cerro", "Producers/Torbey Beats", 1, 2, 1, 13, 14, 0, 1000, 1500, 0.55f, 150, 4500, 0.2f, 0.3f, 0.2f, 0.2f, 1.4f, 0.4f, 0.3f},
        {"Pique de Pelota", "Producers/Torbey Beats", 2, 0, 1, 7, 7, 1, 250, 250, 0.6f, 200, 9000, 0, 0.6f, 0, 0.3f, 1.3f, 0, 0.3f},
        {"Tambor de Llamadas", "Producers/Torbey Beats", 3, 1, 1, 6, 9, 0, 250, 333, 0.5f, 150, 6000, 0.3f, 0.6f, 0.1f, 0.2f, 1.2f, 0, 0.3f},
        {"Grito en el Estadio", "Producers/Torbey Beats", 1, 0, 1, 10, 11, 0, 500, 750, 0.45f, 300, 5000, 0, 0.6f, 0.1f, 0.4f, 1.5f, 0.6f, 0.3f},
        {"Ómnibus en el Túnel", "Producers/Torbey Beats", 0, 2, 1, 10, 10, 1, 500, 500, 0.7f, 200, 3000, 0.3f, 0.3f, 0.2f, 0.3f, 1, 0.3f, 0.3f},
        {"Cassette de la Feria", "Producers/Torbey Beats", 2, 3, 1, 8, 8, 1, 375, 375, 0.5f, 400, 3200, 0.3f, 0.8f, 0.4f, 0.2f, 1, 0, 0.35f},
        {"Pat Pat Pat", "Producers/Pat Osk", 2, 0, 1, 8, 8, 1, 375, 375, 0.65f, 150, 8000, 0.1f, 0.6f, 0.05f, 0.4f, 1.3f, 0, 0.35f},
        {"Ta, Ta, Ta... Ta", "Producers/Pat Osk", 2, 1, 1, 4, 5, 0, 125, 188, 0.55f, 200, 7000, 0.3f, 0.6f, 0.1f, 0.3f, 1.2f, 0, 0.3f},
        {"Grappamiel Eterna", "Producers/Pat Osk", 1, 1, 1, 14, 15, 0, 1500, 2000, 0.8f, 200, 4000, 0.4f, 0.25f, 0.35f, 0.3f, 1.5f, 0.5f, 0.3f},
        {"Rebote en la Rambla", "Producers/Pat Osk", 3, 0, 1, 8, 10, 0, 375, 500, 0.6f, 150, 8000, 0, 0.6f, 0.1f, 0.3f, 1.4f, 0.2f, 0.3f},
        {"Radio AM del Interior", "Producers/Pat Osk", 0, 3, 1, 10, 10, 1, 500, 500, 0.5f, 500, 2800, 0.4f, 0.6f, 0.15f, 0.3f, 1, 0, 0.35f},
        {"Niebla en Punta Carretas", "Producers/Pat Osk", 1, 2, 0, 8, 8, 0, 230, 310, 0.75f, 200, 4500, 0, 0.2f, 0.4f, 0.4f, 1.6f, 0.9f, 0.3f},
    };
    std::vector<pat::ui::FactoryPreset> out;
    for (const auto& p : list)
        out.push_back({u(p.name),
                       u(p.category),
                       {{kMode, p.mode},
                        {kCharacter, p.character},
                        {kSync, p.sync},
                        {kDivL, p.divL},
                        {kDivR, p.divR},
                        {kLink, p.link},
                        {kTimeL, p.timeL},
                        {kTimeR, p.timeR},
                        {kFeedback, p.feedback},
                        {kLowCut, p.lowCut},
                        {kHighCut, p.highCut},
                        {kDrive, p.drive},
                        {kModRate, p.modRate},
                        {kModDepth, p.modDepth},
                        {kDuck, p.duck},
                        {kWidth, p.width},
                        {kDiffusion, p.diffusion},
                        {kMix, p.mix}}});
    return out;
}
