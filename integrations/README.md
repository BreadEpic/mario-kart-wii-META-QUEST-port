# Wheel Wizard VR integration

The portable release uses Wheel Wizard revision
`86618e7367df935d78401583136e492c6f00fa27` with the changes in
[`wheelwizard-vr.patch`](wheelwizard-vr.patch).

The patch adds a local portable launcher for the base VR build and Retro Rewind VR,
keeps Wheel Wizard settings beside the executable, opens `WiiCompiled-VR-Setup.exe`
when a build is missing, and fixes the home-page game selector so its full English
label remains visible above the animated wheel decoration.

To reproduce the bundled launcher:

```powershell
git clone https://github.com/TeamWheelWizard/WheelWizard.git
cd WheelWizard
git checkout 86618e7367df935d78401583136e492c6f00fa27
git apply ..\wheelwizard-vr.patch
dotnet publish WheelWizard/WheelWizard.csproj -c Release -r win-x64 --self-contained true `
  -p:PublishSingleFile=true -p:IncludeNativeLibrariesForSelfExtract=true `
  -p:EnableCompressionInSingleFile=true
```

Wheel Wizard is licensed under GPL-3.0. Its unmodified license is included in the
portable archive.

`Launcher/Build-Portable.ps1` now performs the pinned checkout, patch validation, self-contained
launcher build, ROM-free bundle checks and manifest/checksum generation. Use a clean tagged Git
checkout for releases; `-AllowDirty` is reserved for local development builds. The package workflow
publishes only the portable archive and checksum when a tag is pushed.

The patched Update action opens the portable VR updater. It updates managed launcher/setup files;
run the new setup to compile the game runtime from your own disc. It does not replace the local VR
launcher with Wheel Wizard's upstream non-VR binary.
