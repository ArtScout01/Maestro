#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "G2Dsp.h"

class G2Processor : public juce::AudioProcessor
{
public:
    G2Processor();

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Maestro G-2"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static const juce::StringArray tabIds;   // 11 Tabschalter
    juce::AudioProcessorValueTreeState apvts;

private:
    float p (const char* id) const { return apvts.getRawParameterValue (id)->load(); }
    bool tab (const char* id) const { return p (id) > 0.5f; }

    float fs = 44100.f;
    juce::AudioBuffer<float> monoBuf, fuzzBuf;
    juce::dsp::Oversampling<float> oversampling { 1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, false };

    g2::PickDetector pick;
    g2::Fuzz fuzz;
    g2::EchoGate echo;
    g2::WowWow wow;
    g2::Svf color1, color2;
    g2::StringBass bass;
    g2::Percussion perc;
    g2::OnePole toneLp;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (G2Processor)
};
