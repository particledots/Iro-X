// Iro-x stand-in for OpenWare's Patch.h.
// The Iroi firmware includes "Patch.h" to get the audio and maths types of the OpenWare platform. On the hardware
// this file defines the Patch base class and the knob / button ids. The effects engine only needs the types, so
// only the types are provided here. (The hardware layer of the firmware, Ui.h and friends, is not compiled in
// Iro-x: the plugin parameters feed the engine directly, see src/engine/IroxEngine.cpp.)
#pragma once

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <utility>
#include "basicmaths.h"
#include "FloatArray.h"
#include "AudioBuffer.h"
#include "SignalProcessor.h"
#include "SignalGenerator.h"

#ifndef LEFT_CHANNEL
#define LEFT_CHANNEL 0
#define RIGHT_CHANNEL 1
#endif
