#pragma once

#include "../dsp/GraphPedalProcessor.h"
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

//==============================================================================
// Audio probe (task #67 — the AI agent's "ears").
//
// Runs a battery of test signals through a FRESH OFFLINE CLONE of a pedal's
// DSP graph (built from its serialized JSON, so the live audio thread is never
// touched) and returns an agent-readable report of how the pedal actually
// SOUNDS: level/gain, clipping + harmonic distortion (THD), dynamic response
// (compression), spectral tilt (bright/dark), noise floor, DC offset,
// latency/tail, and NaN/Inf sanity — plus a synthesized one-line diagnosis.
//
// Complements verify_pedal (which checks graph TOPOLOGY): verify says the wires
// are connected; probe says the audio coming out is sane and musical.
//==============================================================================
namespace pf::ai
{
namespace probe_detail
{
    constexpr double kSR    = 48000.0;
    constexpr int    kBlock = 512;
    constexpr int    kFFT   = 16384;          // ~0.34s analysis window
    constexpr int    kWarm  = 7200;           // 150ms warmup for time-based fx

    inline float dbfs (float lin) { return 20.0f * std::log10 (juce::jmax (lin, 1.0e-9f)); }

    struct Stats { float rms = 0.0f, peak = 0.0f, dc = 0.0f; bool nan = false; };

    inline Stats analyse (const std::vector<float>& x, int start, int len)
    {
        Stats s;
        const int end = juce::jmin ((int) x.size(), start + len);
        double sum = 0.0, sumsq = 0.0; int n = 0;
        for (int i = juce::jmax (0, start); i < end; ++i)
        {
            const float v = x[(size_t) i];
            if (std::isnan (v) || std::isinf (v)) { s.nan = true; continue; }
            sum += v; sumsq += (double) v * v;
            s.peak = juce::jmax (s.peak, std::abs (v));
            ++n;
        }
        if (n > 0) { s.dc = (float) (sum / n); s.rms = (float) std::sqrt (sumsq / n); }
        return s;
    }

    // Render a mono input through a fresh offline clone; return ch0 output.
    inline std::vector<float> render (const juce::String& name, const juce::String& json,
                                      const std::vector<float>& in)
    {
        GraphPedalProcessor proc (name, json);
        proc.prepareToPlay (kSR, kBlock);

        std::vector<float> out (in.size(), 0.0f);
        juce::AudioBuffer<float> buf (2, kBlock);
        juce::MidiBuffer midi;
        const int total = (int) in.size();

        for (int pos = 0; pos < total; pos += kBlock)
        {
            const int n = juce::jmin (kBlock, total - pos);
            buf.clear();
            for (int i = 0; i < n; ++i)
            {
                buf.setSample (0, i, in[(size_t) (pos + i)]);
                buf.setSample (1, i, in[(size_t) (pos + i)]);
            }
            juce::AudioBuffer<float> view (buf.getArrayOfWritePointers(), 2, n);
            midi.clear();
            proc.processBlock (view, midi);
            for (int i = 0; i < n; ++i) out[(size_t) (pos + i)] = buf.getSample (0, i);
        }
        return out;
    }

    inline std::vector<float> sine (double freq, float amp, int n)
    {
        std::vector<float> v ((size_t) n);
        const double w = 2.0 * juce::MathConstants<double>::pi * freq / kSR;
        for (int i = 0; i < n; ++i) v[(size_t) i] = amp * (float) std::sin (w * i);
        return v;
    }

    // Magnitude spectrum (kFFT/2 bins) of a Hann-windowed segment starting at `start`.
    inline std::vector<float> spectrum (const std::vector<float>& x, int start)
    {
        constexpr int order = 14;  // 2^14 == kFFT
        juce::dsp::FFT fft (order);
        std::vector<float> data ((size_t) (2 * kFFT), 0.0f);
        for (int i = 0; i < kFFT; ++i)
        {
            const int idx = start + i;
            const float s = (idx >= 0 && idx < (int) x.size()) ? x[(size_t) idx] : 0.0f;
            const float win = 0.5f * (1.0f - std::cos (2.0f * juce::MathConstants<float>::pi
                                                       * (float) i / (float) (kFFT - 1)));
            data[(size_t) i] = s * win;
        }
        fft.performFrequencyOnlyForwardTransform (data.data());
        data.resize ((size_t) (kFFT / 2));
        return data;
    }

    inline int binFor (double freq) { return (int) std::lround (freq * kFFT / kSR); }

    // Sum of magnitude^2 across [loHz, hiHz].
    inline double bandPower (const std::vector<float>& mag, double loHz, double hiHz)
    {
        const int lo = juce::jmax (1, binFor (loHz));
        const int hi = juce::jmin ((int) mag.size() - 1, binFor (hiHz));
        double p = 0.0;
        for (int b = lo; b <= hi; ++b) p += (double) mag[(size_t) b] * mag[(size_t) b];
        return p;
    }
}

//==============================================================================
inline juce::String probeAudio (const juce::String& pedalName, const juce::String& graphJSON)
{
    using namespace probe_detail;

    if (graphJSON.isEmpty())
        return "AUDIO PROBE: pedal has no DSP graph to test.";

    juce::String r;
    r << "AUDIO PROBE - \"" << pedalName << "\"  (" << (int) (kSR / 1000) << " kHz, stereo)\n";

    const double f0 = 1000.0;
    const int    total = kWarm + kFFT;

    // ---- Test 1: 1 kHz sine at -12 dBFS (nominal level) ----
    const float quietAmp = 0.25f;
    auto inQ  = sine (f0, quietAmp, total);
    auto outQ = render (pedalName, graphJSON, inQ);
    const Stats sIn  = analyse (inQ,  kWarm, kFFT);
    const Stats sOut = analyse (outQ, kWarm, kFFT);

    // ---- Test 2: 1 kHz sine at -3 dBFS (hot level, for dynamics) ----
    const float loudAmp = 0.707f;
    auto inL  = sine (f0, loudAmp, total);
    auto outL = render (pedalName, graphJSON, inL);
    const Stats sInL  = analyse (inL,  kWarm, kFFT);
    const Stats sOutL = analyse (outL, kWarm, kFFT);

    // ---- Test 3: white noise (-12 dBFS) for spectral tilt ----
    std::vector<float> inN ((size_t) total);
    juce::Random rng (20260529);
    for (int i = 0; i < total; ++i) inN[(size_t) i] = quietAmp * (rng.nextFloat() * 2.0f - 1.0f);
    auto outN = render (pedalName, graphJSON, inN);

    // ---- Test 4: silence for noise floor / self-oscillation ----
    std::vector<float> inZ ((size_t) total, 0.0f);
    auto outZ = render (pedalName, graphJSON, inZ);
    const Stats sZ = analyse (outZ, kWarm, kFFT);

    // ---- Test 5: impulse for latency + tail ----
    const int impAt = 64;
    std::vector<float> inI ((size_t) kFFT, 0.0f); inI[(size_t) impAt] = 1.0f;
    auto outI = render (pedalName, graphJSON, inI);

    //--------------------------------------------------------------------------
    const bool nan = sOut.nan || sOutL.nan || sZ.nan
                     || analyse (outN, kWarm, kFFT).nan || analyse (outI, 0, kFFT).nan;
    const bool silent = sOut.rms < 1.0e-4f;     // < -80 dBFS out for -12 dBFS in

    const float gainQ = dbfs (sOut.rms)  - dbfs (sIn.rms);
    const float gainL = dbfs (sOutL.rms) - dbfs (sInL.rms);
    const float compression = gainQ - gainL;    // >0 => gain reduced at hot input

    // THD from the quiet-sine spectrum.
    float thdPct = 0.0f;
    if (! silent && ! nan)
    {
        auto mag = spectrum (outQ, kWarm);
        const int fb = binFor (f0);
        double fund = 0.0;
        for (int b = juce::jmax (1, fb - 2); b <= fb + 2; ++b)
            if (b < (int) mag.size()) fund = juce::jmax (fund, (double) mag[(size_t) b]);
        double harm = 0.0;
        for (int k = 2; k <= 12; ++k)
        {
            const int hb = binFor (f0 * k);
            if (hb >= (int) mag.size()) break;
            double m = 0.0;
            for (int b = hb - 2; b <= hb + 2; ++b)
                if (b > 0 && b < (int) mag.size()) m = juce::jmax (m, (double) mag[(size_t) b]);
            harm += m * m;
        }
        if (fund > 1.0e-9) thdPct = (float) (100.0 * std::sqrt (harm) / fund);
    }

    // Spectral tilt from noise: out high/low vs in high/low.
    float tiltDb = 0.0f;
    if (! silent && ! nan)
    {
        auto mo = spectrum (outN, kWarm);
        auto mi = spectrum (inN,  kWarm);
        const double oL = bandPower (mo, 100.0, 500.0),  oH = bandPower (mo, 2000.0, 8000.0);
        const double iL = bandPower (mi, 100.0, 500.0),  iH = bandPower (mi, 2000.0, 8000.0);
        if (oL > 1e-12 && iL > 1e-12 && iH > 1e-12)
            tiltDb = 10.0f * (float) std::log10 ((oH / oL) / (iH / iL));
    }

    // Latency + tail from impulse.
    int latency = -1, tail = 0;
    {
        float pk = 0.0f;
        for (float v : outI) if (! (std::isnan (v) || std::isinf (v))) pk = juce::jmax (pk, std::abs (v));
        if (pk > 1.0e-5f)
        {
            const float on = 0.02f * pk, off = 0.001f * pk;   // -34dB on, -60dB tail end
            int first = -1, last = -1;
            for (int i = impAt; i < (int) outI.size(); ++i)
            {
                const float a = std::abs (outI[(size_t) i]);
                if (first < 0 && a > on) first = i;
                if (a > off) last = i;
            }
            if (first >= 0) { latency = first - impAt; tail = juce::jmax (0, last - first); }
        }
    }

    //--------------------------------------------------------------------------
    auto clipNote = [&] (const Stats& s) -> juce::String
    {
        if (s.peak >= 0.999f) return "hard clip at 0 dBFS";
        if (s.peak >= 0.95f)  return "near full-scale";
        return juce::String (dbfs (s.peak), 1) + " dBFS peak";
    };

    r << "SANITY:  " << (nan ? "FAIL - NaN/Inf in output" : "ok") << "\n";
    r << "LEVEL:   in " << juce::String (dbfs (sIn.rms), 1) << " -> out "
      << juce::String (dbfs (sOut.rms), 1) << " dBFS  (gain "
      << (gainQ >= 0 ? "+" : "") << juce::String (gainQ, 1) << " dB)\n";
    r << "PEAK:    " << clipNote (sOut) << "\n";
    r << "DISTORTION: THD " << juce::String (thdPct, 1) << "% @ 1kHz\n";
    r << "DYNAMICS: gain@-3dBFS " << (gainL >= 0 ? "+" : "") << juce::String (gainL, 1)
      << " dB  (" << (compression > 1.5f ? juce::String (compression, 1) + " dB compression/clip at hot input"
                                         : juce::String ("linear")) << ")\n";
    r << "TONE:    tilt " << (tiltDb >= 0 ? "+" : "") << juce::String (tiltDb, 1)
      << " dB high/low vs input ("
      << (tiltDb > 2.0f ? "brighter" : tiltDb < -2.0f ? "darker" : "neutral") << ")\n";
    r << "NOISE FLOOR: " << juce::String (dbfs (sZ.rms), 1) << " dBFS (silence in)\n";
    r << "DC OFFSET: " << juce::String (sOut.dc, 5) << "\n";
    r << "LATENCY: " << (latency < 0 ? juce::String ("n/a")
                                     : juce::String (latency) + " samples") << "\n";
    if (tail > kBlock)
        r << "TAIL:    ~" << juce::String (tail / kSR, 2) << " s (delay/reverb)\n";

    // ---- Synthesized diagnosis ----
    r << "\nDIAGNOSIS:\n";
    if (nan)
    {
        r << "  ! BROKEN: output contains NaN/Inf - the DSP is unstable "
             "(check feedback paths, divide-by-zero, uninitialised state).\n";
        return r;
    }
    if (silent)
    {
        r << "  ! SILENT: no audible output for a -12 dBFS input. The audio path is "
             "probably broken or a gain is at zero - run verify_pedal and check levels.\n";
        return r;
    }

    juce::StringArray bits;
    bits.add (gainQ > 1.0f  ? "+" + juce::String (gainQ, 0) + " dB boost"
            : gainQ < -1.0f ? juce::String (gainQ, 0) + " dB cut"
                            : "unity gain");
    bits.add (thdPct < 1.0f  ? "clean"
            : thdPct < 5.0f  ? "light drive"
            : thdPct < 20.0f ? "overdrive"
                             : "heavy distortion/fuzz");
    if (compression > 1.5f) bits.add ("compresses at hot levels");
    if (tiltDb > 2.0f)  bits.add ("bright");
    if (tiltDb < -2.0f) bits.add ("dark");
    if (tail > kBlock)  bits.add ("time-based tail");

    r << "  Working - " << bits.joinIntoString (", ") << ". Noise floor "
      << juce::String (dbfs (sZ.rms), 0) << " dBFS"
      << (dbfs (sZ.rms) > -50.0f ? " (NOISY - check for self-oscillation)." : " (quiet).") << "\n";
    return r;
}
}
