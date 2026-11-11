#pragma once
#include <cstdint>
// Start the lookup tables the firmware's fast maths needs. Safe to call many times, from any thread.
void irox_init_owl_runtime();
// Random numbers are per thread. Tests set the seed to get repeatable output.
void irox_seed_random (uint32_t seed);
uint32_t irox_random_state();
// While one of these exists on a thread, memory allocated by that thread is cleared (see owl_runtime.cpp).
struct irox_ZeroedAllocations { irox_ZeroedAllocations(); ~irox_ZeroedAllocations(); };
