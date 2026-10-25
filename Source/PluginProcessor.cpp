#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace { constexpr int kChunk = 4096; }

const juce::StringArray G2Processor::tabIds { "natural", "fuzz", "color1", "color2", "wow", "echo",
                                              "bongo", "brush", "clave", "tambo", "bass" };

juce::AudioProcessorValueTreeState::ParameterLayout G2Processor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    const char* names[] = { "Natural Amp", "Fuzz Tone", "Color Tones 1", "Color Tones 2", "Wow Wow", "Echo Repeat",
                            "Bongo", "Brush", "Clave", "Tambourine", "String Bass" };
    for (int i = 0; i < tabIds.size(); ++i)
        l.add (std::make_unique<AudioParameterBool> (ParameterID { tabIds[i], 1 }, names[i], false));

    auto knob = [&l] (const char* id, const char* name, float def)
    {
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.f, 1.f), def));
    };
    knob ("sens",       "Sensitivity (VR1)",      0.6f);
    knob ("wowSpeed",   "Wow Wow Speed (VR2)",    0.5f);
    knob ("bongoAdj",   "Bongo Adj (VR3)",        0.5f);
    knob ("echoSpeed",  "Echo Repeat Speed (VR4)", 0.5f);
    knob ("echoWidth",  "Percussion Adj (VR5)",   0.5f);
    knob ("bassVol",    "Bass Volume (VR6)",      0.6f);
    knob ("percVol",    "Percussion Volume (VR7)", 0.6f);
    knob ("trebleVol",  "Treble Volume (VR8)",    0.7f);
    knob ("trebleTone", "Treble Tone (VR9)",      0.7f);
    return l;
}

G2Processor::G2Processor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "G2", createLayout())
{
}

bool G2Processor::isBusesLayoutSupported (const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo())
           && l.getMainInputChannelSet() == o;
}

void G2Processor::prepareToPlay (double sr, int samplesPerBlock)
{
    fs = (float) sr;
    oversampling.initProcessing ((size_t) kChunk);
    oversampling.reset();
    fuzz.prepare (fs * (float) oversampling.getOversamplingFactor());
    pick.prepare (fs);
    echo.prepare (fs);
    wow.prepare (fs);
    color1.reset(); color2.reset();
    bass.prepare (fs);
    perc.prepare (fs);
    toneLp.reset();
    const int sz = juce::jmax (samplesPerBlock, kChunk);
    monoBuf.setSize (1, sz);
    fuzzBuf.setSize (1, sz);
}

void G2Processor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int nIn = getTotalNumInputChannels(), nOut = getTotalNumOutputChannels();
    if (nIn == 0) { buffer.clear(); return; }

    if (monoBuf.getNumSamples() < n) { monoBuf.setSize (1, n); fuzzBuf.setSize (1, n); }
    float* m = monoBuf.getWritePointer (0);
    float* fz = fuzzBuf.getWritePointer (0);

    // Eingang: Mono (Gitarre, Input Jack)
    juce::FloatVectorOperations::copy (m, buffer.getReadPointer (0), n);
    if (nIn > 1)
    {
        juce::FloatVectorOperations::add (m, buffer.getReadPointer (1), n);
        juce::FloatVectorOperations::multiply (m, 0.5f, n);
    }

    const bool fuzzOn = tab ("fuzz"), natOn = tab ("natural"), echoOn = tab ("echo"), wowOn = tab ("wow"),
               c1 = tab ("color1"), c2 = tab ("color2"), bongo = tab ("bongo"), brush = tab ("brush"),
               clave = tab ("clave"), tambo = tab ("tambo"), bassOn = tab ("bass");
    const bool modActive = echoOn || wowOn || c1 || c2;

    // Fuzz Preamps #1-#3 (Q18-Q20), 4x oversampled
    if (fuzzOn)
    {
        juce::FloatVectorOperations::copy (fz, m, n);
        for (int s = 0; s < n; s += kChunk)
        {
            const int len = juce::jmin (kChunk, n - s);
            float* chans[1] = { fz + s };
            juce::dsp::AudioBlock<float> blk (chans, 1, (size_t) len);
            auto up = oversampling.processSamplesUp (blk);
            float* d = up.getChannelPointer (0);
            for (size_t i = 0; i < up.getNumSamples(); ++i) d[i] = fuzz.process (d[i]);
            oversampling.processSamplesDown (blk);
        }
    }

    const float thr      = 0.3f * std::pow (0.02f, p ("sens"));              // VR1
    const float wowSpd   = p ("wowSpeed");                                    // VR2
    const float echoHz   = 1.5f * std::pow (14.f / 1.5f, p ("echoSpeed"));   // VR4 (EST: 1.5-14 Hz)
    const float duty     = juce::jmap (p ("echoWidth"), 0.2f, 0.7f);          // VR5
    const float bassGain = 0.7f * p ("bassVol");                              // VR6
    const float percGain = 0.6f * p ("percVol");                              // VR7
    const float trebGain = 1.43f * p ("trebleVol");                           // VR8
    toneLp.set (700.f * std::pow (17.f, p ("trebleTone")), fs);               // VR9 (EST)
    perc.setBongoLen (p ("bongoAdj"));                                        // VR3
    color1.set (g2::WowWow::freqFor (0.2f), g2::WowWow::kQ, fs);              // EST: feste Abstimmung
    color2.set (g2::WowWow::freqFor (0.7f), g2::WowWow::kQ, fs);              // EST
    constexpr float cScale = 2.0f / g2::WowWow::kQ;

    for (int i = 0; i < n; ++i)
    {
        const float x = m[i];
        const bool trig = pick.process (x, thr);
        const bool pulse = pick.pulse();
        if (trig) perc.trigger (bongo, clave || tambo);

        // Signalweg Gitarre: Q1/Q2 (Natural) oder Fuzz -> Q22/Echo -> Q27 (Wow/Color) -> Treble -> Q29
        float src = fuzzOn ? fz[i] : std::tanh (2.f * x) * 0.5f;
        if (modActive && ! (natOn || fuzzOn)) src = 0.f;
        if (echoOn) src *= echo.process (trig, echoHz, duty, fs);

        float y = src;
        if (wowOn || c1 || c2)
        {
            float acc = 0.f;
            if (wowOn) acc += wow.process (src, trig, wowSpd, fs);
            if (c1)    acc += color1.bp (src) * cScale;
            if (c2)    acc += color2.bp (src) * cScale;
            y = acc;
        }
        y = toneLp.lp (y) * trebGain;

        const float b  = bassOn ? bass.process (x, pulse) : 0.f;
        const float pc = perc.process (bongo, clave || tambo, brush || tambo, pulse);

        m[i] = std::tanh (y + b * bassGain + pc * percGain);   // Output Preamp Q29/Q30
    }

    for (int ch = 0; ch < nOut; ++ch)
        juce::FloatVectorOperations::copy (buffer.getWritePointer (ch), m, n);
}

juce::AudioProcessorEditor* G2Processor::createEditor() { return new G2Editor (*this); }

void G2Processor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml()) copyXmlToBinary (*xml, dest);
}

void G2Processor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType())) apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new G2Processor(); }
