// Wraps the 48 kHz / 32-sample Iroi engine so it can be used at any host sample rate and any host block size.
//  - host rate other than 48 kHz: a band-limited resampler goes in front of and behind the engine
//  - host blocks of any length: the audio is collected into blocks of 32 for the engine (fixed delay)
// The delay is constant and reported to the host (latencySamples), so the host can compensate for it.
#pragma once
#include <vector>
#include <cstdint>
#include <memory>
#include "IroxEngine.h"

namespace irox
{
// Streaming band-limited sample rate converter (windowed sinc, Kaiser window). Output sample j is the signal at
// input time j * inRate / outRate: there is no time offset, the only cost is that an output sample can only be
// produced once the input samples around it have arrived (halfWidth() input samples later).
class StreamResampler
{
public:
    StreamResampler (double inRate, double outRate);
    void reset();
    int halfWidth() const { return halfWidth_; }      // in input samples
    void push (const float* in, int n);
    int pull (float* out, int maxOut);                // returns how many were written
    bool isPassThrough() const { return passThrough_; }
private:
    bool passThrough_;
    double step_, cut_, pos_ = 0.0;
    int halfWidth_;
    std::int64_t base_ = 0, total_ = 0;               // absolute index of buf_[0], and of the next input sample
    std::vector<float> buf_;
    static const std::vector<float>& table();
};

class Stream
{
public:
    Stream();
    ~Stream();
    // Allocates everything. Call from the non-audio thread. maxBlock is the largest block the host will send.
    void prepare (double hostRate, int maxBlock);
    // Delay from input to output in host samples (what to report to the host).
    int latencySamples() const { return latency_; }
    void setParams (const EngineParams& p, float inputGainDb, float outputGainDb);
    // Logic's tempo and transport, once per host block (before process).
    void setTransport (const Transport& t) { if (engine_) engine_->setTransport (t); }
    // in / out: 2 channels each (a mono input is passed as the same pointer twice). In place is allowed.
    void process (const float* inL, const float* inR, float* outL, float* outR, int n);
    // Tests only: skip the engine (the audio then only goes through the resamplers and the block FIFO).
    void setTestBypass (bool b) { bypass_ = b; }
    // How many times the output ran short (should stay 0).
    int underruns() const { return underruns_; }
    // Peak and count of non-finite samples the engine produced (they are replaced by 0).
    int nonFiniteCount() const { return nonFinite_; }
    float modulationValue() const { return engine_ ? engine_->modulationValue() : 0.f; }
    double beatPhase() const { return engine_ ? engine_->beatPhase() : 0.0; }

private:
    struct Fifo
    {
        std::vector<float> d; size_t head = 0, count = 0;
        void init (size_t cap) { d.assign (cap, 0.f); head = 0; count = 0; }
        size_t capacity() const { return d.size(); }
        void push (float v) { if (count < d.size()) { d[(head + count) % d.size()] = v; ++count; } }
        float pop() { float v = d[head]; head = (head + 1) % d.size(); --count; return v; }
    };
    void processChunk (const float* inL, const float* inR, float* outL, float* outR, int n);

    double hostRate_ = 48000.0;
    int maxBlock_ = 0, latency_ = 0;
    int underruns_ = 0, nonFinite_ = 0;
    bool bypass_ = false;
    std::unique_ptr<Engine> engine_;
    std::unique_ptr<StreamResampler> inRs_[2], outRs_[2];
    Fifo acc_[2];       // engine-rate input waiting for a full block
    Fifo outF_[2];      // host-rate output
    std::vector<float> tmp_, blk_[2];
    EngineParams params_;
    float inGainTarget_ = 1.f, outGainTarget_ = 1.f, inGain_ = 1.f, outGain_ = 1.f;
};
}
