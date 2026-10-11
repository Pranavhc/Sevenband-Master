#include "PluginEditor.h"

namespace
{
    const juce::Colour kBlue   { 0xff4aa3ff };
    const juce::Colour kOrange { 0xffffb347 };
    const juce::Colour kRed    { 0xffff5c6c };
}

MasterEditor::MasterEditor (MasterProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    //==========================================================================
    // Global knobs
    const char* ids[]   = { "balance", "range", "maxboost", "dynamics", "speed", "output" };
    const char* names[] = { "Balance All", "Max Cut", "Max Boost", "Dynamics All", "Analysis", "Output" };

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        auto& k = knobs[i];
        k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        k.label.setText (names[i], juce::dontSendNotification);
        k.label.setJustificationType (juce::Justification::centred);
        k.attach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, ids[i], k.slider);
        addAndMakeVisible (k.slider);
        addAndMakeVisible (k.label);
    }

    //==========================================================================
    // Genre + bypass
    for (size_t i = 0; i < kGenres.size(); ++i)
        genreBox.addItem (kGenres[i].name, (int) i + 1);
    genreAttach = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (proc.apvts, "genre", genreBox);
    addAndMakeVisible (genreBox);

    bypassAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (proc.apvts, "bypass", bypassButton);
    addAndMakeVisible (bypassButton);

    //==========================================================================
    // Per-band vertical sliders
    for (size_t b = 0; b < 7; ++b)
    {
        auto setup = [this] (juce::Slider& s, juce::Colour c)
        {
            s.setSliderStyle (juce::Slider::LinearVertical);
            s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 46, 16);
            s.setTextValueSuffix ("%");
            s.setNumDecimalPlacesToDisplay (0);
            s.setColour (juce::Slider::trackColourId, c.withAlpha (0.85f));
            s.setColour (juce::Slider::thumbColourId, c);
            addAndMakeVisible (s);
        };

        setup (balSliders[b], kOrange);
        setup (dynSliders[b], kRed);

        balAttach[b] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "bal" + juce::String ((int) b), balSliders[b]);
        dynAttach[b] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (proc.apvts, "dyn" + juce::String ((int) b), dynSliders[b]);
    }

    // Mid is the reference band: it is never tonally corrected
    balSliders[(size_t) MasterProcessor::anchorBand].setEnabled (false);

    setSize (780, 640);
    startTimerHz (30);
}

void MasterEditor::resized()
{
    auto r = getLocalBounds().reduced (16);

    auto top = r.removeFromTop (30);
    top.removeFromLeft (230);                                   // title is painted here
    genreBox.setBounds (top.removeFromLeft (170));
    top.removeFromLeft (16);
    bypassButton.setBounds (top.removeFromLeft (90));

    r.removeFromTop (10);
    meterArea = r.removeFromTop (210);
    r.removeFromTop (8);
    sliderArea = r.removeFromTop (215);
    r.removeFromTop (8);

    // Vertical sliders sit directly under their band's meter column
    const int colW = meterArea.getWidth() / MasterProcessor::numBands;
    for (int b = 0; b < MasterProcessor::numBands; ++b)
    {
        auto col = juce::Rectangle<int> (meterArea.getX() + b * colW, sliderArea.getY(), colW, sliderArea.getHeight()).reduced (4, 0);
        col.removeFromTop (16);                                  // "BAL" / "DYN" captions
        auto left = col.removeFromLeft (col.getWidth() / 2);
        balSliders[(size_t) b].setBounds (left);
        dynSliders[(size_t) b].setBounds (col);
    }

    // Global knobs
    const int w = r.getWidth() / (int) knobs.size();
    for (auto& k : knobs)
    {
        auto c = r.removeFromLeft (w);
        k.label.setBounds  (c.removeFromTop (18));
        k.slider.setBounds (c);
    }
}

void MasterEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff15171c));
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (20.0f, juce::Font::bold));
    g.drawText ("Seven Band Master", 16, 12, 220, 26, juce::Justification::left);

    //==========================================================================
    // Band meters
    auto area = meterArea.toFloat();
    g.setColour (juce::Colour (0xff1e2128));
    g.fillRoundedRectangle (area, 6.0f);

    const int   n      = MasterProcessor::numBands;
    const float colW   = area.getWidth() / (float) n;
    const float maxDev = 12.0f;
    const float zeroY  = area.getCentreY() - 4.0f;
    const float halfH  = (area.getHeight() - 70.0f) * 0.5f;

    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.drawHorizontalLine ((int) zeroY, area.getX() + 6.0f, area.getRight() - 6.0f);

    const int genreIdx = juce::jlimit (0, (int) kGenres.size() - 1,
                                       (int) *proc.apvts.getRawParameterValue ("genre"));

    for (int b = 0; b < n; ++b)
    {
        const float x = area.getX() + colW * (float) b;

        const float dev     = proc.measuredRelDb[(size_t) b].load() - kGenres[(size_t) genreIdx].targetRelDb[(size_t) b];
        const float applied = proc.appliedGainDb[(size_t) b].load();
        const float gr      = proc.compGrDb[(size_t) b].load();

        auto bar = [&] (float db, int slot, juce::Colour c)
        {
            const float h = juce::jlimit (-1.0f, 1.0f, db / maxDev) * halfH;
            juce::Rectangle<float> rc (x + colW * (0.10f + 0.29f * (float) slot),
                                       h >= 0.0f ? zeroY - h : zeroY,
                                       colW * 0.25f, std::abs (h));
            g.setColour (c);
            g.fillRect (rc);
        };

        bar (dev,     0, kBlue);      // band level vs. target (up = too loud)
        bar (applied, 1, kOrange);    // tonal-balance correction
        bar (-gr,     2, kRed);       // compressor gain reduction (down)

        g.setColour (juce::Colours::white.withAlpha (0.85f));
        g.setFont (13.0f);
        g.drawText (MasterProcessor::bandNames[(size_t) b],
                    (int) x, (int) (area.getBottom() - 40.0f), (int) colW, 16, juce::Justification::centred);
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.setFont (11.0f);
        g.drawText (juce::String (applied, 1) + " / -" + juce::String (gr, 1),
                    (int) x, (int) (area.getBottom() - 24.0f), (int) colW, 16, juce::Justification::centred);
    }

    g.setFont (11.0f);
    g.setColour (kBlue);
    g.drawText ("Level vs target", (int) area.getX() + 10, (int) area.getY() + 5, 110, 14, juce::Justification::left);
    g.setColour (kOrange);
    g.drawText ("Balance (dB)", (int) area.getX() + 125, (int) area.getY() + 5, 110, 14, juce::Justification::left);
    g.setColour (kRed);
    g.drawText ("Compression GR (dB)", (int) area.getX() + 235, (int) area.getY() + 5, 140, 14, juce::Justification::left);

    //==========================================================================
    // Captions above the vertical sliders
    auto sa = sliderArea.toFloat();
    g.setColour (juce::Colour (0xff1e2128));
    g.fillRoundedRectangle (sa, 6.0f);

    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    for (int b = 0; b < n; ++b)
    {
        auto col = juce::Rectangle<float> (area.getX() + colW * (float) b, sa.getY() + 2.0f, colW, 14.0f).reduced (4.0f, 0.0f);
        auto left = col.removeFromLeft (col.getWidth() * 0.5f);

        const bool anchor = (b == MasterProcessor::anchorBand);
        g.setColour (anchor ? juce::Colours::white.withAlpha (0.2f) : kOrange);
        g.drawText ("BAL", left.toNearestInt(), juce::Justification::centred);
        g.setColour (kRed);
        g.drawText ("DYN", col.toNearestInt(), juce::Justification::centred);
    }
}
