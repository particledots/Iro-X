#pragma once
// macOS has no exp10f (glibc has). The OwlProgram library uses it in its non-ARM fallback.
// Force-included for the engine on macOS only (see CMakeLists.txt). Same maths, written with powf.
#include <math.h>
#ifndef exp10f
#define exp10f(x) powf (10.0f, (x))
#endif
