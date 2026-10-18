# PatSuite

Free, open-source (AGPL-3.0) suite of visual audio plugins for FL Studio and any VST3 host:
**PatDeEsser**, **PatSidechain**, **PatDoubler**, **PatTune**, **Patan** (auto-panner),
**PatPatPat** (ping-pong delay) and **PatQpro** (pro equalizer), plus the one-click installer that
also installs **PatWave** and **PatVerb**. Every plugin comes with factory presets and saves yours.

Black-and-neon look, a Simple view for beginners and a Pro view with every control, three visual
performance levels, and a "Neon fixed" mode without animations.

## Build (Windows)

Requirements: Visual Studio 2022+ Build Tools (C++), CMake 3.25+ (comes with VS), Git, and
Inno Setup 6 for the installer.

```powershell
powershell -ExecutionPolicy Bypass -File tools/fetch-deps.ps1    # JUCE + GoogleTest (once)
powershell -ExecutionPolicy Bypass -File tools/build.ps1 -Test    # every plugin + unit tests
powershell -ExecutionPolicy Bypass -File tools/build.ps1 -Run     # open the PatTemplate standalone
powershell -ExecutionPolicy Bypass -File tools/package-suite.ps1  # dist\PatSuite-x.y.z-Setup.exe
```

Built plugins end up in `build/release/plugins/<Name>/<Name>_artefacts/RelWithDebInfo/VST3/`.

## Loading in FL Studio

1. Run `PatSuite-x.y.z-Setup.exe`, or copy the `.vst3` folder into `C:\Program Files\Common Files\VST3`.
2. In FL Studio: **Options > Manage plugins > Find installed plugins**.
3. Star the Pat plugins so they appear in the plugin menus. Effects go in a mixer slot.

## Layout

```
common/core   pure C++ (no JUCE, unit-tested): lock-free FIFO, triple buffer, performance rule, tuning tables, FFT
common/ui     JUCE: theme, knob, meter, spectrum, layered canvas, frame clock, performance settings,
              PluginFrame (shared editor shell: presets, Simple/Pro, level selector, Neon fixed)
dsp/<plugin>  audio engines, pure C++ and unit-tested
ui/<plugin>   editors
plugins/<P>   one CMake target + processor per plugin
docs/         DECISIONES.md (technical decisions), PENDIENTES.md (open items)
installer/    PatSuite.iss (one installer for every Pat plugin)
presets/      your own presets, collected by tools/collect-presets.ps1, ship inside each plugin
```
