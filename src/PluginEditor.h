#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"
#include <memory>
#include <vector>

// The window: five coloured sections as in the approved mock-up, faders with their Mod and Rnd amounts, the Randomise
// button, Input and Output level, window size, and a line showing the tempo Logic gives the plugin.
class IroxEditor : public juce::AudioProcessorEditor,
                   private juce::Timer
{
public:
    explicit IroxEditor (IroxProcessor&);
    ~IroxEditor() override;
    void resized() override;

    static constexpr int baseWidth = 1180, baseHeight = 560;

private:
    struct Fader;
    struct Section;
    using SliderAtt = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboAtt  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void setupSlider (juce::Slider&, const juce::String& paramId, juce::Slider::SliderStyle, juce::Colour);
    void addFader (int section, const juce::String& name, const char* id, const char* modId, const char* rndId, bool isType = false);
    void paintBoard (juce::Graphics&);
    void layoutBoard();
    void timerCallback() override;

    // The whole window content is drawn on a fixed 1180 x 560 board that is scaled to the window size.
    struct Board : juce::Component
    {
        std::function<void (juce::Graphics&)> onPaint;
        void paint (juce::Graphics& g) override { if (onPaint) onPaint (g); }
    } board;

    IroxProcessor& proc;
    std::vector<std::unique_ptr<Section>> sections;

    juce::Label titleLabel, versionLabel, tempoLabel, inLabel, outLabel, sizeLabel, footer;
    juce::TextButton randomise { "Randomise" };
    juce::Slider inputLevel, outputLevel;
    juce::ComboBox sizeBox;
    // declared last so they are destroyed first, before the controls they are attached to
    std::vector<std::unique_ptr<SliderAtt>> sliderAtts;
    std::vector<std::unique_ptr<ComboAtt>> comboAtts;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (IroxEditor)
};
