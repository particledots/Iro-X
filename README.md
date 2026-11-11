# Iro-x

An unofficial Audio Unit (for Logic Pro) and VST3 **effect** plugin for macOS, based on the open-source firmware of
the Befaco Iroi. **Not made, approved or supported by Befaco.** Do not contact them about this plugin. Iro-x is a port
and will be marked as modified as soon as the firmware is changed.

Iro-x is an effect: it processes the audio of a Logic track. It makes no sound by itself and plays no notes.

## Status
Version 0.1.0, Milestone 0 (empty effect): audio passes through unchanged, the window is a placeholder, the Iroi
source is imported unmodified. Not yet tested in Logic (see Test history). Milestones 1 and 2 add the effects.

## Install
1. Download `Iro-x-macOS` from the latest run's Artifacts (GitHub, Actions tab) and unzip it.
2. Double-click `Install-Iro-x.command`. It asks for the Mac password once and installs the Audio Unit and the VST3
   for all users, replacing an older copy.
3. Open Logic. Iro-x is in a track's Audio FX slot, under Audio Units > particledots. If it is missing: Logic's
   Settings > Plug-in Manager > Reset & Rescan Selection.

`Uninstall-Iro-x.command` removes the plugin again. GitHub deletes a run's download after 90 days, so keep the zip if
you want to install again later. The build is universal (Apple silicon and Intel), needs macOS 11 or later, and is
not signed with an Apple developer ID: on a Mac that downloaded it from the internet, macOS may ask for approval
the first time (`INSTALL.txt` in the zip says what to do).

## What it will do (plan, see the Project documents)
The Iroi effects chain, stereo in and stereo out: multimode filter plus VCA (placeable in the chain), 3-band
resonator, echo, "ambience" reverb with the Spacetime macro, an internal modulation source, and the randomiser. Every
control is a parameter Logic can automate and that is saved with the project. The hardware's CV jacks, sync jack and
input detection are not copied; Logic automation, Logic's tempo and the internal modulation take their place.

## Source and licence
- `third_party/iroi-firmware`: Befaco's Iroi source, GPL-3.0 (`SOURCE.txt` names the exact commit). Milestone 0:
  unmodified. The workflow checks this on every build.
- The Iroi source needs headers from the Rebel Technology OpenWare platform and contains code adapted from Mutable
  Instruments; neither is in Befaco's repository. They are handled in Milestone 1 (see `THIRD-PARTY-LICENSES.txt`).
- Iro-x is released under GPL-3.0 (`LICENSE`). A build that is passed on must come with its source.

## Test history
- Milestone 0, Nov 10, 2026: automatic plugin test passes (23 checks: effect type, stereo and mono layouts, audio
  unchanged at 44.1, 48 and 96 kHz, saving and loading, window drawn). Test in Logic on the M4 Mac mini: pending.
