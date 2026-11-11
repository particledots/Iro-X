# Changes Iro-x made to the Iroi source

Original: https://github.com/Befaco/Iroi (GPL-3.0, see LICENSE). Every change to a file in this folder will be marked
with the words `Iro-x change` in the file itself and listed here.

## Milestone 0 (2026-11-10)
No changes. All 30 files are unmodified copies of the commit named in SOURCE.txt.

## Milestone 1 (2026-11-11)
Every changed place carries the words `Iro-x change` in the file. All other files are still byte-for-byte copies of
the commit named in SOURCE.txt (the build checks this file by file).

1. **DelayLine.h, `DelayLine::read(float index)`**: the read position is limited to the buffer (0 to size - 2).
   Why: with the Ambience Spacetime control turned fully counter-clockwise (or pushed there by the modulation)
   the last of the six diffusers asks for a delay of about 49,700 samples, but its buffer (`kAmbienceBufferSize`)
   holds 48,000. The original code then reads up to about 1,700 samples past the end of the buffer (found by the
   address sanitizer in the engine test). On the module that reads whatever memory happens to follow; in a plugin it
   could read garbage or crash. With the change such a delay is the longest the buffer can give. Nothing else changes:
   for every delay inside the buffer the result is identical.

2. **Filter.h, `Allpass::readAt(int index)`**: a read further back than the buffer is long is limited to the length of
   the buffer. Why: the comb filter (filter mode 4) sets its second variable all-pass to twice the delay of the first,
   up to 1468 samples, in a buffer of 1468 samples. The interpolation then reads one sample further back (1469), which
   for one sample in every 1468 (when the write position is 0) is the memory just before the buffer: a garbage value
   in the audio once per cycle at the lowest comb pitches. With the change that read gives the oldest sample in the
   buffer. Everything inside the buffer is read exactly as before.

## Milestone 2 (2026-11-11)
No changes to the firmware files. Tempo sync, the randomiser and the tail length are done in Iro-x's own code
(src/engine/IroxEngine.cpp, src/Parameters.h), which drives the firmware's Clock and PatchCtrls from outside, as the
module's sync jack and Randomize routine would.
