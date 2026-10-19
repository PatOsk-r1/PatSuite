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

; Background song: the first .mp3 (or .wav) found in installer\music plays in a loop while the
; wizard is open. No file there = silent installer.
#define MusicName ""
#define MusicHandle FindFirst(SourcePath + "music\*.mp3", 0)
#if !MusicHandle
  #define MusicHandle FindFirst(SourcePath + "music\*.wav", 0)
#endif
#if MusicHandle
  #define MusicName FindGetFileName(MusicHandle)
  #expr FindClose(MusicHandle)
#endif

[Setup]
AppId={{B71D4E2A-6C93-4F58-A0E7-2D9C5B3F1A64}
AppName=PatSuite
AppVersion={#AppVersion}
AppPublisher=Pat Osk
DefaultDirName={autopf}\PatSuite
DisableProgramGroupPage=yes
OutputDir=..\dist
Compression=lzma2/ultra64
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
#ifdef PreviewPage
; Preview build (tools/preview-installer.ps1): opens straight on one page, no admin prompt.
PrivilegesRequired=lowest
OutputBaseFilename=PatSuite-preview
#else
PrivilegesRequired=admin
OutputBaseFilename=PatSuite-{#AppVersion}-Setup
#endif
; Neon look: art\* comes from tools/make-installer-art.py.
WizardStyle=modern dark hidebevels
WizardBackColor=#030a06
WizardBackImageFile=art\back.png
WizardImageFile=art\wizard.png
WizardSmallImageFile=art\small.png
DisableWelcomePage=no
LicenseFile=art\license.rtf
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
#if HasPlugin("PatChain")
Name: "patchain"; Description: "PatChain - sidechain / ducking"; Types: full custom
#endif
#if HasPlugin("PatDoubler")
Name: "patdoubler"; Description: "PatDoubler - vocal doubler"; Types: full custom
#endif
#if HasPlugin("PatTune")
Name: "pattune"; Description: "PatTune - real-time pitch correction"; Types: full custom
#endif
#if HasPlugin("Patan")
Name: "patan"; Description: "Patan - auto panner with drawn curves"; Types: full custom
#endif
#if HasPlugin("PatPatPat")
Name: "patpatpat"; Description: "PatPatPat - stereo / ping-pong delay"; Types: full custom
#endif
#if HasPlugin("PatQpro")
Name: "patqpro"; Description: "PatQpro - pro equalizer"; Types: full custom
#endif
#if HasPlugin("PatSquash")
Name: "patsquash"; Description: "PatSquash - multiband upward/downward compressor"; Types: full custom
#endif
#if HasPlugin("PatMaster")
Name: "patmaster"; Description: "PatMaster - master limiter + loudness meter"; Types: full custom
#endif
#if HasPlugin("PatBass")
Name: "patbass"; Description: "PatBass - bass synth"; Types: full custom
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
#if HasPlugin("PatChain")
{#Plugin("PatChain", "patchain")}
#endif
#if HasPlugin("PatDoubler")
{#Plugin("PatDoubler", "patdoubler")}
#endif
#if HasPlugin("PatTune")
{#Plugin("PatTune", "pattune")}
#endif
#if HasPlugin("Patan")
{#Plugin("Patan", "patan")}
#endif
#if HasPlugin("PatPatPat")
{#Plugin("PatPatPat", "patpatpat")}
#endif
#if HasPlugin("PatQpro")
{#Plugin("PatQpro", "patqpro")}
#endif
#if HasPlugin("PatSquash")
{#Plugin("PatSquash", "patsquash")}
#endif
#if HasPlugin("PatMaster")
{#Plugin("PatMaster", "patmaster")}
#endif
#if HasPlugin("PatBass")
{#Plugin("PatBass", "patbass")}
#endif
Source: "{#Projects}\PatWave\LICENSE"; DestDir: "{app}"; Flags: ignoreversion
Source: "{#Projects}\PatWave\THIRD_PARTY.txt"; DestDir: "{app}"; Flags: ignoreversion
#if MusicName != ""
Source: "music\{#MusicName}"; Flags: dontcopy
#endif

[Icons]
#if HasPlugin("PatWave")
Name: "{autoprograms}\PatSuite\PatWave"; Filename: "{app}\PatWave.exe"; Components: patwave
#endif
#if HasPlugin("PatVerb")
Name: "{autoprograms}\PatSuite\PatVerb"; Filename: "{app}\PatVerb.exe"; Components: patverb
#endif
#if HasPlugin("PatDeEsser")
Name: "{autoprograms}\PatSuite\PatDeEsser"; Filename: "{app}\PatDeEsser.exe"; Components: patdeesser
#endif
#if HasPlugin("PatChain")
Name: "{autoprograms}\PatSuite\PatChain"; Filename: "{app}\PatChain.exe"; Components: patchain
#endif
#if HasPlugin("PatDoubler")
Name: "{autoprograms}\PatSuite\PatDoubler"; Filename: "{app}\PatDoubler.exe"; Components: patdoubler
#endif
#if HasPlugin("PatTune")
Name: "{autoprograms}\PatSuite\PatTune"; Filename: "{app}\PatTune.exe"; Components: pattune
#endif
#if HasPlugin("Patan")
Name: "{autoprograms}\PatSuite\Patan"; Filename: "{app}\Patan.exe"; Components: patan
#endif
#if HasPlugin("PatPatPat")
Name: "{autoprograms}\PatSuite\PatPatPat"; Filename: "{app}\PatPatPat.exe"; Components: patpatpat
#endif
#if HasPlugin("PatQpro")
Name: "{autoprograms}\PatSuite\PatQpro"; Filename: "{app}\PatQpro.exe"; Components: patqpro
#endif
#if HasPlugin("PatSquash")
Name: "{autoprograms}\PatSuite\PatSquash"; Filename: "{app}\PatSquash.exe"; Components: patsquash
#endif
#if HasPlugin("PatMaster")
Name: "{autoprograms}\PatSuite\PatMaster"; Filename: "{app}\PatMaster.exe"; Components: patmaster
#endif
#if HasPlugin("PatBass")
Name: "{autoprograms}\PatSuite\PatBass"; Filename: "{app}\PatBass.exe"; Components: patbass
#endif

[Messages]
spanish.FinishedLabel=PatSuite quedó instalado. En FL Studio: Options > Manage plugins > Find installed plugins, y marcá la estrella de cada plugin Pat.
english.FinishedLabel=PatSuite is installed. Rescan plugins in your DAW to find them.
spanish.SelectDirBrowseLabel=Acá van solo las versiones standalone (.exe) y el desinstalador. Los plugins VST3 se instalan siempre en C:\Program Files\Common Files\VST3, donde FL Studio y los demás DAW los encuentran solos.
english.SelectDirBrowseLabel=This folder only holds the standalone apps (.exe) and the uninstaller. The VST3 plugins always go to C:\Program Files\Common Files\VST3, where every DAW finds them.

[Code]
const
  Neon = $0014FF39;  { BGR of #39FF14 }
  Mint = $00B0FFA8;

var
  MusicOn: Boolean;
  MusicButton: TNewButton;

function mciSendString(Command: String; Buffer: String; Size: Cardinal; Callback: HWND): Cardinal;
  external 'mciSendStringW@winmm.dll stdcall';

procedure ToggleMusic(Sender: TObject);
begin
  if MusicOn then begin
    mciSendString('pause patmusic', '', 0, 0);
    MusicButton.Caption := '♪ OFF';
  end else begin
    mciSendString('resume patmusic', '', 0, 0);
    MusicButton.Caption := '♪ ON';
  end;
  MusicOn := not MusicOn;
end;

procedure StartMusic;
begin
#if MusicName != ""
  ExtractTemporaryFile('{#MusicName}');
  if mciSendString('open "' + ExpandConstant('{tmp}\{#MusicName}') + '" type mpegvideo alias patmusic', '', 0, 0) <> 0 then
    Exit;
  mciSendString('play patmusic repeat', '', 0, 0);
  MusicOn := True;
  MusicButton := TNewButton.Create(WizardForm);
  MusicButton.Parent := WizardForm;
  MusicButton.Left := WizardForm.ClientWidth - WizardForm.CancelButton.Left - WizardForm.CancelButton.Width;
  MusicButton.Top := WizardForm.CancelButton.Top;
  MusicButton.Width := WizardForm.CancelButton.Width;
  MusicButton.Height := WizardForm.CancelButton.Height;
  MusicButton.Caption := '♪ ON';
  MusicButton.OnClick := @ToggleMusic;
#endif
end;

procedure Paint(L: TNewStaticText; Colour: TColor; Bold: Boolean);
begin
  L.Font.Color := Colour;
  L.StyleElements := [seClient, seBorder];
  if Bold then
    L.Font.Style := [fsBold];
end;

procedure InitializeWizard;
begin
  Paint(WizardForm.WelcomeLabel1, Neon, True);
  Paint(WizardForm.WelcomeLabel2, Mint, False);
  Paint(WizardForm.FinishedHeadingLabel, Neon, True);
  WizardForm.PageNameLabel.Font.Color := Neon;
  WizardForm.PageDescriptionLabel.Font.Color := Mint;
  StartMusic;
end;

procedure DeinitializeSetup;
begin
  mciSendString('close patmusic', '', 0, 0);
end;

#ifdef PreviewPage
function ShouldSkipPage(PageID: Integer): Boolean;
begin
  Result := PageID <> {#PreviewPage};
end;
#endif
