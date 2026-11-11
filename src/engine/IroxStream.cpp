#include "IroxStream.h"
#include <cmath>
#include <algorithm>

namespace irox
{
namespace
{
constexpr int kZeroCrossings = 16;       // kernel half width in zero crossings
constexpr int kTableRes = 512;           // table entries per zero crossing
constexpr double kCutoff = 0.95;         // of the lower Nyquist frequency
constexpr double kPi = 3.14159265358979323846;

double besselI0 (double x)
{
    double sum = 1.0, term = 1.0, q = x * x * 0.25;
    for (int k = 1; k < 60; ++k) { term *= q / ((double) k * (double) k); sum += term; if (term < 1e-14 * sum) break; }
    return sum;
}
}

const std::vector<float>& StreamResampler::table()
{
    static const std::vector<float> t = []
    {
        // windowed sinc h(u), u from 0 to kZeroCrossings, kTableRes entries per unit (+2 spare for interpolation)
        std::vector<float> v ((size_t) (kZeroCrossings * kTableRes + 2), 0.f);
        const double beta = 9.0, i0b = besselI0 (beta);
        for (size_t i = 0; i < v.size(); ++i)
        {
            const double u = (double) i / kTableRes;
            const double s = u < 1e-12 ? 1.0 : std::sin (kPi * u) / (kPi * u);
            const double r = u / kZeroCrossings;
            const double w = r >= 1.0 ? 0.0 : besselI0 (beta * std::sqrt (1.0 - r * r)) / i0b;
            v[i] = (float) (s * w);
        }
        return v;
    }();
    return t;
}

StreamResampler::StreamResampler (double inRate, double outRate)
{
    passThrough_ = std::abs (inRate - outRate) < 0.5;
    step_ = inRate / outRate;
    cut_ = std::min (1.0, outRate / inRate) * kCutoff;
    halfWidth_ = passThrough_ ? 0 : (int) std::ceil (kZeroCrossings / cut_);
    table();
    reset();
}

void StreamResampler::reset()
{
    pos_ = 0.0; base_ = 0; total_ = 0;
    buf_.clear();
    buf_.reserve (1 << 15);
}

void StreamResampler::push (const float* in, int n)
{
    buf_.insert (buf_.end(), in, in + n);
    total_ += n;
}

int StreamResampler::pull (float* out, int maxOut)
{
    const std::vector<float>& tab = table();
    int produced = 0;
    while (produced < maxOut)
    {
        const std::int64_t i0 = (std::int64_t) std::floor (pos_);
        if (passThrough_)
        {
            if (i0 >= total_) break;
            out[produced++] = buf_[(size_t) (i0 - base_)];
            pos_ += 1.0;
        }
        else
        {
            if (i0 + halfWidth_ > total_ - 1) break;           // needs input that has not arrived yet
            double sum = 0.0, wsum = 0.0;
            for (std::int64_t n = i0 - halfWidth_ + 1; n <= i0 + halfWidth_; ++n)
            {
                const double u = std::abs ((double) n - pos_) * cut_;
                if (u >= kZeroCrossings) continue;
                const double f = u * kTableRes;
                const size_t k = (size_t) f;
                const double w = (double) tab[k] + ((double) tab[k + 1] - (double) tab[k]) * (f - (double) k);
                wsum += w;
                if (n >= base_ && n < total_) sum += w * (double) buf_[(size_t) (n - base_)];
            }
            out[produced++] = wsum != 0.0 ? (float) (sum / wsum) : 0.f;
            pos_ += step_;
        }
        // drop samples no later output can need
        const std::int64_t keepFrom = (std::int64_t) std::floor (pos_) - halfWidth_;
        if (keepFrom - base_ > 4096)
        {
            buf_.erase (buf_.begin(), buf_.begin() + (keepFrom - base_));
            base_ = keepFrom;
        }
    }
    return produced;
}

// ---------------------------------------------------------------------------------------------------------------

Stream::Stream() {}
Stream::~Stream() = default;

void Stream::prepare (double hostRate, int maxBlock)
{
    hostRate_ = hostRate;
    maxBlock_ = std::max (maxBlock, 32);
    engine_.reset (new Engine);
    engine_->setParams (params_);

    const bool direct = std::abs (hostRate - kEngineSampleRate) < 0.5;
    const double ratio = hostRate / kEngineSampleRate;          // host samples per engine sample
    int hostWaitIn = 0, engWaitOut = 0;
    for (int c = 0; c < 2; ++c)
    {
        inRs_[c].reset (new StreamResampler (hostRate, kEngineSampleRate));
        outRs_[c].reset (new StreamResampler (kEngineSampleRate, hostRate));
    }
    if (! direct)
    {
        hostWaitIn = inRs_[0]->halfWidth();                    // host samples
        engWaitOut = outRs_[0]->halfWidth();                   // engine samples
    }
    // Worst case wait until the output for a given input sample exists:
    //  input resampler + up to 31 engine samples for a full block + output resampler, plus a little spare.
    const double waitHost = hostWaitIn + (kEngineBlockSize - 1 + engWaitOut) * ratio;
    latency_ = (int) std::ceil (waitHost) + 2;
    if (direct) latency_ = kEngineBlockSize;                   // 32 samples exactly

    const size_t cap = (size_t) (maxBlock_ * 2 + latency_ + 8192);
    for (int c = 0; c < 2; ++c)
    {
        acc_[c].init ((size_t) (kEngineBlockSize * 2 + maxBlock_ * 2 + 4096));
        outF_[c].init (cap);
        for (int i = 0; i < latency_; ++i) outF_[c].push (0.f);   // the fixed delay
        blk_[c].assign ((size_t) kEngineBlockSize, 0.f);
    }
    tmp_.assign ((size_t) (maxBlock_ * 2 + 4096), 0.f);
    underruns_ = nonFinite_ = 0;
    inGain_ = inGainTarget_; outGain_ = outGainTarget_;
}

void Stream::setParams (const EngineParams& p, float inputGainDb, float outputGainDb)
{
    params_ = p;
    if (engine_) engine_->setParams (p);
    inGainTarget_ = std::pow (10.f, inputGainDb / 20.f);
    outGainTarget_ = std::pow (10.f, outputGainDb / 20.f);
}

void Stream::process (const float* inL, const float* inR, float* outL, float* outR, int n)
{
    if (! engine_) { for (int i = 0; i < n; ++i) { outL[i] = inL[i]; outR[i] = inR[i]; } return; }
    int done = 0;
    while (done < n)
    {
        const int c = std::min (n - done, maxBlock_);
        processChunk (inL + done, inR + done, outL + done, outR + done, c);
        done += c;
    }
}

void Stream::processChunk (const float* inL, const float* inR, float* outL, float* outR, int n)
{
    const float* in[2] = { inL, inR };
    float* out[2] = { outL, outR };
    const bool direct = inRs_[0]->isPassThrough();

    // Read the inputs first (the output buffers may be the same memory), with a smoothed input gain.
    const float gainStep = 0.002f;
    float g = inGain_;
    std::vector<float>& t = tmp_;
    for (int c = 0; c < 2; ++c)
    {
        g = inGain_;
        for (int i = 0; i < n; ++i)
        {
            g += gainStep * (inGainTarget_ - g);
            t[(size_t) i] = in[c][i] * g;
        }
        if (direct)
            for (int i = 0; i < n; ++i) acc_[c].push (t[(size_t) i]);
        else
        {
            inRs_[c]->push (t.data(), n);
            const size_t room = acc_[c].capacity() - acc_[c].count;
            const int got = inRs_[c]->pull (t.data() + n, (int) std::min<size_t> (room, t.size() - (size_t) n));
            for (int i = 0; i < got; ++i) acc_[c].push (t[(size_t) (n + i)]);
        }
    }
    inGain_ = g;

    // Full blocks of 32 go through the engine.
    while (acc_[0].count >= (size_t) kEngineBlockSize && acc_[1].count >= (size_t) kEngineBlockSize)
    {
        for (int i = 0; i < kEngineBlockSize; ++i) { blk_[0][(size_t) i] = acc_[0].pop(); blk_[1][(size_t) i] = acc_[1].pop(); }
        if (! bypass_) engine_->processBlock (blk_[0].data(), blk_[1].data());
        for (int c = 0; c < 2; ++c)
        {
            for (int i = 0; i < kEngineBlockSize; ++i)
            {
                float v = blk_[c][(size_t) i];
                if (! std::isfinite (v)) { v = 0.f; ++nonFinite_; }
                blk_[c][(size_t) i] = v;
            }
            if (direct)
                for (int i = 0; i < kEngineBlockSize; ++i) outF_[c].push (blk_[c][(size_t) i]);
            else
            {
                outRs_[c]->push (blk_[c].data(), kEngineBlockSize);
                const size_t room = outF_[c].capacity() - outF_[c].count;
                const int got = outRs_[c]->pull (t.data(), (int) std::min<size_t> (room, t.size()));
                for (int i = 0; i < got; ++i) outF_[c].push (t[(size_t) i]);
            }
        }
    }

    // Hand out n samples, with a smoothed output gain.
    float og = outGain_;
    for (int c = 0; c < 2; ++c)
    {
        og = outGain_;
        for (int i = 0; i < n; ++i)
        {
            og += gainStep * (outGainTarget_ - og);
            float v = 0.f;
            if (outF_[c].count > 0) v = outF_[c].pop(); else ++underruns_;
            out[c][i] = v * og;
        }
    }
    outGain_ = og;
}
}
