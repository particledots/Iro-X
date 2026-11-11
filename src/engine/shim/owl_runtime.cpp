// Iro-x: the parts of the OpenWare run time that a computer does not have (see third_party/owlprogram/SOURCE.txt).
//  - message output (silent), assertion failure (stops the program, only used in debug builds)
//  - the audio buffer object (AudioBuffer::create / destroy)
//  - start-up of the fast logarithm and power lookup tables (the same tables OpenWare falls back to)
//  - randf(): one random number generator per thread, so that several plugin instances never share state
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <new>
#include <vector>
#include "AudioBuffer.h"
#include "FloatArray.h"
#include "message.h"
#include "basicmaths.h"
#include "FastLogTable.h"
#include "FastPowTable.h"
#include "owl_runtime.h"

extern "C" {
void debugMessage(const char*) {}
void error(int8_t code, const char* reason)
{
    std::fprintf(stderr, "Iro-x engine error %d: %s\n", (int) code, reason ? reason : "");
    std::abort();
}
void assert_failed(const char* msg, const char* location, int line)
{
    std::fprintf(stderr, "Iro-x engine assertion failed: %s (%s, line %d)\n", msg ? msg : "", location ? location : "", line);
    std::abort();
}
char* msg_itoa(int, int) { static char s[1] = { 0 }; return s; }
char* msg_ftoa(float, int) { static char s[1] = { 0 }; return s; }
}
void debugMessage(const char*, int) {}
void debugMessage(const char*, int, int) {}
void debugMessage(const char*, int, int, int) {}
void debugMessage(const char*, float) {}
void debugMessage(const char*, float, float) {}
void debugMessage(const char*, float, float, float) {}
void assert_failed(uint8_t*, uint32_t) { std::abort(); }

// ---- audio buffer ----
AudioBuffer::~AudioBuffer() {}

namespace
{
class OwnedAudioBuffer : public AudioBuffer
{
public:
    OwnedAudioBuffer (int channels, int samples) : channels_ (channels), size_ (samples), data_ ((size_t) channels * (size_t) samples, 0.0f) {}
    FloatArray getSamples (int channel) override { return FloatArray (data_.data() + (size_t) channel * (size_t) size_, (size_t) size_); }
    int getChannels() override { return channels_; }
    int getSize() override { return size_; }
    void clear() override { std::fill (data_.begin(), data_.end(), 0.0f); }
private:
    int channels_, size_;
    std::vector<float> data_;
};
}

AudioBuffer* AudioBuffer::create (int channels, int samples) { return new OwnedAudioBuffer (channels, samples); }
void AudioBuffer::destroy (AudioBuffer* buffer) { delete buffer; }

// ---- random numbers: xorshift32, one state per thread ----
namespace
{
thread_local uint32_t randomState = 33641u;
}
extern "C" float randf()
{
    randomState ^= randomState << 13;
    randomState ^= randomState >> 17;
    randomState ^= randomState << 5;
    return (float) randomState * (1.0f / 4294967296.0f);
}
void irox_seed_random (uint32_t seed) { randomState = seed != 0 ? seed : 33641u; }
uint32_t irox_random_state() { return randomState; }

// ---- lookup tables (set once, read only afterwards) ----
void irox_init_owl_runtime()
{
    static std::once_flag once;
    std::call_once (once, []
    {
        fast_log_set_table (fast_log_table, (int) fast_log_table_size);
        fast_pow_set_table (fast_pow_table, (int) fast_pow_table_size);
    });
}

// ---- zeroed memory while the engine is being built ----
// On the module OpenWare's allocator (pvPortMalloc, heap_5.c) clears every block it hands out, and the firmware
// relies on it: a number of its members (for example the dry/wet values) are never given a start value. So while an
// Engine is constructed, everything allocated by this thread is cleared the same way. At every other time the normal
// allocator is used, unchanged.
namespace
{
thread_local int zeroDepth = 0;
void* allocate (std::size_t n, bool mayThrow)
{
    void* p = std::malloc (n != 0 ? n : 1);
    if (p == nullptr) { if (mayThrow) throw std::bad_alloc(); return nullptr; }
    if (zeroDepth > 0) std::memset (p, 0, n);
    return p;
}
}
irox_ZeroedAllocations::irox_ZeroedAllocations() { ++zeroDepth; }
irox_ZeroedAllocations::~irox_ZeroedAllocations() { --zeroDepth; }

void* operator new (std::size_t n) { return allocate (n, true); }
void* operator new[] (std::size_t n) { return allocate (n, true); }
void* operator new (std::size_t n, const std::nothrow_t&) noexcept { return allocate (n, false); }
void* operator new[] (std::size_t n, const std::nothrow_t&) noexcept { return allocate (n, false); }
void operator delete (void* p) noexcept { std::free (p); }
void operator delete[] (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept { std::free (p); }
void operator delete (void* p, const std::nothrow_t&) noexcept { std::free (p); }
void operator delete[] (void* p, const std::nothrow_t&) noexcept { std::free (p); }
