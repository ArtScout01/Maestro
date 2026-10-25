#pragma once
#include <juce_dsp/juce_dsp.h>
#include <cmath>

// DSP-Bausteine, benannt nach den Transistoren/Stufen des Maestro G-2 (Service Manual, Drawing No. 1).
// Zeitkonstanten/Frequenzen sind, wo moeglich, aus den Bauteilwerten des Schaltplans abgeleitet
// (siehe Kommentare). Mit "EST" markierte Werte sind Schaetzungen.
namespace g2
{
constexpr float kPi = 3.14159265358979f;

inline float coefFromMs (float ms, float fs) { return 1.0f - std::exp (-1.0f / (juce::jmax (0.01f, ms) * 0.001f * fs)); }

struct OnePole
{
    float a = 0.f, z = 0.f;
    void set (float fc, float fs) { a = 1.0f - std::exp (-2.0f * kPi * fc / fs); }
    float lp (float x) { z += a * (x - z); return z; }
    float hp (float x) { z += a * (x - z); return x - z; }
    void reset() { z = 0.f; }
};

struct Follower
{
    float aAtk = 0.f, aRel = 0.f, y = 0.f;
    void set (float atkMs, float relMs, float fs) { aAtk = coefFromMs (atkMs, fs); aRel = coefFromMs (relMs, fs); }
    float process (float x) { const float r = std::abs (x); y += (r > y ? aAtk : aRel) * (r - y); return y; }
    void reset() { y = 0.f; }
};

// TPT-State-Variable-Filter (Zavalishin/Cytomic)
struct Svf
{
    float g = 0.f, k = 1.f, a1 = 0.f, a2 = 0.f, a3 = 0.f, ic1 = 0.f, ic2 = 0.f;
    void set (float fc, float Q, float fs)
    {
        g = std::tan (kPi * juce::jmin (fc, 0.45f * fs) / fs);
        k = 1.0f / Q;
        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }
    // liefert Bandpass (Spitzenverstaerkung 1/k)
    float bp (float v0)
    {
        const float v3 = v0 - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1;
        ic2 = 2.f * v2 - ic2;
        return v1;
    }
    void reset() { ic1 = ic2 = 0.f; }
};

// Q3 Pick-Detector-Preamp + Q4 Pick Detector + Q5/Q6 One-Shot + Q7 Pulse Former.
// Erkennt den Anschlag (schneller Pegelanstieg), liefert einen Puls fester Laenge.
// One-Shot-Zeit: ~0.69 * 330k * 0.47u = ~107 ms (Schaltplan: 330K / .47)
struct PickDetector
{
    Follower fast, slow;
    int hold = 0, holdLen = 0;

    void prepare (float fs)
    {
        fast.set (0.5f, 25.f, fs);
        slow.set (30.f, 250.f, fs);
        holdLen = (int) (0.107f * fs);
        hold = 0;
        fast.reset(); slow.reset();
    }
    // true im Moment eines neuen Anschlags; thr = Empfindlichkeitsschwelle (VR1)
    bool process (float x, float thr)
    {
        const float f = fast.process (x), s = slow.process (x);
        if (hold > 0) { --hold; return false; }
        if (f > thr && f > 2.0f * s) { hold = holdLen; return true; }
        return false;
    }
    bool pulse() const { return hold > 0; }
};

// Q18-Q20 Fuzz Preamps #1-#3: drei Verstaerkerstufen mit Clipping, "Spitzen abgeschnitten".
// Koppelglieder aus dem Schaltplan: .033u/10K (~480 Hz), .0033u/22K+ (~1.2 kHz EST) -> duenner Maestro-Fuzz.
struct Fuzz
{
    OnePole hp2, hp3, lp;
    void prepare (float fsOS)
    {
        hp2.set (480.f, fsOS);
        hp3.set (1200.f, fsOS);
        lp.set (9000.f, fsOS);
        hp2.reset(); hp3.reset(); lp.reset();
    }
    float process (float x)
    {
        float y = std::tanh (30.f * x);                                    // Q18 (EST Gain)
        constexpr float bias = 0.25f;                                      // Q19 asymmetrisch (EST)
        y = std::tanh (12.f * y + bias) - std::tanh (bias);
        y = hp2.hp (y);                                                    // .033u -> Q20
        y = juce::jlimit (-1.f, 0.85f, 9.f * y);                           // Q20 hartes Clipping
        y = hp3.hp (y);                                                    // .0033u Ausgangskopplung
        return 0.30f * lp.lp (y);
    }
};

// Q23/Q24 Echo-Repeat-Multivibrator + Q21/Q22: zerhackt das Signal (Tremolo/Repeat),
// wird bei jedem Anschlag neu gestartet (Diode D11).
struct EchoGate
{
    float phase = 1.f, g = 1.f, a = 0.f;
    void prepare (float fs) { a = coefFromMs (1.5f, fs); phase = 1.f; g = 1.f; }
    float process (bool trig, float hz, float duty, float fs)
    {
        if (trig) phase = 0.f;
        phase += hz / fs;
        if (phase >= 1.f) phase -= 1.f;
        g += a * ((phase < duty ? 1.f : 0.f) - g);
        return g;
    }
};

// Q25-Q28 Wow-Wow: Lampe P1 (GE 1450) + Photozelle verstimmen den Q27-Schwingkreis (L3 = 500 mH)
// im Bereich ~300...1700 Hz. Lampe heizt schnell auf, kuehlt langsam ab -> "wow".
struct WowWow
{
    float lamp = 0.f, atk = 0.f;
    int hold = 0;
    Svf f;
    int ctr = 0;
    static constexpr float kQ = 3.5f;   // EST
    void prepare (float fs) { atk = coefFromMs (15.f, fs); lamp = 0.f; hold = 0; f.reset(); }
    static float freqFor (float b) { return 300.f * std::pow (1700.f / 300.f, b); }
    // speed 0..1 (VR2): hoeher = kuerzer/schneller
    float process (float x, bool trig, float speed, float fs)
    {
        if (trig) hold = (int) (juce::jmap (speed, 0.25f, 0.02f) * fs);
        const float target = hold > 0 ? 1.f : 0.f;
        if (hold > 0) --hold;
        const float rel = coefFromMs (juce::jmap (speed, 450.f, 60.f), fs);
        lamp += (target > lamp ? atk : rel) * (target - lamp);
        if ((ctr++ & 7) == 0) f.set (freqFor (lamp), kQ, fs);
        return f.bp (x) * (1.f / kQ) * 2.0f;
    }
};

// Q8-Q13 String Bass: RC-Tiefpass (3.3K/.18u, 2x ~268 Hz) -> Squaring (Schmitt) -> Divider (Flip-Flop, f/2)
// -> Diodengatter (D4-D6), geoeffnet vom Pulse Former Q7.
struct StringBass
{
    OnePole in1, in2, out1, out2;
    Follower env;
    bool high = false, div = false;
    float gate = 0.f, gAtk = 0.f, gRel = 0.f;
    void prepare (float fs)
    {
        in1.set (268.f, fs); in2.set (268.f, fs);
        out1.set (220.f, fs); out2.set (220.f, fs);   // EST
        env.set (2.f, 60.f, fs);
        gAtk = coefFromMs (3.f, fs);
        gRel = coefFromMs (70.f, fs);                  // ~ 23.5k * 3u (D4-Netzwerk)
        in1.reset(); in2.reset(); out1.reset(); out2.reset(); env.reset();
        high = div = false; gate = 0.f;
    }
    float process (float x, bool pulse)
    {
        const float s = in2.lp (in1.lp (x));
        const float hyst = 0.003f + 0.12f * env.process (s);
        if (! high && s > hyst) { high = true; div = ! div; }
        else if (high && s < -hyst) high = false;
        gate += (pulse ? gAtk : gRel) * ((pulse ? 1.f : 0.f) - gate);
        return out2.lp (out1.lp ((div ? 1.f : -1.f) * gate));
    }
};

// Percussion: alle Stimmen werden vom Pulse Former Q7 ausgeloest.
struct Percussion
{
    // Bongo Q17: abklingender Oszillator, Laenge via VR3
    float bAmp = 0.f, bPitch = 0.f, bPh = 0.f, bDec = 0.f, bPDec = 0.f;
    // Clave L2 (100 mH) mit .0068u+.027u -> f = 1/(2*pi*sqrt(L*C)) = ~2.74 kHz
    float cAmp = 0.f, cPh = 0.f, cDec = 0.f;
    // Brush: Rauschen (Q14 Zener) -> Noise Amp Q15 -> Gate Q16 mit L1 (27 mH) + .022u -> ~6.5 kHz
    Svf brushF;
    float bruEnv = 0.f, bruA = 0.f, bruR = 0.f;
    juce::Random rnd;
    float fs = 44100.f;

    void prepare (float sr)
    {
        fs = sr;
        brushF.set (6500.f, 1.2f, fs);
        bruA = coefFromMs (6.f, fs);
        bruR = coefFromMs (40.f, fs);
        bPDec = std::exp (-1.f / (0.025f * fs));
        cDec = std::exp (-6.9f / (0.040f * fs));
        bAmp = cAmp = bruEnv = 0.f;
        brushF.reset();
    }
    void setBongoLen (float adj01) { bDec = std::exp (-6.9f / (juce::jmap (adj01, 0.06f, 0.5f) * fs)); }
    void trigger (bool bongo, bool clave) { if (bongo) { bAmp = 1.f; bPitch = 1.f; } if (clave) cAmp = 1.f; }

    float process (bool bongoOn, bool claveOn, bool brushOn, bool pulse)
    {
        float out = 0.f;
        if (bongoOn)
        {
            bPh += (270.f * (1.f + 0.35f * bPitch)) / fs; if (bPh >= 1.f) bPh -= 1.f;
            out += 0.9f * bAmp * std::sin (2.f * kPi * bPh);
        }
        bAmp *= bDec; bPitch *= bPDec;
        if (claveOn)
        {
            cPh += 2740.f / fs; if (cPh >= 1.f) cPh -= 1.f;
            out += 0.5f * cAmp * std::sin (2.f * kPi * cPh);
        }
        cAmp *= cDec;
        bruEnv += (pulse ? bruA : bruR) * ((pulse ? 1.f : 0.f) - bruEnv);
        if (brushOn)
            out += 0.9f * bruEnv * brushF.bp (rnd.nextFloat() * 2.f - 1.f) * (1.f / 1.2f);
        return out;
    }
};
} // namespace g2
