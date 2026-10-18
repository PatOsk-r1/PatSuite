; PatSuite: one installer for every Pat plugin. Built by tools/package-suite.ps1, which passes
; /DProjects=<folder that contains PatWave, PatVerb...> and /DAppVersion.
; A plugin is included only if its release build exists, so new plugins join automatically
; once they are added to the list below and built.
#ifndef AppVersion
  #define AppVersion "0.1.0"
#endif
#ifndef Projects
  #define Projects "..\.."
#endif

; Stand-alone projects (PatWave, PatVerb) live in sibling folders; the vocal suite plugins are
; built inside PatSuite itself (build\dist\plugins\<Name>).
#define SiblingDir(Name) Projects + "\" + Name + "\build\dist\" + Name + "_artefacts\Release"
#define SuiteDir(Name) Projects + "\PatSuite\build\dist\plugins\" + Name + "\" + Name + "_artefacts\Release"
#define PluginDir(Name) DirExists(SiblingDir(Name)) ? SiblingDir(Name) : SuiteDir(Name)
#define HasPlugin(Name) FileExists(PluginDir(Name) + "\VST3\" + Name + ".vst3\Contents\x86_64-win\" + Name + ".vst3")

[Setup]
AppId={{B71D4E2A-6C93-4F58-A0E7-2D9C5B3F1A64}
AppName=PatSuite
AppVersion={#AppVersion}
AppPublisher=PatWave
DefaultDirName={autopf}\PatSuite
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=PatSuite-{#AppVersion}-Setup
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
LicenseFile={#Projects}\PatWave\LICENSE
UninstallDisplayName=PatSuite (Pat plugins)

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[CustomMessages]
spanish.FullInstall=Todos los plugins
spanish.CustomInstall=Elegir plugins
english.FullInstall=All plugins
english.CustomInstall=Choose plugins

[Types]
Name: "full"; Description: "{cm:FullInstall}"
Name: "custom"; Description: "{cm:CustomInstall}"; Flags: iscustom

[Components]
#if HasPlugin("PatWave")
Name: "patwave"; Description: "PatWave - wavetable synth"; Types: full custom
#endif
#if HasPlugin("PatVerb")
Name: "patverb"; Description: "PatVerb - visual reverb"; Types: full custom
#endif
#if HasPlugin("PatDeEsser")
Name: "patdeesser"; Description: "PatDeEsser - de-esser"; Types: full custom
#endif
#if HasPlugin("PatSidechain")
Name: "patsidechain"; Description: "PatSidechain - sidechain / ducking"; Types: full custom
#endif
#if HasPlugin("PatDoubler")
Name: "patdoubler"; Description: "PatDoubler - vocal doubler"; Types: full custom
#endif
#if HasPlugin("PatTune")
Name: "pattune"; Description: "PatTune - real-time pitch correction"; Types: full custom
#endif

[Files]
; Each plugin: the VST3 where every DAW looks, plus its standalone app.
#define Plugin(Name, Comp) \
  "Source: """ + PluginDir(Name) + "\VST3\" + Name + ".vst3\*""; DestDir: ""{commoncf64}\VST3\" + Name + ".vst3""; Components: " + Comp + "; Flags: ignoreversion recursesubdirs" + NewLine + \
  "Source: """ + PluginDir(Name) + "\Standalone\" + Name + ".exe""; DestDir: ""{app}""; Components: " + Comp + "; Flags: ignoreversion skipifsourcedoesntexist" + NewLine
#if HasPlugin("PatWave")
{#Plugin("PatWave", "patwave")}
#endif
#if HasPlugin("PatVerb")
{#Plugin("PatVerb", "patverb")}
#endif
#if HasPlugin("PatDeEsser")
{#Plugin("PatDeEsser", "patdeesser")}
#endif
#if HasPlugin("PatSidechain")
{#Plugin("PatSidechain", "patsidechain")}
#endif
#if HasPlugin("PatDoubler")
{#Plugin("PatDoubler", "patdoubler")}
#endif
#if HasPlugin("PatTune")
{#Plugin("PatTune", "pattune")}
#endif
Source: "{#Projects}\PatWave\LICENSE"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
#if HasPlugin("PatWave")
Name: "{autoprograms}\PatSuite\PatWave"; Filename: "{app}\PatWave.exe"; Components: patwave
#endif
#if HasPlugin("PatVerb")
Name: "{autoprograms}\PatSuite\PatVerb"; Filename: "{app}\PatVerb.exe"; Components: patverb
#endif

[Messages]
spanish.FinishedLabel=PatSuite quedó instalado. En FL Studio: Options > Manage plugins > Find installed plugins, y marcá la estrella de cada plugin Pat.
english.FinishedLabel=PatSuite is installed. Rescan plugins in your DAW to find them.
