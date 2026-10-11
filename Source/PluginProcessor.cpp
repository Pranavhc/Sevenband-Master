#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    // Crossovers between: Sub | Bass | Low-Mid | Mid | Up-Mid | Presence | Air
    constexpr std::array<double, 6> crossoverHz { 60.0, 250.0, 500.0, 2000.0, 4000.0, 8000.0 };

    // Base compressor times per band (scaled by the genre)
    constexpr std::array<float, 7> baseAttackMs  { 30.0f, 24.0f, 18.0f, 12.0f,  8.0f,  5.0f,  3.0f };
    constexpr std::array<float, 7> baseReleaseMs { 300.0f, 220.0f, 170.0f, 130.0f, 100.0f, 80.0f, 60.0f };

    constexpr float kneeDb      = 6.0f;
    constexpr float maxReduceDb = 6.0f;

    // Default per-band tonal-balance amounts (%). Upper bands are deliberately conservative.
    // (index 3 = Mid anchor, not used for balance)
    constexpr std::array<float, 7> defaultBalance { 40.0f, 40.0f, 40.0f, 0.0f, 25.0f, 15.0f, 10.0f };
}

const std::array<const char*, MasterProcessor::numBands> MasterProcessor::bandNames {
    "Sub", "Bass", "Low-Mid", "Mid", "Up-Mid", "Presence", "Air" };

//==============================================================================
MasterProcessor::MasterProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (auto& a : measuredRelDb) a = 0.0f;
    for (auto& a : appliedGainDb) a = 0.0f;
    for (auto& a : compGrDb)      a = 0.0f;

    pGenre    = apvts.getRawParameterValue ("genre");
    pBalance  = apvts.getRawParameterValue ("balance");
    pRange    = apvts.getRawParameterValue ("range");
    pMaxBoost = apvts.getRawParameterValue ("maxboost");
    pDynamics = apvts.getRawParameterValue ("dynamics");
    pSpeed    = apvts.getRawParameterValue ("speed");
    pOutput   = apvts.getRawParameterValue ("output");
    pBypass   = apvts.getRawParameterValue ("bypass");

    for (size_t b = 0; b < (size_t) numBands; ++b)
    {
        pBandBal[b] = apvts.getRawParameterValue ("bal" + juce::String ((int) b));
        pBandDyn[b] = apvts.getRawParameterValue ("dyn" + juce::String ((int) b));
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout MasterProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;

    StringArray genreNames;
    for (auto& g : kGenres) genreNames.add (g.name);

    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "genre", 1 }, "Genre", genreNames, 0));

    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "balance", 1 }, "Balance (All Bands)",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "range", 1 }, "Max Cut",
            NormalisableRange<float> (0.0f, 6.0f, 0.1f), 3.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "maxboost", 1 }, "Max Boost",
            NormalisableRange<float> (0.0f, 4.0f, 0.1f), 1.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "dynamics", 1 }, "Dynamics (All Bands)",
            NormalisableRange<float> (0.0f, 100.0f, 0.1f), 100.0f, AudioParameterFloatAttributes().withLabel ("%")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "speed", 1 }, "Analysis Time",
            NormalisableRange<float> (0.5f, 15.0f, 0.1f, 0.5f), 4.0f, AudioParameterFloatAttributes().withLabel ("s")));
    l.add (std::make_unique<AudioParameterFloat> (ParameterID { "output", 1 }, "Output",
            NormalisableRange<float> (-12.0f, 12.0f, 0.1f), 0.0f, AudioParameterFloatAttributes().withLabel ("dB")));
    l.add (std::make_unique<AudioParameterBool>  (ParameterID { "bypass", 1 }, "Bypass", false));

    // Per-band controls
    for (int b = 0; b < numBands; ++b)
    {
        const String nm (bandNames[(size_t) b]);
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { "bal" + String (b), 1 }, nm + " Balance",
                NormalisableRange<float> (0.0f, 100.0f, 0.1f), defaultBalance[(size_t) b], AudioParameterFloatAttributes().withLabel ("%")));
        l.add (std::make_unique<AudioParameterFloat> (ParameterID { "dyn" + String (b), 1 }, nm + " Dynamics",
                NormalisableRange<float> (0.0f, 100.0f, 0.1f), 60.0f, AudioParameterFloatAttributes().withLabel ("%")));
    }
    return l;
}

bool MasterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet()  == juce::AudioChannelSet::stereo();
}

//==============================================================================
void MasterProcessor::prepareToPlay (double sampleRate, int)
{
    sr = sampleRate;

    for (auto& s : splitter) s.prepare (sr, crossoverHz);

    bandPower.fill (0.0);
    balanceDb.fill (0.0f);
    balanceLin.fill (1.0f);
    thrDb.fill (0.0f);
    grDb.fill (0.0f);
    detMs.fill (0.0f);
    primed = false;
    prevOutGain = -1.0f;
}

//==============================================================================
void MasterProcessor::updateAnalysis (const std::array<double, numBands>& acc, int n, int nch,
                                      const GenrePreset& genre)
{
    std::array<double, numBands> blockPower;
    for (size_t b = 0; b < (size_t) numBands; ++b)
        blockPower[b] = acc[b] / (double) (n * nch);

    // Gate: only learn while there is actual signal
    if (blockPower[anchorBand] + blockPower[1] >= 1.0e-7)
    {
        if (! primed)
        {
            bandPower = blockPower;
            primed = true;
        }
        else
        {
            const double alpha = std::exp (-(double) n / (sr * (double) pSpeed->load()));
            for (size_t b = 0; b < (size_t) numBands; ++b)
                bandPower[b] = alpha * bandPower[b] + (1.0 - alpha) * blockPower[b];
        }
    }

    if (! primed) return;

    const float master   = pBalance->load() * 0.01f;
    const float maxCut   = pRange->load();
    const float maxBoost = pMaxBoost->load();
    const float refDb    = (float) (10.0 * std::log10 (bandPower[anchorBand] + 1.0e-12));
    const float g        = (float) std::exp (-(double) n / (sr * 1.0));   // ~1 s glide

    for (size_t b = 0; b < (size_t) numBands; ++b)
    {
        const float bandDb = (float) (10.0 * std::log10 (bandPower[b] + 1.0e-12));
        const float rel    = bandDb - refDb;
        measuredRelDb[b].store (rel);

        float target = 0.0f;
        if ((int) b != anchorBand)
        {
            const float error  = genre.targetRelDb[b] - rel;   // positive = band too quiet
            const float amount = master * pBandBal[b]->load() * 0.01f;

            if (error >= 0.0f)
            {
                // Boost: capped by Max Boost, and faded out when the band is FAR below target
                // (that is almost always an intentional roll-off / low-passed master, not a fault)
                const float fade = juce::jlimit (0.0f, 1.0f, (12.0f - error) / 6.0f);
                target = juce::jmin (error * amount * fade, maxBoost);
            }
            else
            {
                target = juce::jmax (error * amount, -maxCut);
            }
        }

        balanceDb[b]  = g * balanceDb[b] + (1.0f - g) * target;
        balanceLin[b] = juce::Decibels::decibelsToGain (balanceDb[b]);
        appliedGainDb[b].store (balanceDb[b]);

        // Adaptive threshold: tracks the band's own long-term level
        thrDb[b] = bandDb + genre.thresholdOffsetDb;
    }
}

//==============================================================================
void MasterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    const int n   = buffer.getNumSamples();
    const int nch = juce::jmin (buffer.getNumChannels(), 2);
    if (n == 0 || nch == 0) return;

    if (pBypass->load() > 0.5f)
        return;

    const auto& genre = kGenres[(size_t) juce::jlimit (0, (int) kGenres.size() - 1, (int) pGenre->load())];

    const float slope = 1.0f - 1.0f / genre.ratio;

    std::array<float, numBands> dynAmt;
    for (size_t b = 0; b < (size_t) numBands; ++b)
        dynAmt[b] = pDynamics->load() * 0.01f * pBandDyn[b]->load() * 0.01f;
    const bool compOn = primed;

    std::array<float, numBands> atkCoef, relCoef;
    for (size_t b = 0; b < (size_t) numBands; ++b)
    {
        atkCoef[b] = (float) std::exp (-1.0 / (sr * 0.001 * (double) (baseAttackMs[b]  * genre.attackScale)));
        relCoef[b] = (float) std::exp (-1.0 / (sr * 0.001 * (double) (baseReleaseMs[b] * genre.releaseScale)));
    }
    const float detCoef = (float) std::exp (-1.0 / (sr * 0.010));   // ~10 ms RMS-style detector

    float* L = buffer.getWritePointer (0);
    float* R = buffer.getWritePointer (nch > 1 ? 1 : 0);

    std::array<double, numBands> acc {};

    for (int i = 0; i < n; ++i)
    {
        double bl[numBands], br[numBands];
        splitter[0].process ((double) L[i], bl);
        splitter[1].process ((double) R[i], br);

        double outL = 0.0, outR = 0.0;

        for (size_t b = 0; b < (size_t) numBands; ++b)
        {
            const double pl = bl[b] * bl[b], pr = br[b] * br[b];
            acc[b] += pl + pr;

            float gain = balanceLin[b];

            if (compOn)
            {
                // Linked-stereo detector
                const float p = (float) juce::jmax (pl, pr);
                detMs[b] = detCoef * detMs[b] + (1.0f - detCoef) * p;

                const float lvl  = 10.0f * std::log10 (detMs[b] + 1.0e-12f);
                const float over = lvl - thrDb[b];

                float red = 0.0f;
                if (over > kneeDb * 0.5f)
                    red = over * slope;
                else if (over > -kneeDb * 0.5f)
                {
                    const float t = over + kneeDb * 0.5f;
                    red = slope * t * t / (2.0f * kneeDb);
                }
                red = juce::jmin (red, maxReduceDb) * dynAmt[b];

                const float c = red > grDb[b] ? atkCoef[b] : relCoef[b];
                grDb[b] = c * grDb[b] + (1.0f - c) * red;

                gain *= juce::Decibels::decibelsToGain (-grDb[b]);
            }

            outL += bl[b] * (double) gain;
            outR += br[b] * (double) gain;
        }

        L[i] = (float) outL;
        R[i] = (float) outR;
    }

    for (size_t b = 0; b < (size_t) numBands; ++b)
        compGrDb[b].store (grDb[b]);

    // Learn from this block; results are used from the next block on
    updateAnalysis (acc, n, nch, genre);

    // Output trim (ramped across the block to avoid zipper noise)
    const float outGain = juce::Decibels::decibelsToGain (pOutput->load());
    if (prevOutGain < 0.0f) prevOutGain = outGain;
    buffer.applyGainRamp (0, n, prevOutGain, outGain);
    prevOutGain = outGain;
}

//==============================================================================
juce::AudioProcessorEditor* MasterProcessor::createEditor() { return new MasterEditor (*this); }

void MasterProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void MasterProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MasterProcessor(); }
