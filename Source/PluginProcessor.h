#pragma once
#include <JuceHeader.h>
#include <array>
#include <atomic>
#include "Dsp.h"
#include "Genres.h"

class MasterProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numBands   = 7;
    static constexpr int anchorBand = 3;   // 500-2k: the band the others are balanced against

    MasterProcessor();
    ~MasterProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Seven Band Master"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- Read by the editor (written on the audio thread) ----
    std::array<std::atomic<float>, numBands> measuredRelDb;   // band level relative to anchor band
    std::array<std::atomic<float>, numBands> appliedGainDb;   // tonal-balance correction
    std::array<std::atomic<float>, numBands> compGrDb;        // current compressor gain reduction

    static const std::array<const char*, numBands> bandNames;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void updateAnalysis (const std::array<double, numBands>& acc, int numSamples, int numChannels,
                         const GenrePreset& genre);

    double sr = 44100.0;

    std::array<dsp7::ChannelSplitter, 2> splitter;

    // Analysis of the (pre-correction) band signals
    std::array<double, numBands> bandPower {};
    bool primed = false;

    // Per-band state used inside the sample loop
    std::array<float, numBands> balanceDb {};    // smoothed tonal-balance gain (dB)
    std::array<float, numBands> balanceLin {};   // same, linear
    std::array<float, numBands> thrDb {};        // adaptive compressor threshold (dB)
    std::array<float, numBands> grDb {};         // smoothed gain reduction (dB)
    std::array<float, numBands> detMs {};        // detector mean-square

    float prevOutGain = -1.0f;

    std::atomic<float>* pGenre    = nullptr;
    std::atomic<float>* pBalance  = nullptr;
    std::atomic<float>* pRange    = nullptr;
    std::atomic<float>* pMaxBoost = nullptr;
    std::atomic<float>* pDynamics = nullptr;
    std::atomic<float>* pSpeed    = nullptr;
    std::atomic<float>* pOutput   = nullptr;
    std::atomic<float>* pBypass   = nullptr;
    std::array<std::atomic<float>*, numBands> pBandBal {};   // per-band tonal-balance amount
    std::array<std::atomic<float>*, numBands> pBandDyn {};   // per-band dynamics amount

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterProcessor)
};
