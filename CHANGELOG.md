# Changelog

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
- Result: pending (Logic 11.2 on the M4 Mac mini).

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
