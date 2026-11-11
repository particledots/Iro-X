# Iro-x

An unofficial Audio Unit (for Logic Pro) and VST3 **effect** plugin for macOS, based on the open-source firmware of
the Befaco Iroi. **Not made, approved or supported by Befaco.** Do not contact them about this plugin. Iro-x is a port
and will be marked as modified as soon as the firmware is changed.

Iro-x is an effect: it processes the audio of a Logic track. It makes no sound by itself and plays no notes.

## Status
Version 1.0.0, Milestone 2 (the finished plugin): the Iroi effects chain follows Logic's tempo, rings out past the end
of a region (tail length 12 s), has a working Randomise button and its final window. Milestones 0 and 1 passed in Logic
on the M4 Mac mini (see Test history); the Milestone 2 test in Logic is still to be done.

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

## What it does
The Iroi effects chain, stereo in and stereo out (a mono track is also accepted): multimode filter plus VCA (placeable
in the chain), 3-band resonator, echo, "ambience" reverb with the Spacetime macro, an internal modulation source, and
the randomiser. Every control is a parameter Logic can automate and that is saved with the project. The hardware's CV
jacks, sync jack and input detection are not copied; Logic automation, Logic's tempo and the internal modulation take
their place.

- **Logic's tempo.** On the module a clock pulse in the sync jack sets the tempo for the modulation speed, the echo
  times and the auto-pan. Iro-x makes that pulse from Logic's tempo, one per quarter note, and keeps it on the beats of
  the song (it follows a jump or a loop at once, and keeps running at the tempo when Logic is stopped). The modulation
  Speed fader then picks a note value of the beat (the centre is one cycle per beat), and the echo Density picks the
  echo time as a fraction of a beat. With no tempo from the host the plugin uses the firmware's internal clock (60 BPM).
  Tempos below 40 or above 300 BPM are limited to that range for the clock. The firmware only moves the echo times for
  a change of more than about 6 % in the tempo, so a slow tempo ramp reaches the echo in steps.
- **Randomise.** The button in the window (and the automatable parameter "Randomise": off to on randomises once) moves
  ten controls, Cutoff, Resonance, Tune, Feedback, Repeats, Density, Decay, Spacetime, Modulation Level and Speed, by a
  random amount of up to their own Rnd amount (0 = stays, 100 = anywhere), as the firmware does. Each control is moved
  inside a change gesture, so Logic can record it as automation. Left out: the module's long press that glides to the
  new values (the change is immediate).
- **Tail.** The plugin tells Logic the sound rings on for 12 s after the audio ends (measured: reverb Decay at full
  with Spacetime in the middle 9.6 s, resonator Feedback 0.8 up to 10.6 s, to -80 dB). The firmware also has settings
  that ring much longer or on purpose for ever (reverb Decay at full with Spacetime at either end, echo Repeats above
  about 0.95 with long times, resonator Feedback above about 0.85): no number covers those; the sound goes on while
  Logic keeps playing and is cut at the end of the 12 s only when Logic bounces or stops.
- **Window.** Dark, one colour per section (Filter + VCA, Resonator, Echo, Ambience, Modulation), size 75 to 150 %. The
  faders show your settings; the modulation moves the sound, not the faders (decided with Bryn on Nov 11, 2026). The top
  line shows the tempo Logic gives the plugin.

## Source and licence
- `third_party/iroi-firmware`: Befaco's Iroi source, GPL-3.0 (`SOURCE.txt` names the exact commit). Two small bug
  fixes are marked "Iro-x change" and listed in `IROX-CHANGES.md`; every other file is an unchanged copy. The workflow
  checks this file by file on every build. Milestone 2 changed nothing in it.
- `third_party/owlprogram`: 31 library files from Rebel Technology's OwlProgram (the OpenWare library the Iroi source is
  written against), GPL-3.0, unmodified (`SOURCE.txt` names the commit).
- `src/engine`: Iro-x's own code that runs the firmware on a computer (stand-ins for what the module's OpenWare system
  provides, the 48 kHz converter for other sample rates). The Iroi source also contains code adapted from Mutable
  Instruments, which is MIT-licensed (checked in Milestone 2; the notice is in `THIRD-PARTY-LICENSES.txt`).
- Iro-x is released under GPL-3.0 (`LICENSE`). A build that is passed on must come with its source.

## How it works inside
The firmware only runs at 48 kHz in blocks of 32 samples, so Iro-x always runs the engine that way and converts the
sound to and from the sample rate of the project (44.1, 48 or 96 kHz) with a high-quality resampler. The plugin tells
the host its delay: 32 samples at 48 kHz (about 0.7 ms), a little more at 44.1 and 96 kHz. Each plugin instance has its
own independent engine. Input and Output level are plugin controls (-24 to +12 dB). The firmware's Noise control is not copied (the
firmware never uses it). The parameter names and order from Milestone 1 are unchanged; the "Randomise" parameter was
added at the end.

## Test history
- Milestone 0, Nov 10, 2026: automatic plugin test passes (23 checks: effect type, stereo and mono layouts, audio
  unchanged at 44.1, 48 and 96 kHz, saving and loading, window drawn). 
- Milestone 0, Nov 11, 2026: test in Logic 11.2 on the M4 Mac mini (macOS Tahoe): passed, 8 of 8 checks (install, appears and loads under Audio FX, audio unchanged, window text correct, survives save and reopen, uninstall removes it). Intel Mac not tested.
- Milestone 1, Nov 11, 2026: engine test in the workspace (x86, with address and undefined-behaviour checks, and ARM64): all checks passed (resampler, every effect, 40 s random-settings run at 44.1, 48 and 96 kHz, instances independent). Automatic plugin test: 48 checks passed. Test in Logic 11.2 on the M4 Mac mini (macOS Tahoe), Nov 11, 2026: passed, 15 of 15 checks (install and rescan, window, clean playback, Filter with all four modes and four chain positions, Resonator, Echo, Ambience including Spacetime at both ends without noise or crash, Modulation with all seven types, Input and Output level, Logic automation with clear names, settings restored after save and reopen, three instances independent, 44.1 and 96 kHz, window sizes 75/125/150 % and a mono track). Note: the Modulation moves the sound but the faders do not move on screen; that is how it works (the modulation is inside the engine) and is left as is. Intel Mac not tested (not requested).
- Milestone 2, Nov 11, 2026 (automatic tests, workspace): engine test 66 checks passed (x86 plain, x86 with address and undefined-behaviour checks; the new sections are host tempo and tail); plugin test 70 checks passed (Linux, native). The run on GitHub (Apple silicon, Intel through Rosetta) and the test in Logic on the M4 Mac mini are still to be done.
