# Changelog

## 1.0.0: Milestone 2 (the finished plugin)
- Logic's tempo: the module's sync jack is replaced by a clock made from Logic's tempo and song position (one pulse
  per quarter note). The modulation speed, the echo times and the auto-pan follow it, also after a jump or loop and while
  Logic is stopped. No host tempo: the firmware's internal clock (60 BPM). Clock range 40 to 300 BPM.
- Tail length reported to Logic: 12 s (measured with the engine test, section 6). Settings that ring on much longer or
  for ever by design are listed in the README and are not covered.
- Randomise: the button in the window and a new automatable parameter "Randomise" (added at the END of the parameter
  list; nothing saved or automated before it moves). It moves the ten controls the firmware randomises, each by up to its
  Rnd amount, with the firmware's own arithmetic, inside change gestures so Logic can record it. Opening a project does not
  randomise. Not copied: the long-press glide.
- Final window: dark, one colour per section, new faders, Mod and Rnd bars, Randomise button, tempo line, notice about
  Befaco and Mutable Instruments in the footer. The faders still show only your settings (decided).
- Licence check: the Mutable Instruments code adapted inside the Iroi firmware (stmlib, Rings, Plaits) is MIT-licensed;
  the notice is in `THIRD-PARTY-LICENSES.txt`. No change to the firmware files. The workflow checks that the notice is in
  the download.
- Tests: engine test now has host tempo (modulation rate, echo time, song position, jumps, extreme tempos) and tail
  sections and flushes denormal numbers like the plugin does (66 checks); plugin test has 70 checks (host tempo, randomiser,
  Randomise parameter, tail).
- Result: passed in Logic on both Macs, Nov 11, 2026 (M4 Mac mini with Logic 11.2, and the Intel Mac): all 20 checks, nothing heard wrong. Tail with Bounce in Place: about 7.2 s at Spacetime 50 %, about 12.4 s at 59 % (longer settings ring past the reported 12 s, as documented). Sample rate tested: 44.1 kHz.

## 0.2.0: Milestone 1 (the effects run)
- The Iroi engine (filter + VCA, resonator, echo, ambience, modulation) processes the track's audio. Stereo in and out,
  mono accepted. Works at 44.1, 48 and 96 kHz through a resampler; the engine itself runs at 48 kHz in 32-sample blocks.
  Reported delay: 32 samples at 48 kHz. Tail length still 0 (Milestone 2).
- 40 automatable parameters, saved with the Logic project: every Iroi control, the Mod and Rnd amounts, Input and
  Output level (-24 to +12 dB). Names and IDs are fixed from now on. Not copied: the firmware's Noise control (the
  firmware never uses it).
- Rough window with every control, grouped by section as in the approved mock-up, window size 75 to 150 %. The Rnd
  amounts are saved but inert and the Randomise button is disabled until Milestone 2.
- New: `src/engine` (engine without the OpenWare platform), `third_party/owlprogram` (31 unmodified OwlProgram library
  files), `tools/engine_test` (test signal through every effect, WAV files, random-settings run, instance test).
- Two bug fixes in the firmware copy, marked "Iro-x change" and listed in `IROX-CHANGES.md`: a delay read past the
  end of the ambience buffer and a comb-filter read one sample before its buffer. Known and accepted: a few kilobytes
  per plugin instance are never given back by the firmware.
- Workflow: file-by-file firmware check, engine test (Apple silicon and Intel), WAV files as an artifact.
- Result: passed on Nov 11, 2026 in Logic 11.2 on the M4 Mac mini (macOS Tahoe). All 15 checks OK. Note from the test: the modulation changes the sound but the faders on screen do not follow it (the modulation runs inside the engine); left as is. Intel Mac not tested (not requested).

## 0.1.0: Milestone 0 (empty effect)
- Empty Audio Unit effect (type `aufx`, shows under Audio FX in Logic) and VST3 effect. Stereo in, stereo out; a mono
  track is also accepted. The audio passes through unchanged. No MIDI. No parameters yet. Tail length 0.
- Placeholder window with the version and the "unofficial, not affiliated with Befaco" notice.
- Befaco Iroi firmware imported unmodified into `third_party/iroi-firmware` (30 files, commit
  e8b8dda, 1.0.1). It is not compiled yet. The workflow compares it byte for byte with Befaco's repository.
- Universal build (arm64 + x86_64), macOS 11.0 minimum, built on GitHub Actions (workflow from ELXAR-Two).
  Install and uninstall scripts. Zip contains licences, `SOURCE.txt` and the change list.
- Automatic plugin test (`tools/plugin_test`): 23 checks, run on Apple silicon and, through Rosetta, on Intel.
- Result: passed on Nov 11, 2026 in Logic 11.2 on the M4 Mac mini (macOS Tahoe). All 8 checks OK: build downloaded and installed, plugin appears under Audio FX > Audio Units > particledots > Iro-x and loads, audio sounds unchanged, window shows "Iro-x", "Version 0.1.0" and the unofficial notice, loads again after saving and reopening the project, uninstall removes it. Intel Mac not tested (not requested).
