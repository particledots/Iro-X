#include "PluginEditor.h"

namespace
{
const juce::Colour kBg (0xff15171c), kPanel (0xff1e2128), kText (0xffe8eaf0), kDim (0xff8a93a5);
const juce::Colour kColours[5] = { juce::Colour (0xffe8756a), juce::Colour (0xffe9a23b), juce::Colour (0xff5fbf7f),
                                   juce::Colour (0xff5b9bd5), juce::Colour (0xff9b7fd4) };
// The look of the faders, the thin Mod / Rnd bars, the level sliders and the Randomise button.
struct IroxLookAndFeel : juce::LookAndFeel_V4
{
    IroxLookAndFeel()
    {
        setColour (juce::ComboBox::backgroundColourId, juce::Colour (0xff262a33));
        setColour (juce::ComboBox::outlineColourId, juce::Colour (0xff3a404c));
        setColour (juce::ComboBox::textColourId, juce::Colour (0xffe8eaf0));
        setColour (juce::ComboBox::arrowColourId, juce::Colour (0xff8a93a5));
        setColour (juce::PopupMenu::backgroundColourId, juce::Colour (0xff1e2128));
        setColour (juce::PopupMenu::textColourId, juce::Colour (0xffe8eaf0));
        setColour (juce::PopupMenu::highlightedBackgroundColourId, juce::Colour (0xff3a404c));
        setColour (juce::TextButton::textColourOffId, juce::Colour (0xff15171c));
        setColour (juce::TextButton::textColourOnId, juce::Colour (0xff15171c));
    }

    void drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle style, juce::Slider& s) override
    {
        const auto colour = s.findColour (juce::Slider::thumbColourId);
        if (style == juce::Slider::LinearBar)                      // the thin Mod / Rnd amounts
        {
            const auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
            g.setColour (juce::Colour (0xff2c313b));
            g.fillRoundedRectangle (r, 3.0f);
            g.setColour (colour.withAlpha (0.55f));
            g.fillRoundedRectangle (r.withWidth (juce::jlimit (0.0f, (float) w, pos - (float) x)), 3.0f);
            g.setColour (juce::Colour (0xffe8eaf0));
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (s.getTextFromValue (s.getValue()), r, juce::Justification::centred);
            return;
        }
        if (style == juce::Slider::LinearVertical)                 // the main faders
        {
            const float cx = (float) x + (float) w * 0.5f;
            const auto groove = juce::Rectangle<float> (cx - 4.0f, (float) y + 2.0f, 8.0f, (float) h - 4.0f);
            g.setColour (juce::Colour (0xff0f1116));
            g.fillRoundedRectangle (groove.expanded (1.0f), 5.0f);
            g.setColour (juce::Colour (0xff2c313b));
            g.fillRoundedRectangle (groove, 4.0f);
            g.setColour (colour.withAlpha (0.75f));
            g.fillRoundedRectangle (groove.withTop (pos), 4.0f);
            // tick marks at 0, 25, 50, 75, 100 %
            g.setColour (juce::Colour (0xff4a5160));
            for (int k = 0; k <= 4; ++k)
            {
                const float ty = groove.getBottom() - groove.getHeight() * (float) k / 4.0f;
                g.drawLine (cx - 14.0f, ty, cx - 8.0f, ty, 1.0f);
                g.drawLine (cx + 8.0f, ty, cx + 14.0f, ty, 1.0f);
            }
            const auto cap = juce::Rectangle<float> (cx - 13.0f, pos - 7.0f, 26.0f, 14.0f);
            g.setColour (juce::Colour (0xff0f1116));
            g.fillRoundedRectangle (cap.translated (0.0f, 1.5f), 3.5f);
            g.setColour (colour);
            g.fillRoundedRectangle (cap, 3.5f);
            g.setColour (juce::Colour (0xff15171c).withAlpha (0.55f));
            g.fillRect (cap.withHeight (1.6f).withY (cap.getCentreY() - 0.8f).reduced (4.0f, 0.0f));
            return;
        }
        if (style == juce::Slider::LinearHorizontal)               // Input and Output level
        {
            const float cy = (float) y + (float) h * 0.5f;
            const auto groove = juce::Rectangle<float> ((float) x + 2.0f, cy - 3.0f, (float) w - 4.0f, 6.0f);
            g.setColour (juce::Colour (0xff2c313b));
            g.fillRoundedRectangle (groove, 3.0f);
            g.setColour (juce::Colour (0xff8a93a5));
            g.fillRoundedRectangle (groove.withRight (pos), 3.0f);
            g.setColour (juce::Colour (0xffc9ced9));
            g.fillEllipse (pos - 7.0f, cy - 7.0f, 14.0f, 14.0f);
            return;
        }
        juce::LookAndFeel_V4::drawLinearSlider (g, x, y, w, h, pos, minPos, maxPos, style, s);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
        auto c = juce::Colour (0xffe9a23b);
        if (! b.isEnabled()) c = c.withSaturation (0.1f).withAlpha (0.4f);
        else if (down) c = c.darker (0.25f);
        else if (over) c = c.brighter (0.15f);
        g.setColour (c);
        g.fillRoundedRectangle (r, 6.0f);
    }
    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::FontOptions (15.0f, juce::Font::bold); }
};
IroxLookAndFeel& lnf() { static IroxLookAndFeel l; return l; }

const char* kTitles[5] = { "Filter + VCA", "Resonator", "Echo", "Ambience", "Modulation" };
}

struct IroxEditor::Fader
{
    juce::Slider main, mod, rnd;
    juce::Label name;
};

struct IroxEditor::Section
{
    int index = 0;
    std::vector<std::unique_ptr<Fader>> faders;
    juce::ComboBox modeBox, posBox;     // Filter section only
    juce::Label modeLabel, posLabel;
};

void IroxEditor::setupSlider (juce::Slider& s, const juce::String& id, juce::Slider::SliderStyle style, juce::Colour c)
{
    s.setSliderStyle (style);
    s.setLookAndFeel (&lnf());
    s.setColour (juce::Slider::thumbColourId, c);
    s.setColour (juce::Slider::trackColourId, c.withAlpha (0.55f));
    s.setColour (juce::Slider::backgroundColourId, juce::Colour (0xff2c313b));
    s.setColour (juce::Slider::textBoxTextColourId, kText);
    s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    board.addAndMakeVisible (s);
    sliderAtts.push_back (std::make_unique<SliderAtt> (proc.apvts, id, s));
}

void IroxEditor::addFader (int sec, const juce::String& name, const char* id, const char* modId, const char* rndId, bool isType)
{
    auto& S = *sections[(size_t) sec];
    auto f = std::make_unique<Fader>();
    const auto c = kColours[sec];
    setupSlider (f->main, id, juce::Slider::LinearVertical, c);
    f->main.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 62, 18);
    if (isType)
    {
        f->main.textFromValueFunction = [] (double v) { return juce::String (irox_params::modTypeNames()[juce::jlimit (0, 6, (int) std::lround (v * 6.0))]); };
    }
    else
    {
        f->main.textFromValueFunction = [] (double v) { return juce::String ((int) std::lround (v * 100.0)) + " %"; };
    }
    f->main.setNumDecimalPlacesToDisplay (2);
    f->name.setText (name, juce::dontSendNotification);
    f->name.setJustificationType (juce::Justification::centred);
    f->name.setColour (juce::Label::textColourId, kText);
    f->name.setFont (juce::FontOptions (11.0f));
    f->name.setMinimumHorizontalScale (0.7f);
    board.addAndMakeVisible (f->name);
    auto amount = [&] (juce::Slider& s, const char* pid, const char* label)
    {
        setupSlider (s, pid, juce::Slider::LinearBar, c);
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        const juce::String l (label);
        s.textFromValueFunction = [l] (double v) { return l + " " + juce::String ((int) std::lround (v * 100.0)); };
        s.updateText();
        s.setTooltip (l == "Mod" ? "How far the internal modulation moves this control" : "How far the Randomise button moves this control");
    };
    if (modId != nullptr) amount (f->mod, modId, "Mod");
    if (rndId != nullptr) amount (f->rnd, rndId, "Rnd");
    S.faders.push_back (std::move (f));
}

IroxEditor::IroxEditor (IroxProcessor& p) : AudioProcessorEditor (&p), proc (p)
{
    for (int i = 0; i < 5; ++i) { auto s = std::make_unique<Section>(); s->index = i; sections.push_back (std::move (s)); }

    addFader (0, "Level", "filter_level", nullptr, nullptr);
    addFader (0, "Cutoff", "filter_cutoff", "filter_cutoff_mod", "filter_cutoff_rnd");
    addFader (0, "Resonance", "filter_resonance", "filter_resonance_mod", "filter_resonance_rnd");
    addFader (1, "Level", "resonator_level", nullptr, nullptr);
    addFader (1, "Tune", "resonator_tune", "resonator_tune_mod", "resonator_tune_rnd");
    addFader (1, "Feedback", "resonator_feedback", "resonator_feedback_mod", "resonator_feedback_rnd");
    addFader (1, "Dissonance", "resonator_dissonance", nullptr, nullptr);
    addFader (2, "Level", "echo_level", nullptr, nullptr);
    addFader (2, "Repeats", "echo_repeats", "echo_repeats_mod", "echo_repeats_rnd");
    addFader (2, "Density", "echo_density", "echo_density_mod", "echo_density_rnd");
    addFader (2, "Filter", "echo_filter", nullptr, nullptr);
    addFader (3, "Level", "ambience_level", nullptr, nullptr);
    addFader (3, "Decay", "ambience_decay", "ambience_decay_mod", "ambience_decay_rnd");
    addFader (3, "Spacetime", "ambience_spacetime", "ambience_spacetime_mod", "ambience_spacetime_rnd");
    addFader (3, "Auto-pan", "ambience_autopan", nullptr, nullptr);
    addFader (4, "Level", "mod_level", nullptr, "mod_level_rnd");
    addFader (4, "Speed", "mod_speed", nullptr, "mod_speed_rnd");
    addFader (4, "Type", "mod_type", nullptr, nullptr, true);

    auto& F = *sections[0];
    auto setupCombo = [&] (juce::ComboBox& box, juce::Label& label, const juce::String& text, const juce::StringArray& items, const char* id)
    {
        box.addItemList (items, 1);
        board.addAndMakeVisible (box);
        label.setText (text, juce::dontSendNotification);
        label.setColour (juce::Label::textColourId, kDim);
        label.setFont (juce::FontOptions (12.0f));
        board.addAndMakeVisible (label);
        comboAtts.push_back (std::make_unique<ComboAtt> (proc.apvts, id, box));
    };
    setupCombo (F.modeBox, F.modeLabel, "Mode", { "Low pass", "Band pass", "High pass", "Comb" }, "filter_mode");
    setupCombo (F.posBox, F.posLabel, "Position in chain", { "1", "2", "3", "4" }, "filter_position");

    titleLabel.setText ("Iro-x", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions (30.0f, juce::Font::bold));
    versionLabel.setText ("v" + IroxProcessor::versionString(), juce::dontSendNotification);
    versionLabel.setColour (juce::Label::textColourId, kDim);
    versionLabel.setFont (juce::FontOptions (12.0f));
    tempoLabel.setColour (juce::Label::textColourId, kDim);
    tempoLabel.setFont (juce::FontOptions (12.0f));
    tempoLabel.setJustificationType (juce::Justification::centredLeft);
    for (auto* l : { &titleLabel, &versionLabel, &tempoLabel, &inLabel, &outLabel, &sizeLabel, &footer })
    {
        if (l == &titleLabel || l == &inLabel || l == &outLabel || l == &sizeLabel) l->setColour (juce::Label::textColourId, kText);
        board.addAndMakeVisible (*l);
    }
    inLabel.setText ("Input", juce::dontSendNotification);
    outLabel.setText ("Output", juce::dontSendNotification);
    sizeLabel.setText ("Window size", juce::dontSendNotification);
    footer.setText ("Unofficial plugin, not made, approved or supported by Befaco or by Mutable Instruments. Based on the open-source Befaco Iroi "
                    "firmware (GPL-3.0), which contains code adapted from Mutable Instruments (MIT). Do not contact Befaco about this plugin.", juce::dontSendNotification);
    footer.setColour (juce::Label::textColourId, juce::Colour (0xff7d8696));
    footer.setFont (juce::FontOptions (11.0f));
    footer.setJustificationType (juce::Justification::centredLeft);

    randomise.setLookAndFeel (&lnf());
    randomise.setTooltip ("Moves Cutoff, Resonance, Tune, Feedback, Repeats, Density, Decay, Spacetime, Modulation Level and Speed "
                          "by a random amount, up to their Rnd amount. Logic can automate it (parameter Randomise).");
    randomise.onClick = [this] { proc.randomise(); };
    board.addAndMakeVisible (randomise);

    for (auto* s : { &inputLevel, &outputLevel })
    {
        s->setSliderStyle (juce::Slider::LinearHorizontal);
        s->setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
        s->setTextValueSuffix (" dB");
        s->setColour (juce::Slider::textBoxTextColourId, kText);
        s->setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s->setColour (juce::Slider::thumbColourId, juce::Colour (0xffc9ced9));
        board.addAndMakeVisible (*s);
    }
    sliderAtts.push_back (std::make_unique<SliderAtt> (proc.apvts, "input_level", inputLevel));
    sliderAtts.push_back (std::make_unique<SliderAtt> (proc.apvts, "output_level", outputLevel));

    sizeBox.addItemList ({ "75 %", "100 %", "125 %", "150 %" }, 1);
    sizeBox.setSelectedId (2, juce::dontSendNotification);
    sizeBox.onChange = [this]
    {
        static const float scales[] = { 0.75f, 1.0f, 1.25f, 1.5f };
        const float s = scales[juce::jlimit (0, 3, sizeBox.getSelectedId() - 1)];
        setSize (juce::roundToInt (baseWidth * s), juce::roundToInt (baseHeight * s));
    };
    board.addAndMakeVisible (sizeBox);

    addAndMakeVisible (board);
    board.onPaint = [this] (juce::Graphics& g) { paintBoard (g); };
    board.setBounds (0, 0, baseWidth, baseHeight);
    layoutBoard();
    setResizable (true, true);
    setResizeLimits (baseWidth * 3 / 4, baseHeight * 3 / 4, baseWidth * 3 / 2, baseHeight * 3 / 2);
    getConstrainer()->setFixedAspectRatio ((double) baseWidth / baseHeight);
    setSize (baseWidth, baseHeight);
    timerCallback();
    startTimerHz (8);
}

void IroxEditor::timerCallback()
{
    const double bpm = proc.lastHostBpm();
    tempoLabel.setText (bpm > 0.0 ? "Tempo from Logic: " + juce::String (bpm, 1) + " BPM"
                                  : juce::String ("No host tempo: internal clock (60 BPM)"), juce::dontSendNotification);
}

IroxEditor::~IroxEditor()
{
    stopTimer();
    randomise.setLookAndFeel (nullptr);
    for (auto& s : sections) for (auto& f : s->faders) { f->main.setLookAndFeel (nullptr); f->mod.setLookAndFeel (nullptr); f->rnd.setLookAndFeel (nullptr); }
}

void IroxEditor::paintBoard (juce::Graphics& g)
{
    g.fillAll (kBg);
    auto area = juce::Rectangle<int> (0, 0, baseWidth, baseHeight);
    auto cols = area.withTrimmedTop (64).withTrimmedBottom (34).reduced (12, 0);
    const int w = cols.getWidth() / 5;
    for (int i = 0; i < 5; ++i)
    {
        auto r = cols.withX (cols.getX() + i * w).withWidth (w).reduced (5, 0);
        g.setColour (kPanel);
        g.fillRoundedRectangle (r.toFloat(), 8.0f);
        g.setColour (kColours[i]);
        g.fillRoundedRectangle (r.withHeight (5).toFloat(), 2.5f);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText (kTitles[i], r.withTrimmedTop (9).withHeight (24), juce::Justification::centred);
    }
}

void IroxEditor::resized()
{
    board.setTransform (juce::AffineTransform::scale ((float) getWidth() / baseWidth, (float) getHeight() / baseHeight));
}

void IroxEditor::layoutBoard()
{
    const juce::Rectangle<int> board (0, 0, baseWidth, baseHeight);
    auto top = board.withHeight (64).reduced (14, 10);
    titleLabel.setBounds (top.removeFromLeft (84));
    versionLabel.setBounds (top.removeFromLeft (56));
    randomise.setBounds (top.removeFromLeft (130).reduced (0, 4));
    top.removeFromLeft (14);
    tempoLabel.setBounds (top.removeFromLeft (210));
    auto right = top.removeFromRight (112);
    sizeBox.setBounds (right.reduced (0, 6));
    sizeLabel.setBounds (top.removeFromRight (84));
    top.removeFromRight (10);
    outputLevel.setBounds (top.removeFromRight (170));
    outLabel.setBounds (top.removeFromRight (56));
    inputLevel.setBounds (top.removeFromRight (170));
    inLabel.setBounds (top.removeFromRight (48));

    footer.setBounds (board.withTrimmedTop (baseHeight - 34).reduced (16, 1));

    auto cols = board.withTrimmedTop (64).withTrimmedBottom (34).reduced (12, 0);
    const int w = cols.getWidth() / 5;
    for (int i = 0; i < 5; ++i)
    {
        auto& S = *sections[(size_t) i];
        auto r = cols.withX (cols.getX() + i * w).withWidth (w).reduced (11, 0).withTrimmedTop (38).withTrimmedBottom (8);
        if (i == 0)
        {
            auto sel = r.removeFromBottom (92);
            S.modeLabel.setBounds (sel.removeFromTop (16));
            S.modeBox.setBounds (sel.removeFromTop (24));
            S.posLabel.setBounds (sel.removeFromTop (16));
            S.posBox.setBounds (sel.removeFromTop (24));
        }
        const int n = (int) S.faders.size();
        const int fw = r.getWidth() / std::max (n, 1);
        for (int k = 0; k < n; ++k)
        {
            auto& f = *S.faders[(size_t) k];
            auto c = r.withX (r.getX() + k * fw).withWidth (fw).reduced (2, 0);
            f.name.setBounds (c.removeFromTop (18));
            auto amounts = c.removeFromBottom (44);
            f.main.setBounds (c);
            f.mod.setBounds (amounts.removeFromTop (20).reduced (1, 1));
            f.rnd.setBounds (amounts.removeFromTop (20).reduced (1, 1));
        }
    }
}
