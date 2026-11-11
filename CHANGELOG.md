# Changelog

## 0.1.0: Milestone 0 (empty effect)
- Empty Audio Unit effect (type `aufx`, shows under Audio FX in Logic) and VST3 effect. Stereo in, stereo out; a mono
  track is also accepted. The audio passes through unchanged. No MIDI. No parameters yet. Tail length 0.
- Placeholder window with the version and the "unofficial, not affiliated with Befaco" notice.
- Befaco Iroi firmware imported unmodified into `third_party/iroi-firmware` (30 files, commit
  e8b8dda, 1.0.1). It is not compiled yet. The workflow compares it byte for byte with Befaco's repository.
- Universal build (arm64 + x86_64), macOS 11.0 minimum, built on GitHub Actions (workflow from ELXAR-Two).
  Install and uninstall scripts. Zip contains licences, `SOURCE.txt` and the change list.
- Automatic plugin test (`tools/plugin_test`): 23 checks, run on Apple silicon and, through Rosetta, on Intel.
- Result: pending (test in Logic on the M4 Mac mini).
