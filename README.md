# Iro-x

An unofficial Audio Unit (for Logic Pro) and VST3 **effect** plugin for macOS, based on the open-source firmware of
the Befaco Iroi. **Not made, approved or supported by Befaco.** Do not contact them about this plugin. Iro-x is a port
and will be marked as modified as soon as the firmware is changed.

Iro-x is an effect: it processes the audio of a Logic track. It makes no sound by itself and plays no notes.

## Status
Version 0.2.0, Milestone 1 (the effects run): the Iroi engine processes the audio of the track, every control is in a
rough window and is a parameter Logic can automate and save. Milestone 0 passed in Logic on the M4 Mac mini; the
Milestone 1 test in Logic is still to be done (see Test history). Not yet in Milestone 1: the randomiser, Logic's
tempo for tempo-synced parts, the tail length, the final look (all Milestone 2).

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

## What it does (and what is still to come)
The Iroi effects chain, stereo in and stereo out: multimode filter plus VCA (placeable in the chain), 3-band
resonator, echo, "ambience" reverb with the Spacetime macro, an internal modulation source, and the randomiser. Every
control is a parameter Logic can automate and that is saved with the project. The hardware's CV jacks, sync jack and
input detection are not copied; Logic automation, Logic's tempo and the internal modulation take their place.

## Source and licence
- `third_party/iroi-firmware`: Befaco's Iroi source, GPL-3.0 (`SOURCE.txt` names the exact commit). Two small bug
  fixes are marked "Iro-x change" and listed in `IROX-CHANGES.md`; every other file is an unchanged copy. The workflow
  checks this file by file on every build.
- `third_party/owlprogram`: 31 library files from Rebel Technology's OwlProgram (the OpenWare library the Iroi source is
  written against), GPL-3.0, unmodified (`SOURCE.txt` names the commit).
- `src/engine`: Iro-x's own code that runs the firmware on a computer (stand-ins for what the module's OpenWare system
  provides, the 48 kHz converter for other sample rates). The Iroi source also contains code adapted from Mutable
  Instruments (see `THIRD-PARTY-LICENSES.txt`, still to be checked in Milestone 2).
- Iro-x is released under GPL-3.0 (`LICENSE`). A build that is passed on must come with its source.

## How it works inside (Milestone 1)
The firmware only runs at 48 kHz in blocks of 32 samples, so Iro-x always runs the engine that way and converts the
sound to and from the sample rate of the project (44.1, 48 or 96 kHz) with a high-quality resampler. The plugin tells
the host its delay: 32 samples at 48 kHz (about 0.7 ms), a little more at 44.1 and 96 kHz. Each plugin instance has its
own independent engine. Input and Output level are plugin controls (-24 to +12 dB). The ten "Rnd" amounts are saved but do
nothing until the randomiser arrives in Milestone 2.

## Test history
- Milestone 0, Nov 10, 2026: automatic plugin test passes (23 checks: effect type, stereo and mono layouts, audio
  unchanged at 44.1, 48 and 96 kHz, saving and loading, window drawn). 
- Milestone 0, Nov 11, 2026: test in Logic 11.2 on the M4 Mac mini (macOS Tahoe): passed, 8 of 8 checks (install, appears and loads under Audio FX, audio unchanged, window text correct, survives save and reopen, uninstall removes it). Intel Mac not tested.
- Milestone 1, Nov 11, 2026: engine test in the workspace (x86, with address and undefined-behaviour checks, and ARM64): all checks passed (resampler, every effect, 40 s random-settings run at 44.1, 48 and 96 kHz, instances independent). Automatic plugin test: 48 checks passed. Test in Logic: pending.
