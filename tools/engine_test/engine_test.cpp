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

int main (int argc, char** argv)
{
    const std::string outDir = argc > 1 ? argv[1] : "";
    if (! outDir.empty()) { std::string cmd = "mkdir -p '" + outDir + "'"; if (std::system (cmd.c_str()) != 0) return 2; }
    testStream();
    testSections (outDir);
    testFuzz();
    testInstances();
    std::printf ("\n%s\n", failures == 0 ? "RESULT - all engine checks passed" : "RESULT - SOME ENGINE CHECKS FAILED");
    return failures == 0 ? 0 : 1;
}
