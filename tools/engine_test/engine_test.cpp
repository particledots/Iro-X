// Iro-x engine test: runs test signals through the real engine (no host, no window, no JUCE).
//  1. the resampler and block FIFO around the engine (engine skipped): delay, accuracy, anti-aliasing, no underruns
//  2. every effect section on its own: sound comes out, stays finite, behaves as expected; writes WAV files
//  3. a long random-parameter run (finds crashes and out-of-range numbers when built with the sanitizers)
//  4. several instances at once do not disturb each other
// Not part of the download. Usage: irox_engine_test [folder for WAV files]
#include <cmath>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include "IroxStream.h"
#if defined(__SSE__)
#include <xmmintrin.h>
#endif

using namespace irox;

namespace
{
int failures = 0;
void check (bool ok, const std::string& what)
{
    std::printf ("%-86s %s\n", what.c_str(), ok ? "ok" : "FAIL");
    if (! ok) ++failures;
}
std::string fmt (const char* f, ...)
{
    char b[512];
    va_list a; va_start (a, f); std::vsnprintf (b, sizeof b, f, a); va_end (a);
    return b;
}

struct Rng { uint32_t s; float next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return (float) s * (1.f / 4294967296.f); } };

struct Audio { std::vector<float> l, r; double rate = 48000; size_t size() const { return l.size(); } };

double rms (const std::vector<float>& v, size_t a, size_t b)
{
    b = std::min (b, v.size()); if (a >= b) return 0;
    double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i];
    return std::sqrt (s / (double) (b - a));
}
double peak (const std::vector<float>& v) { double p = 0; for (float x : v) p = std::max (p, (double) std::fabs (x)); return p; }
double db (double x) { return 20.0 * std::log10 (std::max (x, 1e-12)); }

void writeWav (const std::string& path, const Audio& a)
{
    FILE* f = std::fopen (path.c_str(), "wb"); if (! f) return;
    const uint32_t n = (uint32_t) a.size(), ch = 2, rate = (uint32_t) a.rate, dataBytes = n * ch * 4, riff = 36 + dataBytes;
    const uint16_t fmtTag = 3, chans = 2, bits = 32, align = (uint16_t) (ch * 4); const uint32_t fmtLen = 16, byteRate = rate * ch * 4;
    std::fwrite ("RIFF", 1, 4, f); std::fwrite (&riff, 4, 1, f); std::fwrite ("WAVEfmt ", 1, 8, f); std::fwrite (&fmtLen, 4, 1, f);
    std::fwrite (&fmtTag, 2, 1, f); std::fwrite (&chans, 2, 1, f); std::fwrite (&rate, 4, 1, f); std::fwrite (&byteRate, 4, 1, f);
    std::fwrite (&align, 2, 1, f); std::fwrite (&bits, 2, 1, f); std::fwrite ("data", 1, 4, f); std::fwrite (&dataBytes, 4, 1, f);
    for (size_t i = 0; i < a.size(); ++i) { std::fwrite (&a.l[i], 4, 1, f); std::fwrite (&a.r[i], 4, 1, f); }
    std::fclose (f);
}

// Runs the audio through a Stream in host blocks of the given sizes (cycled).
Audio run (Stream& st, const Audio& in, const std::vector<int>& blocks)
{
    Audio out; out.rate = in.rate; out.l.resize (in.size()); out.r.resize (in.size());
    size_t pos = 0, bi = 0;
    while (pos < in.size())
    {
        const int n = (int) std::min<size_t> ((size_t) blocks[bi++ % blocks.size()], in.size() - pos);
        st.process (in.l.data() + pos, in.r.data() + pos, out.l.data() + pos, out.r.data() + pos, n);
        pos += (size_t) n;
    }
    return out;
}
Audio runOnce (Stream& st, const Audio& in, int block = 512) { return run (st, in, { block }); }

Audio makeSine (double rate, double seconds, std::vector<std::pair<double, double>> partials)
{
    Audio a; a.rate = rate; const size_t n = (size_t) (rate * seconds); a.l.resize (n); a.r.resize (n);
    for (size_t i = 0; i < n; ++i)
    {
        double v = 0; for (auto& p : partials) v += p.second * std::sin (2.0 * M_PI * p.first * (double) i / rate);
        a.l[i] = (float) v; a.r[i] = (float) v;
    }
    return a;
}
Audio makeNoise (double rate, double seconds, float amp, uint32_t seed = 1)
{
    Audio a; a.rate = rate; const size_t n = (size_t) (rate * seconds); a.l.resize (n); a.r.resize (n); Rng g { seed };
    for (size_t i = 0; i < n; ++i) { a.l[i] = (g.next() * 2 - 1) * amp; a.r[i] = (g.next() * 2 - 1) * amp; }
    return a;
}
Audio makeSilence (double rate, double seconds) { Audio a; a.rate = rate; a.l.assign ((size_t) (rate * seconds), 0.f); a.r = a.l; return a; }
}

// ---------------------------------------------------------------------------------------------------------------
static void testStream()
{
    std::printf ("\n== 1. resampler and block FIFO (engine skipped) ==\n");
    const std::vector<int> odd = { 1, 7, 32, 33, 64, 100, 511, 1, 2, 1024, 13, 31, 480, 4096, 5, 256 };
    for (double rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        Stream st; st.prepare (rate, 4096); st.setTestBypass (true);
        const int lat = st.latencySamples();
        auto in = makeSine (rate, 3.0, { { 1000.0, 0.4 }, { 7300.0, 0.3 } });
        auto out = run (st, in, odd);
        // out[t] should equal in[t - lat] (the engine is skipped, so only the delay and the resamplers show)
        double err = 0, sig = 0; const size_t skip = (size_t) (0.2 * rate);
        for (size_t t = skip + (size_t) lat; t < in.size(); ++t)
        {
            const double d = (double) out.l[t] - (double) in.l[t - (size_t) lat];
            err += d * d; sig += (double) in.l[t - (size_t) lat] * in.l[t - (size_t) lat];
        }
        // fractional delay mismatch is possible by up to one sample; search for the best delay around lat
        double best = 1e9; int bestLag = 0;
        for (int lag = lat - 3; lag <= lat + 3; ++lag)
        {
            double e = 0; for (size_t t = skip + 8; t + 8 < in.size(); ++t) { const double d = (double) out.l[t] - (double) in.l[t - (size_t) lag]; e += d * d; }
            if (e < best) { best = e; bestLag = lag; }
        }
        const double snr = db (std::sqrt (best / (double) (in.size() - skip - 16))) - db (std::sqrt (sig / (double) (in.size() - skip - (size_t) lat)));
        check (st.underruns() == 0, fmt ("%6.0f Hz: no underruns with odd block sizes (1 to 4096), latency %d samples", rate, lat));
        check (std::abs (bestLag - lat) <= 1, fmt ("%6.0f Hz: signal appears %d samples late, reported %d", rate, bestLag, lat));
        check (snr < -70.0, fmt ("%6.0f Hz: output equals the delayed input, error %.1f dB below the signal", rate, -snr));
        (void) err;
    }
    // passband and anti-aliasing at the common rates
    for (double rate : { 44100.0, 96000.0 })
    {
        Stream st; st.prepare (rate, 1024); st.setTestBypass (true);
        auto in = makeSine (rate, 1.0, { { 18000.0, 0.5 } });
        auto out = runOnce (st, in);
        const double g = db (rms (out.l, (size_t) (0.5 * rate), out.size()) / rms (in.l, 0, in.size()));
        check (std::fabs (g) < 0.3, fmt ("%6.0f Hz: an 18 kHz tone passes with %.2f dB gain", rate, g));
    }
    {
        const double rate = 96000.0;
        Stream st; st.prepare (rate, 1024); st.setTestBypass (true);
        auto in = makeSine (rate, 1.0, { { 30000.0, 0.5 } });
        auto out = runOnce (st, in);
        const double g = db (rms (out.l, (size_t) (0.5 * rate), out.size()) / rms (in.l, 0, in.size()));
        check (g < -70.0, fmt ("  96000 Hz: a 30 kHz tone (above the engine's 24 kHz) is removed, %.1f dB", g));
    }
    {
        const double rate = 44100.0;
        Stream st; st.prepare (rate, 1024); st.setTestBypass (true);
        auto in = makeSine (rate, 1.0, { { 21600.0, 0.5 } });
        auto out = runOnce (st, in);
        const double g = db (rms (out.l, (size_t) (0.5 * rate), out.size()) / rms (in.l, 0, in.size()));
        std::printf ("   (44.1 kHz, a 21.6 kHz tone near the host's Nyquist frequency: %.1f dB)\n", g);
    }
}

// ---------------------------------------------------------------------------------------------------------------
static EngineParams base()
{
    EngineParams p;          // defaults: filter wide open, resonator / echo / ambience dry
    return p;
}

static void testSections (const std::string& outDir)
{
    std::printf ("\n== 2. every effect section ==\n");
    const double rate = 48000.0;
    auto save = [&] (const char* name, const Audio& a) { if (! outDir.empty()) writeWav (outDir + "/" + name + ".wav", a); };

    // default settings: audio goes through without trouble
    {
        Stream st; st.prepare (rate, 512); st.setParams (base(), 0.f, 0.f);
        auto in = makeSine (rate, 2.0, { { 220.0, 0.3 }, { 1800.0, 0.2 } });
        auto out = runOnce (st, in);
        const double g = db (rms (out.l, 24000, out.size()) / rms (in.l, 24000, in.size()));
        save ("default", out);
        check (st.nonFiniteCount() == 0 && std::isfinite (peak (out.l)), "default settings: output is finite");
        check (g > -12.0 && g < 6.0, fmt ("default settings: level change %.1f dB (filter open, effects dry)", g));
    }
    // filter: low pass attenuates high frequencies
    {
        auto lp = [&] (double f)
        {
            Stream st; st.prepare (rate, 512); EngineParams p = base(); p.filterCutoff = 0.5f; p.filterResonance = 0.1f; st.setParams (p, 0.f, 0.f);
            auto in = makeSine (rate, 1.5, { { f, 0.3 } }); auto out = runOnce (st, in);
            return db (rms (out.l, 36000, out.size()));
        };
        const double lo = lp (150.0), hi = lp (6000.0);
        check (lo - hi > 20.0, fmt ("filter low pass (cutoff about 640 Hz): 150 Hz %.1f dB, 6 kHz %.1f dB", lo, hi));
        EngineParams p = base(); p.filterCutoff = 0.5f;
        for (float mode : { 0.375f, 0.625f, 0.875f })
        {
            Stream st; st.prepare (rate, 512); p.filterMode = mode; p.filterResonance = 0.4f; st.setParams (p, 0.f, 0.f);
            auto in = makeNoise (rate, 2.0, 0.3f); auto out = runOnce (st, in);
            check (st.nonFiniteCount() == 0 && rms (out.l, 24000, out.size()) > 1e-3, fmt ("filter mode %.3f (band pass, high pass, comb): sound comes out, rms %.1f dB", mode, db (rms (out.l, 24000, out.size()))));
            save (mode < 0.5f ? "filter_bandpass" : mode < 0.7f ? "filter_highpass" : "filter_comb", out);
        }
        // level 0 = the VCA is closed
        Stream st; st.prepare (rate, 512); p = base(); p.filterLevel = 0.f; st.setParams (p, 0.f, 0.f);
        auto in = makeNoise (rate, 1.0, 0.3f); auto out = runOnce (st, in);
        check (rms (out.l, 12000, out.size()) < 1e-4, "filter level 0: the VCA is closed, silence");
        // filter position: every position works
        for (float pos : { 0.125f, 0.375f, 0.625f, 0.875f })
        {
            Stream s2; s2.prepare (rate, 512); p = base(); p.filterPosition = pos; p.filterCutoff = 0.5f; p.resonatorLevel = 0.5f; p.echoLevel = 0.5f; p.ambienceLevel = 0.5f; s2.setParams (p, 0.f, 0.f);
            auto o2 = runOnce (s2, makeNoise (rate, 1.5, 0.2f));
            check (s2.nonFiniteCount() == 0 && rms (o2.l, 12000, o2.size()) > 1e-3, fmt ("filter position %.3f: sound comes out", pos));
        }
    }
    // resonator
    {
        Stream st; st.prepare (rate, 512); EngineParams p = base(); p.resonatorLevel = 1.f; p.resonatorFeedback = 0.9f; p.resonatorTune = 0.5f; st.setParams (p, 0.f, 0.f);
        auto in = makeSilence (rate, 2.0); for (int i = 0; i < 480; ++i) { in.l[(size_t) i] = in.r[(size_t) i] = (float) std::sin (i * 0.3) * 0.5f; }
        auto out = runOnce (st, in); save ("resonator_ring", out);
        const double tail = rms (out.l, 9600, 19200);
        check (st.nonFiniteCount() == 0 && tail > 1e-3, fmt ("resonator: a short burst keeps ringing (rms %.1f dB between 0.2 and 0.4 s)", db (tail)));
        for (float tune : { 0.f, 0.25f, 0.75f, 1.f })
        {
            Stream s2; s2.prepare (rate, 512); p.resonatorTune = tune; p.resonatorFeedback = 1.f; p.resonatorDissonance = 0.8f; s2.setParams (p, 0.f, 0.f);
            auto o2 = runOnce (s2, makeNoise (rate, 2.0, 0.2f));
            check (s2.nonFiniteCount() == 0 && peak (o2.l) < 40.0, fmt ("resonator tune %.2f, feedback full, peak %.2f: stays finite", tune, peak (o2.l)));
        }
    }
    // echo
    {
        Stream st; st.prepare (rate, 512); EngineParams p = base(); p.echoLevel = 1.f; p.echoRepeats = 0.6f; p.echoDensity = 0.3f; st.setParams (p, 0.f, 0.f);
        auto in = makeSilence (rate, 4.0); for (int i = 0; i < 200; ++i) in.l[(size_t) i] = in.r[(size_t) i] = 0.6f * (float) std::sin (i * 0.2);
        auto out = runOnce (st, in); save ("echo", out);
        check (st.nonFiniteCount() == 0 && rms (out.l, 12000, 96000) > 1e-4, fmt ("echo: repeats arrive after the input has ended (rms %.1f dB, 0.25 to 2 s)", db (rms (out.l, 12000, 96000))));
        for (float rep : { 0.f, 0.99f, 1.f })
            for (float dens : { 0.f, 0.5f, 1.f })
            {
                Stream s2; s2.prepare (rate, 512); p.echoRepeats = rep; p.echoDensity = dens; s2.setParams (p, 0.f, 0.f);
                auto o2 = runOnce (s2, makeNoise (rate, 2.0, 0.2f));
                if (s2.nonFiniteCount() != 0 || peak (o2.l) > 40.0) check (false, fmt ("echo repeats %.2f density %.2f: peak %.2f", rep, dens, peak (o2.l)));
            }
        check (true, "echo: repeats 0, 0.99, 1 with density 0, 0.5, 1 stay finite and below 40");
    }
    // ambience
    {
        Stream st; st.prepare (rate, 512); EngineParams p = base(); p.ambienceLevel = 1.f; p.ambienceDecay = 0.7f; st.setParams (p, 0.f, 0.f);
        auto in = makeSilence (rate, 6.0); for (int i = 0; i < 300; ++i) in.l[(size_t) i] = in.r[(size_t) i] = 0.7f * (float) std::sin (i * 0.15);
        auto out = runOnce (st, in); save ("ambience", out);
        check (st.nonFiniteCount() == 0 && rms (out.l, 14400, 48000) > 1e-4, fmt ("ambience: a tail follows the input (rms %.1f dB, 0.3 to 1 s)", db (rms (out.l, 14400, 48000))));
        for (float decay : { 0.f, 1.f })
            for (float space : { 0.f, 0.25f, 0.75f, 1.f })
            {
                Stream s2; s2.prepare (rate, 512); p.ambienceDecay = decay; p.ambienceSpacetime = space; p.ambienceAutoPan = 0.5f; s2.setParams (p, 0.f, 0.f);
                auto o2 = runOnce (s2, makeNoise (rate, 2.0, 0.2f));
                if (s2.nonFiniteCount() != 0 || peak (o2.l) > 40.0) check (false, fmt ("ambience decay %.2f spacetime %.2f: peak %.2f", decay, space, peak (o2.l)));
            }
        check (true, "ambience: decay 0 and 1 with spacetime 0 to 1 (auto-pan on) stay finite and below 40");
    }
    // modulation
    {
        for (float type : { 0.f, 1.f / 6, 2.f / 6, 0.5f, 4.f / 6, 5.f / 6, 0.999f })
        {
            Stream st; st.prepare (rate, 512); EngineParams p = base(); p.modLevel = 1.f; p.modSpeed = 0.8f; p.modType = type; p.filterCutoffMod = 1.f; p.filterCutoff = 0.5f; p.filterResonance = 0.3f; st.setParams (p, 0.f, 0.f);
            auto in = makeNoise (rate, 6.0, 0.3f);
            float lo = 9, hi = -9; Audio out; out.rate = rate;
            const size_t n = in.size(); out.l.resize (n); out.r.resize (n);
            for (size_t pos = 0; pos < n; pos += 480)
            {
                st.process (in.l.data() + pos, in.r.data() + pos, out.l.data() + pos, out.r.data() + pos, 480);
                if (pos > 48000) { lo = std::min (lo, st.modulationValue()); hi = std::max (hi, st.modulationValue()); }
            }
            check (st.nonFiniteCount() == 0 && std::isfinite (lo), fmt ("modulation type %.3f: modulation moves between %.2f and %.2f, output finite", type, lo, hi));
        }
    }
}

// ---------------------------------------------------------------------------------------------------------------
static EngineParams randomParams (Rng& g)
{
    EngineParams p; float* f = &p.filterLevel; const size_t count = sizeof (EngineParams) / sizeof (float);
    for (size_t i = 0; i < count; ++i)
    {
        float v = g.next();
        const float c = g.next();
        if (c < 0.12f) v = 0.f; else if (c < 0.24f) v = 1.f; else if (c < 0.28f) v = 0.5f;
        f[i] = v;
    }
    return p;
}

static void testFuzz()
{
    std::printf ("\n== 3. random parameters, long run ==\n");
    for (double rate : { 48000.0, 44100.0, 96000.0 })
    {
        Rng g { 12345u + (uint32_t) rate };
        Stream st; st.prepare (rate, 1024);
        const size_t blocks = (size_t) (rate * 40.0 / 480.0);
        double pk = 0; std::vector<float> l (480), r (480), ol (480), orr (480);
        for (size_t b = 0; b < blocks; ++b)
        {
            if (b % 25 == 0) st.setParams (randomParams (g), (g.next() - 0.5f) * 40.f, (g.next() - 0.5f) * 40.f);
            const float kind = g.next();
            for (int i = 0; i < 480; ++i)
            {
                float v = (g.next() * 2 - 1) * 0.5f;
                if (kind < 0.2f) v = (i % 97 == 0) ? 0.9f : 0.f;                         // clicks
                else if (kind < 0.4f) v = 0.7f * std::sin ((float) (b * 480 + i) * 0.05f); // tone
                else if (kind < 0.5f) v = 0.f;                                             // silence
                l[(size_t) i] = v; r[(size_t) i] = kind < 0.6f ? v : (g.next() * 2 - 1) * 0.5f;
            }
            st.process (l.data(), r.data(), ol.data(), orr.data(), 480);
            for (int i = 0; i < 480; ++i) pk = std::max ({ pk, (double) std::fabs (ol[(size_t) i]), (double) std::fabs (orr[(size_t) i]) });
        }
        check (st.nonFiniteCount() == 0 && st.underruns() == 0, fmt ("%6.0f Hz: 40 s of random parameter changes: no invalid numbers, no underruns (peak %.1f)", rate, pk));
    }
}

// ---------------------------------------------------------------------------------------------------------------
static void testInstances()
{
    std::printf ("\n== 4. several instances ==\n");
    const double rate = 48000.0;
    EngineParams p = base();
    p.filterCutoff = 0.6f; p.filterResonance = 0.4f; p.resonatorLevel = 0.5f; p.resonatorFeedback = 0.7f; p.echoLevel = 0.5f; p.ambienceLevel = 0.5f;
    p.modLevel = 0.8f; p.modType = 1.f / 6; p.modSpeed = 0.7f; p.filterCutoffMod = 0.4f; p.echoDensityMod = 0.5f;     // sine modulation: no random numbers
    auto in = makeNoise (rate, 3.0, 0.3f, 7);
    // reference: one instance on its own
    Stream a; a.prepare (rate, 512); a.setParams (p, 0.f, 0.f);
    auto ref = run (a, in, { 480 });
    // three instances, interleaved block by block, with different block sizes; instance 2 gets other settings
    constexpr int N = 3;
    Stream s[N]; Audio out[N]; size_t pos[N] = { 0, 0, 0 }; const int bs[N] = { 480, 100, 777 };
    EngineParams other = p; other.resonatorTune = 0.1f; other.echoRepeats = 0.9f; other.filterMode = 0.875f;
    for (int i = 0; i < N; ++i) { s[i].prepare (rate, 1024); s[i].setParams (i == 2 ? other : p, 0.f, 0.f); out[i].rate = rate; out[i].l.resize (in.size()); out[i].r.resize (in.size()); }
    bool more = true;
    while (more)
    {
        more = false;
        for (int i = 0; i < N; ++i)
        {
            if (pos[i] >= in.size()) continue;
            const int n = (int) std::min<size_t> ((size_t) bs[i], in.size() - pos[i]);
            s[i].process (in.l.data() + pos[i], in.r.data() + pos[i], out[i].l.data() + pos[i], out[i].r.data() + pos[i], n);
            pos[i] += (size_t) n; more = true;
        }
    }
    check (std::memcmp (ref.l.data(), out[0].l.data(), ref.l.size() * sizeof (float)) == 0 && std::memcmp (ref.r.data(), out[0].r.data(), ref.r.size() * sizeof (float)) == 0,
           "instance 1 (interleaved with two others, other block sizes) is bit-identical to a single instance");
    check (std::memcmp (ref.l.data(), out[1].l.data(), ref.l.size() * sizeof (float)) == 0,
           "instance 2 (same settings, block size 100) is bit-identical too: host block size does not change the sound");
    check (std::memcmp (ref.l.data(), out[2].l.data(), ref.l.size() * sizeof (float)) != 0, "instance 3 with other settings sounds different");
}


// ---------------------------------------------------------------------------------------------------------------
// Runs the stream in 480-sample host blocks, telling it the tempo (and song position) before every block.
static Audio runTempo (Stream& st, const Audio& in, double bpm, bool playing, double ppqStart, bool hasPpq, int block, std::vector<double>* phases = nullptr)
{
    Audio out; out.rate = in.rate; out.l.resize (in.size()); out.r.resize (in.size());
    double ppq = ppqStart;
    for (size_t pos = 0; pos < in.size(); pos += (size_t) block)
    {
        const int n = (int) std::min<size_t> ((size_t) block, in.size() - pos);
        Transport t; t.bpm = bpm; t.playing = playing; t.hasPpq = hasPpq; t.ppq = ppq;
        st.setTransport (t);
        st.process (in.l.data() + pos, in.r.data() + pos, out.l.data() + pos, out.r.data() + pos, n);
        if (phases) phases->push_back (st.beatPhase());
        if (playing) ppq += bpm / 60.0 * (double) n / in.rate;
    }
    return out;
}

// Modulation rate in Hz measured from the modulation value (upward crossings of its mean).
static double modulationHz (double bpm, bool withTempo, double rate = 48000.0)
{
    Stream st; st.prepare (rate, 1024);
    EngineParams p = base(); p.modLevel = 1.f; p.modSpeed = 0.5f; p.modType = 1.f / 6; st.setParams (p, 0.f, 0.f);   // sine, speed at the centre = one cycle per beat
    const int block = 480; const double seconds = 40.0;
    auto in = makeNoise (rate, seconds, 0.1f);
    std::vector<float> m;
    for (size_t pos = 0; pos + block <= in.size(); pos += (size_t) block)
    {
        std::vector<float> o1 (block), o2 (block);
        if (withTempo) { Transport t; t.bpm = bpm; st.setTransport (t); }
        st.process (in.l.data() + pos, in.r.data() + pos, o1.data(), o2.data(), block);
        m.push_back (st.modulationValue());
    }
    // skip the first 10 s, count rising crossings of the mean
    const size_t from = (size_t) (10.0 * rate / block);
    double mean = 0; for (size_t i = from; i < m.size(); ++i) mean += m[i]; mean /= (double) (m.size() - from);
    int cross = 0; size_t first = 0, last = 0;
    for (size_t i = from + 1; i < m.size(); ++i)
        if (m[i - 1] < mean && m[i] >= mean) { if (cross == 0) first = i; last = i; ++cross; }
    if (cross < 3) return 0.0;
    return (double) (cross - 1) / (((double) (last - first)) * block / rate);
}

static double firstEchoSeconds (double bpm, bool withTempo)
{
    const double rate = 48000.0;
    Stream st; st.prepare (rate, 1024);
    EngineParams p = base(); p.echoLevel = 1.f; p.echoRepeats = 0.3f; p.echoDensity = 0.5f; st.setParams (p, 0.f, 0.f);
    auto in = makeSilence (rate, 6.0);
    // let the clock settle for 3 s, then one click
    const size_t clickAt = (size_t) (3.0 * rate);
    in.l[clickAt] = in.r[clickAt] = 0.8f;
    Audio out = withTempo ? runTempo (st, in, bpm, false, 0.0, false, 480) : run (st, in, { 480 });
    // time of the first sample after the click that is louder than 3 % of the click (the dry click itself is at +latency)
    const size_t lat = (size_t) st.latencySamples();
    for (size_t i = clickAt + lat + 2000; i < out.size(); ++i)
        if (std::fabs (out.l[i]) > 0.02f) return (double) (i - clickAt - lat) / rate;
    return -1.0;
}

static void testTempo()
{
    std::printf ("\n== 5. Logic's tempo (the module's sync input) ==\n");
    const double internalHz = modulationHz (0, false);
    check (std::fabs (internalHz - 1.0) < 0.05, fmt ("no tempo from the host: modulation runs on the internal clock, %.3f Hz (1 Hz = 60 BPM)", internalHz));
    for (double bpm : { 60.0, 90.0, 120.0, 150.0, 200.0 })
    {
        const double hz = modulationHz (bpm, true);
        check (std::fabs (hz - bpm / 60.0) < 0.03 * bpm / 60.0 + 0.02, fmt ("host tempo %5.1f BPM: modulation at speed 'one per beat' runs at %.3f Hz (expected %.3f)", bpm, hz, bpm / 60.0));
    }
    {   // a rate other than 48 kHz
        const double hz = modulationHz (110.0, true, 44100.0);
        check (std::fabs (hz - 110.0 / 60.0) < 0.06, fmt ("44.1 kHz, 110 BPM: modulation %.3f Hz (expected %.3f)", hz, 110.0 / 60.0));
    }
    // echo times follow the tempo
    {
        const double t60 = firstEchoSeconds (60.0, true), t120 = firstEchoSeconds (120.0, true), t90 = firstEchoSeconds (90.0, true);
        check (t60 > 0 && t120 > 0, fmt ("echo follows the host tempo: first repeat at %.3f s (60 BPM), %.3f s (90 BPM), %.3f s (120 BPM)", t60, t90, t120));
        check (t60 > 0 && t120 > 0 && std::fabs (t60 / t120 - 2.0) < 0.15, fmt ("echo time at 60 BPM is %.2f times the time at 120 BPM (expected 2)", t60 / std::max (t120, 1e-9)));
    }
    // song position: the clock follows the beats of the song
    {
        const double rate = 48000.0;
        Stream st; st.prepare (rate, 1024); st.setParams (base(), 0.f, 0.f);
        auto in = makeNoise (rate, 4.0, 0.1f);
        std::vector<double> ph;
        runTempo (st, in, 120.0, true, 3.7, true, 480, &ph);
        // 4 s at 120 BPM = 8 beats; ppq at the end is 3.7 + 8 - (the last block's length) -> compare phase after the last block
        const double endPpq = 3.7 + 120.0 / 60.0 * 4.0 - 120.0 / 60.0 * 32.0 / rate;      // end of the last block, minus the engine's 32-sample delay
        double d = ph.back() - (endPpq - std::floor (endPpq)); d -= std::floor (d + 0.5);
        check (std::fabs (d) < 0.01, fmt ("playing from beat 3.7: the clock stays on the beat (off by %.4f of a beat)", d));
        // a jump to another position: the clock follows within one block
        Stream s2; s2.prepare (rate, 1024); s2.setParams (base(), 0.f, 0.f);
        Transport t; t.bpm = 100.0; t.playing = true; t.hasPpq = true; t.ppq = 1.0;
        std::vector<float> a (480, 0.f), b (480, 0.f), o1 (480), o2 (480);
        for (int i = 0; i < 40; ++i) { t.ppq = 1.0 + i * 100.0 / 60.0 * 480.0 / rate; s2.setTransport (t); s2.process (a.data(), b.data(), o1.data(), o2.data(), 480); }
        t.ppq = 20.5; s2.setTransport (t); s2.process (a.data(), b.data(), o1.data(), o2.data(), 480);
        const double jp = s2.beatPhase(); double dj = jp - 0.5; dj -= std::floor (dj + 0.5);
        check (std::fabs (dj) < 0.06, fmt ("a jump in the song (loop, locate): the clock is back on the beat at once (phase %.3f, song position .5)", jp));
        // stopped: the clock keeps its tempo
        Stream s3; s3.prepare (rate, 1024); s3.setParams (base(), 0.f, 0.f);
        Transport ts; ts.bpm = 120.0; ts.playing = false;
        double p0 = 0, p1 = 0;
        for (int i = 0; i < 100; ++i) { s3.setTransport (ts); s3.process (a.data(), b.data(), o1.data(), o2.data(), 480); if (i == 49) p0 = s3.beatPhase(); }
        p1 = s3.beatPhase();
        double adv = p1 - p0; adv -= std::floor (adv);
        // 50 blocks of 480 = 0.5 s = exactly 1 beat at 120 BPM -> phase back at the same place
        check (std::fabs (adv) < 0.02 || std::fabs (adv - 1.0) < 0.02, fmt ("stopped: the clock keeps running at the tempo (one beat later the phase moved by %.3f)", adv));
    }
    // tempo changes and extreme tempos stay safe
    {
        const double rate = 48000.0;
        Stream st; st.prepare (rate, 1024);
        EngineParams p = base(); p.echoLevel = 0.6f; p.ambienceLevel = 0.5f; p.modLevel = 1.f; p.modType = 0.3f; p.modSpeed = 0.7f; p.ambienceAutoPan = 0.6f; p.echoDensity = 0.7f; p.echoRepeats = 0.9f;
        st.setParams (p, 0.f, 0.f);
        Rng g { 99 }; std::vector<float> a (480), b (480), o1 (480), o2 (480); bool ok = true; double pk = 0;
        for (int i = 0; i < 1500; ++i)
        {
            Transport t; t.playing = (i / 200) % 2 == 0; t.hasPpq = (i % 3) != 0; t.ppq = (double) i * 0.31;
            const float c = g.next();
            t.bpm = c < 0.1f ? 0.0 : c < 0.2f ? 1.0 : c < 0.3f ? 999.0 : c < 0.4f ? 20.0 + g.next() * 500.0 : 60.0 + 80.0 * std::sin (i * 0.01);
            st.setTransport (t);
            for (int k = 0; k < 480; ++k) { a[(size_t) k] = (g.next() - 0.5f) * 0.5f; b[(size_t) k] = a[(size_t) k]; }
            st.process (a.data(), b.data(), o1.data(), o2.data(), 480);
            for (int k = 0; k < 480; ++k) pk = std::max (pk, (double) std::fabs (o1[(size_t) k]));
        }
        ok = st.nonFiniteCount() == 0 && st.underruns() == 0 && pk < 40.0;
        check (ok, fmt ("30 s of jumping tempos (0, 1, 20 to 999 BPM), start/stop and jumps: finite, no underruns (peak %.1f)", pk));
    }
}

// The plugin runs with denormal numbers flushed to zero (ScopedNoDenormals); the test does the same, otherwise the long
// decaying tails would take many times longer to compute.
static void flushDenormals()
{
#if defined(__SSE__)
    _mm_setcsr (_mm_getcsr() | 0x8040);
#elif defined(__aarch64__)
    unsigned long fpcr; __asm__ volatile ("mrs %0, fpcr" : "=r" (fpcr)); fpcr |= (1ul << 24); __asm__ volatile ("msr fpcr, %0" :: "r" (fpcr));
#endif
}

// How long a short noise burst keeps sounding: time of the last 0.1 s window that is louder than thrDb (dBFS, rms of both
// channels). Gives up at maxSec (returns maxSec).
static double tailSeconds (const EngineParams& p, double thrDb, double maxSec)
{
    Stream st; st.prepare (48000.0, 480); st.setParams (p, 0.f, 0.f);
    const double thr = std::pow (10.0, thrDb / 20.0);
    const size_t total = (size_t) (48000.0 * maxSec);
    std::vector<float> l (480), r (480), ol (480), orr (480);
    Rng g { 1 }; std::vector<double> win; double acc = 0; int cnt = 0; const int winLen = 4800;
    for (size_t pos = 0; pos < total; pos += 480)
    {
        for (int i = 0; i < 480; ++i) { float v = 0.f; if (pos + (size_t) i < 24000) v = (g.next() * 2 - 1) * 0.5f; l[(size_t) i] = r[(size_t) i] = v; }
        st.process (l.data(), r.data(), ol.data(), orr.data(), 480);
        for (int i = 0; i < 480; ++i)
        {
            acc += (double) ol[(size_t) i] * ol[(size_t) i] + (double) orr[(size_t) i] * orr[(size_t) i];
            if (++cnt == winLen) { win.push_back (std::sqrt (acc / (2.0 * winLen))); acc = 0; cnt = 0; }
        }
        const size_t k = win.size();
        if (k > 60 && pos > 96000)               // 3 s of quiet in a row: finished
        {
            bool quiet = true; for (size_t j = k - 30; j < k; ++j) if (win[j] > thr) quiet = false;
            if (quiet) break;
        }
    }
    long last = -1; for (size_t i = 0; i < win.size(); ++i) if (win[i] > thr) last = (long) i;
    return last < 0 ? 0.0 : std::min (maxSec, (double) (last + 1) * winLen / 48000.0);
}

static void testTail()
{
    std::printf ("\n== 6. tail: how long the reverb, echo and resonator keep ringing ==\n");
    constexpr double kThr = -80.0, kMax = 40.0;
    const double reported = 12.0;      // must equal kTailSeconds in src/Parameters.h (that file needs JUCE, so it is not included here)
    EngineParams b;
    // normal settings: the reported tail must cover them
    struct Case { const char* name; EngineParams p; };
    std::vector<Case> covered;
    { EngineParams p = b; p.ambienceLevel = 1.f; p.ambienceDecay = 0.5f; covered.push_back ({ "reverb Decay 0.5, Spacetime 0.5 (the defaults)", p }); }
    { EngineParams p = b; p.ambienceLevel = 1.f; p.ambienceDecay = 1.f; covered.push_back ({ "reverb Decay full, Spacetime 0.5", p }); }
    { EngineParams p = b; p.resonatorLevel = 1.f; p.resonatorFeedback = 0.8f; p.resonatorTune = 0.2f; covered.push_back ({ "resonator Feedback 0.8, Tune 0.2", p }); }
    { EngineParams p = b; p.echoLevel = 1.f; p.echoRepeats = 0.9f; p.echoDensity = 0.f; covered.push_back ({ "echo Repeats 0.9, shortest time", p }); }
    { EngineParams p = b; p.echoLevel = 1.f; p.echoRepeats = 0.5f; p.echoDensity = 0.5f; covered.push_back ({ "echo Repeats 0.5, middle time", p }); }
    double worst = 0;
    for (auto& c : covered)
    {
        const double t = tailSeconds (c.p, kThr, kMax);
        worst = std::max (worst, t);
        check (t < reported, fmt ("%-46s rings %5.1f s to -80 dB (reported tail %.0f s)", c.name, t, reported));
    }
    std::printf ("   (longest of these: %.1f s)\n", worst);
    // settings that ring much longer or for ever: only listed, not covered by any number
    const struct { const char* name; EngineParams p; } longer[] = {
        { "reverb Decay full, Spacetime 0",       [&] { EngineParams p = b; p.ambienceLevel = 1.f; p.ambienceDecay = 1.f; p.ambienceSpacetime = 0.f; return p; }() },
        { "reverb Decay full, Spacetime 1",       [&] { EngineParams p = b; p.ambienceLevel = 1.f; p.ambienceDecay = 1.f; p.ambienceSpacetime = 1.f; return p; }() },
        { "echo Repeats 0.9, time 0.5",           [&] { EngineParams p = b; p.echoLevel = 1.f; p.echoRepeats = 0.9f; p.echoDensity = 0.5f; return p; }() },
        { "echo Repeats 0.97, shortest time",     [&] { EngineParams p = b; p.echoLevel = 1.f; p.echoRepeats = 0.97f; p.echoDensity = 0.f; return p; }() },
        { "resonator Feedback 0.9, Tune 0.2",     [&] { EngineParams p = b; p.resonatorLevel = 1.f; p.resonatorFeedback = 0.9f; p.resonatorTune = 0.2f; return p; }() },
    };
    for (auto& c : longer)
        std::printf ("   not covered: %-40s rings %s s\n", c.name, tailSeconds (c.p, kThr, kMax) >= kMax ? ">= 40" : fmt ("%.1f", tailSeconds (c.p, kThr, kMax)).c_str());
}

int main (int argc, char** argv)
{
    flushDenormals();
    const std::string outDir = argc > 1 ? argv[1] : "";
    if (! outDir.empty()) { std::string cmd = "mkdir -p '" + outDir + "'"; if (std::system (cmd.c_str()) != 0) return 2; }
    testStream();
    testSections (outDir);
    testFuzz();
    testInstances();
    testTempo();
    testTail();
    std::printf ("\n%s\n", failures == 0 ? "RESULT - all engine checks passed" : "RESULT - SOME ENGINE CHECKS FAILED");
    return failures == 0 ? 0 : 1;
}
