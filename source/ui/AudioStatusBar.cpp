#include "AudioStatusBar.h"
#include "../PluginProcessor.h"
#include "LookAndFeel.h"

//==============================================================================
AudioStatusBar::AudioStatusBar (PedalForgeProcessor& proc)
    : processor (proc)
{
    masterSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    masterSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    masterSlider.setRange (0.0, 2.0, 0.001);   // 0..unity..+6dB
    masterSlider.setSkewFactor (0.7);          // weight detail around unity
    masterSlider.setValue (processor.masterVolume.load(), juce::dontSendNotification);
    masterSlider.setTooltip ("Master output volume (0 = silent, 1 = unity, 2 = +6 dB).");
    masterSlider.onValueChange = [this]
    {
        processor.masterVolume.store ((float) masterSlider.getValue(), std::memory_order_relaxed);
    };
    addAndMakeVisible (masterSlider);

    muteBtn.setClickingTogglesState (true);
    muteBtn.setToggleState (processor.masterMute.load(), juce::dontSendNotification);
    muteBtn.setTooltip ("Mute the main output.");
    muteBtn.setColour (juce::TextButton::buttonColourId, PedalForgeLookAndFeel::bgLight);
    muteBtn.setColour (juce::TextButton::buttonOnColourId, juce::Colour (0xFFDC2626));
    muteBtn.setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
    muteBtn.onClick = [this]
    {
        processor.masterMute.store (muteBtn.getToggleState(), std::memory_order_relaxed);
    };
    addAndMakeVisible (muteBtn);

    settingsBtn.setTooltip ("Open audio device settings…");
    settingsBtn.onClick = [this] { openAudioSettings(); };
    addAndMakeVisible (settingsBtn);

    startTimerHz (30);
}

AudioStatusBar::~AudioStatusBar() { stopTimer(); }

//==============================================================================
void AudioStatusBar::resized()
{
    auto b = getLocalBounds().reduced (8, 4);
    const int meterW = 84;
    const int sliderW = juce::jmin (180, b.getWidth() / 4);
    const int btnW = 28;
    const int muteW = 56;
    const int gap = 8;

    inputMeterArea  = b.removeFromLeft (meterW);
    b.removeFromLeft (gap);

    // Device label fills middle remaining space before the right-side widgets.
    auto rightCluster = b.removeFromRight (meterW + gap + btnW + gap + muteW + gap + sliderW + gap);
    outputMeterArea = rightCluster.removeFromRight (meterW);
    rightCluster.removeFromRight (gap);
    settingsBtn.setBounds (rightCluster.removeFromRight (btnW));
    rightCluster.removeFromRight (gap);
    muteBtn.setBounds (rightCluster.removeFromRight (muteW));
    rightCluster.removeFromRight (gap);
    masterSlider.setBounds (rightCluster.removeFromRight (sliderW));
}

//==============================================================================
void AudioStatusBar::timerCallback()
{
    // Read & smooth the input/output peaks.
    auto smooth = [] (float& cur, float target, float attack, float release)
    {
        if (target > cur) cur = cur + (target - cur) * attack;
        else              cur = cur + (target - cur) * release;
    };
    for (int ch = 0; ch < 2; ++ch)
    {
        smooth (displayedInputPeak[ch],  processor.inputPeak[ch].load(),  0.6f, 0.18f);
        smooth (displayedOutputPeak[ch], processor.outputPeak[ch].load(), 0.6f, 0.18f);
    }

    // Device label is updated externally via setDeviceLabel() from
    // PluginEditor (which has access to the standalone holder when
    // running standalone). Falls back to "" if nothing's pushed.
    repaint (inputMeterArea);
    repaint (outputMeterArea);
}

//==============================================================================
void AudioStatusBar::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xFF111418));
    g.setColour (juce::Colour (0xFF26303B));
    g.drawLine (0.0f, 0.0f, (float) getWidth(), 0.0f, 1.0f);

    auto drawMeterPair = [&] (juce::Rectangle<int> area, const float (&peaks)[2], const juce::String& tag)
    {
        const int chH = (area.getHeight() - 4) / 2;
        for (int ch = 0; ch < 2; ++ch)
        {
            const juce::Rectangle<int> chArea (area.getX(), area.getY() + 2 + ch * (chH + 0),
                                                area.getWidth(), chH - 1);
            g.setColour (juce::Colour (0xFF0B0D11));
            g.fillRect (chArea);

            const float p = juce::jlimit (0.0f, 1.0f, peaks[ch]);
            const int filled = (int) (chArea.getWidth() * p);
            // Three-zone colour: green → yellow → red
            for (int x = 0; x < filled; ++x)
            {
                const float frac = (float) x / (float) chArea.getWidth();
                juce::Colour c;
                if      (frac < 0.6f) c = juce::Colour (0xFF22C55E);
                else if (frac < 0.85f) c = juce::Colour (0xFFFBBF24);
                else                   c = juce::Colour (0xFFEF4444);
                g.setColour (c);
                g.fillRect (chArea.getX() + x, chArea.getY(), 1, chArea.getHeight());
            }
        }
        g.setColour (juce::Colour (0xFF6B7280));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText (tag, area.expanded (0, 2), juce::Justification::centredTop, false);
    };

    drawMeterPair (inputMeterArea,  displayedInputPeak,  "IN");
    drawMeterPair (outputMeterArea, displayedOutputPeak, "OUT");

    // Device label in the middle
    auto b = getLocalBounds().reduced (8, 4);
    b.removeFromLeft (inputMeterArea.getWidth() + 16);
    b.removeFromRight (outputMeterArea.getWidth() + 16 + 28 + 8 + 56 + 8 + 180 + 8);
    g.setColour (juce::Colour (0xFF9CA3AF));
    g.setFont (juce::FontOptions (12.0f));
    g.drawText (deviceLabel, b, juce::Justification::centredLeft, true);
}

//==============================================================================
void AudioStatusBar::mouseDown (const juce::MouseEvent& e)
{
    if (! e.mods.isRightButtonDown() && ! e.mods.isCtrlDown())
        return;
    if (inputMeterArea.contains (e.getPosition()))   openInputGainMenu();
    else if (outputMeterArea.contains (e.getPosition())) openOutputGainMenu();
}

void AudioStatusBar::openInputGainMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Input channel gain (software trim)");
    for (int ch = 0; ch < 2; ++ch)
    {
        const float g = processor.inputGain[ch].load();
        m.addItem ("Ch " + juce::String (ch + 1)
                   + "  =  " + juce::String (juce::Decibels::gainToDecibels (g), 1) + " dB"
                   + (std::abs (g - 1.0f) < 1e-3f ? "" : "  ●"),
                   false, false, [] {});
        for (auto db : { -12, -6, -3, 0, +3, +6, +12 })
        {
            m.addItem (juce::String (db) + " dB",
                       true, std::abs (juce::Decibels::gainToDecibels (g) - (float) db) < 0.5f,
                       [this, ch, db] {
                           processor.inputGain[ch].store (juce::Decibels::decibelsToGain ((float) db),
                                                          std::memory_order_relaxed);
                       });
        }
        m.addSeparator();
    }
    m.addItem ("Reset all input channels to 0 dB", [this] {
        for (int ch = 0; ch < PedalForgeProcessor::kMaxChannels; ++ch)
            processor.inputGain[ch].store (1.0f, std::memory_order_relaxed);
    });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
}

void AudioStatusBar::openOutputGainMenu()
{
    juce::PopupMenu m;
    m.addSectionHeader ("Output channel gain (software trim)");
    for (int ch = 0; ch < 2; ++ch)
    {
        const float g = processor.outputGain[ch].load();
        m.addItem ("Ch " + juce::String (ch + 1)
                   + "  =  " + juce::String (juce::Decibels::gainToDecibels (g), 1) + " dB"
                   + (std::abs (g - 1.0f) < 1e-3f ? "" : "  ●"),
                   false, false, [] {});
        for (auto db : { -12, -6, -3, 0, +3, +6, +12 })
        {
            m.addItem (juce::String (db) + " dB",
                       true, std::abs (juce::Decibels::gainToDecibels (g) - (float) db) < 0.5f,
                       [this, ch, db] {
                           processor.outputGain[ch].store (juce::Decibels::decibelsToGain ((float) db),
                                                           std::memory_order_relaxed);
                       });
        }
        m.addSeparator();
    }
    m.addItem ("Reset all output channels to 0 dB", [this] {
        for (int ch = 0; ch < PedalForgeProcessor::kMaxChannels; ++ch)
            processor.outputGain[ch].store (1.0f, std::memory_order_relaxed);
    });
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this));
}

void AudioStatusBar::openAudioSettings()
{
   #if JucePlugin_Build_Standalone
    extern void OpenStandaloneAudioSettingsDialog();
    OpenStandaloneAudioSettingsDialog();
   #endif
}

void AudioStatusBar::setDeviceLabel (const juce::String& label)
{
    if (label != deviceLabel)
    {
        deviceLabel = label;
        repaint();
    }
}
